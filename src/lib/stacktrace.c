/*
 * $Id$
 *
 * Copyright (c) 2004, 2010, Raphael Manfredi
 *
 *----------------------------------------------------------------------
 * This file is part of gtk-gnutella.
 *
 *  gtk-gnutella is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  gtk-gnutella is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with gtk-gnutella; if not, write to the Free Software
 *  Foundation, Inc.:
 *      59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *----------------------------------------------------------------------
 */

/**
 * @ingroup lib
 * @file
 *
 * Stack unwinding support.
 *
 * This file is using raw malloc(), free(), strdup(), etc... because it can
 * be exercised by the debugging malloc layer, at a very low level and we
 * must not interfere.  Don't even think about using g_malloc() and friends or
 * any other glib memory-allocating routine here.
 *
 * This means this file cannot be the target of leak detection by our
 * debugging malloc layer.
 *
 * @author Raphael Manfredi
 * @date 2004, 2010
 */

#include "common.h"		/* For RCSID */

RCSID("$Id$")

#include "stacktrace.h"
#include "atoms.h"		/* For binary_hash() */
#include "ascii.h"
#include "concat.h"
#include "glib-missing.h"
#include "misc.h"
#include "omalloc.h"
#include "parse.h"
#include "path.h"
#include "unsigned.h"
#include "vmm.h"

/* We need hash_table_new_real() to avoid any call to g_malloc() */
#define MALLOC_SOURCE
#include "hashtable.h"
#undef MALLOC_SOURCE

#include "override.h"	/* Must be the last header included */

/*
 * Ensure we use the raw allocation routines even when compiled
 * with -DTRACK_MALLOC.
 */
#undef malloc
#undef free
#undef strdup

/**
 * A routine entry in the symbol table.
 */
struct trace {
	const void *start;			/**< Start PC address */
	char *name;					/**< Routine name */
};

/**
 * The array of trace entries.
 */
static struct {
	struct trace *base;			/**< Array base */
	size_t size;				/**< Amount of entries allocated */
	size_t count;				/**< Amount of entries held */
} trace_array;

/**
 * Deferred loading support.
 */
static char *program_path;
static time_t program_mtime;
static gboolean symbols_loaded;

/**
 * "nm" output parsing context.
 */
struct nm_parser {
	hash_table_t *atoms;		/**< To create string "atoms" */
};

static hash_table_t *stack_atoms;

static void *getreturnaddr(size_t level);
static void *getframeaddr(size_t level);

/**
 * Search executable within the user's PATH.
 *
 * @return full path if found, NULL otherwise.
 */
static char *
locate_from_path(const char *argv0)
{
	char *path;
	char *tok;
	char filepath[MAX_PATH_LEN + 1];
	char *result = NULL;

	if (filepath_basename(argv0) != argv0) {
		g_warning("can't locate \"%s\" in PATH: name contains '%c' already",
			argv0, G_DIR_SEPARATOR);
		return NULL;
	}

	path = getenv("PATH");
	if (NULL == path) {
		g_warning("can't locate \"%s\" in PATH: no such environment variable",
			argv0);
		return NULL;
	}

	path = strdup(path);

	for (tok = strtok(path, ":"); tok; tok = strtok(NULL, ":")) {
		const char *dir = tok;
		struct stat buf;

		if ('\0' == *dir)
			dir = ".";
		concat_strings(filepath, sizeof filepath,
			dir, G_DIR_SEPARATOR_S, argv0, NULL);

		if (-1 != stat(filepath, &buf)) {
			if (S_ISREG(buf.st_mode) && -1 != access(filepath, X_OK)) {
				result = strdup(filepath);
				break;
			}
		}
	}

	free(path);
	return result;
}

/**
 * Compare two trace entries -- qsort() callback.
 */
static int
trace_cmp(const void *p, const void *q)
{
	struct trace const *a = p;
	struct trace const *b = q;

	return a->start == b->start ? 0 :
		pointer_to_ulong(a->start) < pointer_to_ulong(b->start) ? -1 : +1;
}

/**
 * Remove duplicate entry in trace array at the specified index.
 */
static void
trace_remove(size_t i)
{
	struct trace *t;

	g_assert(size_is_non_negative(i));
	g_assert(i < trace_array.count);

	t = &trace_array.base[i];
	free(t->name);
	if (i < trace_array.count - 1)
		memmove(t, t + 1, trace_array.count - i - 1);
	trace_array.count--;
}

/**
 * Sort trace array, remove duplicate entries.
 */
static void
trace_sort(void)
{
	size_t i = 0;
	size_t old_count = trace_array.count;
	const void *last = 0;

	qsort(trace_array.base, trace_array.count,
		sizeof trace_array.base[0], trace_cmp);

	while (i < trace_array.count) {
		struct trace *t = &trace_array.base[i];
		if (last && t->start == last) {
			trace_remove(i);
		} else {
			last = t->start;
			i++;
		}
	}

	if (old_count != trace_array.count) {
		size_t delta = old_count - trace_array.count;
		g_assert(size_is_non_negative(delta));
		g_warning("stripped %lu duplicate symbol%s",
			(unsigned long) delta, 1 == delta ? "" : "s");
	}
}

/**
 * Insert new trace symbol.
 */
static void
trace_insert(const void *start, const char *name)
{
	struct trace *t;

	if (trace_array.count >= trace_array.size) {
		size_t old_size, new_size;
		void *old_base;

		old_base = trace_array.base;
		old_size = trace_array.size * sizeof *t;
		trace_array.size += 1024;
		new_size = trace_array.size * sizeof *t;

		trace_array.base = vmm_alloc_not_leaking(trace_array.size * sizeof *t);
		if (old_base != NULL) {
			memcpy(trace_array.base, old_base, old_size);
			vmm_free(old_base, old_size);
		}
	}

	t = &trace_array.base[trace_array.count++];
	t->start = start;
	t->name = strdup(name);
}

/**
 * Lookup trace structure encompassing given program counter.
 *
 * @return trace structure if found, NULL otherwise.
 */
static struct trace *
trace_lookup(void *pc)
{
	struct trace *low = trace_array.base,
				 *high = &trace_array.base[trace_array.count -1],
				 *mid;

	while (low <= high) {
		mid = low + (high - low) / 2;
		if (pc >= mid->start && (mid == high || pc < (mid+1)->start))
			return mid;			/* Found it! */
		else if (pc < mid->start)
			high = mid - 1;
		else
			low = mid + 1;
	}

	return NULL;				/* Not found */
}

/*
 * @eturn symbolic name for given pc offset, if found, otherwise
 * the hexadecimal value.
 */
static const char *
trace_name(void *pc)
{
	static char buf[256];

	if (0 == trace_array.count) {
		gm_snprintf(buf, sizeof buf, "0x%lx", pointer_to_ulong(pc));
	} else {
		struct trace *t;

		t = trace_lookup(pc);

		if (NULL == t || &trace_array.base[trace_array.count -1] == t) {
			gm_snprintf(buf, sizeof buf, "0x%lx", pointer_to_ulong(pc));
		} else {
			gm_snprintf(buf, sizeof buf, "%s+%u", t->name,
				(unsigned) ptr_diff(pc, t->start));
		}
	}

	return buf;
}

/**
 * Return atom string for the trace name.
 * This memory will never be freed.
 */
static const char *
trace_atom(struct nm_parser *ctx, const char *name)
{
	const char *result;

	result = hash_table_lookup(ctx->atoms, name);

	if (NULL == result) {
		result = ostrdup(name);		/* Never freed */
		hash_table_insert(ctx->atoms, result, result);
	}

	return result;
}

/**
 * Parse the nm output line, recording symbol mapping for function entries.
 *
 * We're looking for lines like:
 *
 *	082bec77 T zget
 *	082be9d3 t zn_create
 */
static void
parse_nm(struct nm_parser *ctx, char *line)
{
	int error;
	const char *ep;
	char *p = line;
	const void *addr;

	addr = parse_pointer(p, &ep, &error);
	if (error || NULL == addr)
		return;

	p = skip_ascii_blanks(ep);

	if ('t' == ascii_tolower(*p)) {
		p = skip_ascii_blanks(&p[1]);
		str_chomp(p, 0);
		trace_insert(addr, trace_atom(ctx, p));
	}
}

static size_t
str_hash(const void *p)
{
	return g_str_hash(p);
}

/**
 * Load symbols from the executable we're running.
 */
static void
load_symbols(const char *path)
{
	char tmp[MAX_PATH_LEN + 80];
	size_t rw;
	FILE *f;
	struct nm_parser nm_ctx;

	rw = gm_snprintf(tmp, sizeof tmp, "nm -p %s", path);
	if (rw != strlen(path) + CONST_STRLEN("nm -p ")) {
		g_warning("full path \"%s\" too long, cannot load symbols", path);
		goto done;
	}

	f = popen(tmp, "r");

	if (NULL == f) {
		g_warning("can't run \"%s\": %s", tmp, g_strerror(errno));
		goto done;
	}

	nm_ctx.atoms = hash_table_new_full_real(str_hash, g_str_equal);

	while (fgets(tmp, sizeof tmp, f)) {
		parse_nm(&nm_ctx, tmp);
	}

	pclose(f);
	hash_table_destroy_real(nm_ctx.atoms);

done:
	g_info("loaded %u symbols from \"%s\"",
		(unsigned) trace_array.count, path);

	trace_sort();
}

/**
 * Get the full program path.
 *
 * @return a newly allocated string (through malloc()) that points to the
 * path of the program being run, NULL if we can't compute a suitable path.
 */
static char *
program_path_allocate(const char *argv0)
{
	struct stat buf;
	const char *file = argv0;

	if (-1 == stat(argv0, &buf)) {
		file = locate_from_path(argv0);
		if (NULL == file) {
			g_warning("cannot find \"%s\" in PATH, not loading symbols", argv0);
			goto error;
		}
	}

	/*
	 * Make sure there are no problematic shell meta-characters in the path.
	 */

	{
		const char meta[] = "$&`:;()<>|";
		const char *p = file;
		int c;

		while ((c = *p++)) {
			if (strchr(meta, c)) {
				g_warning("found shell meta-character '%c' in path \"%s\", "
					"not loading symbols", c, file);
				goto error;
			}
		}
	}

	if (file != NULL && file != argv0)
		return deconstify_gpointer(file);

	return strdup(argv0);

error:
	if (file != NULL && file != argv0)
		free(deconstify_gpointer(file));

	return NULL;
}

/**
 * Initialize stack tracing.
 *
 * @param argv0		the value of argv[0], from main(): the program's filename
 * @param deferred	if TRUE, do not load symbols until it's needed
 */
void
stacktrace_init(const char *argv0, gboolean deferred)
{
	g_assert(argv0 != NULL);

	program_path = program_path_allocate(argv0);

	if (NULL == program_path)
		goto done;

	if (deferred) {
		struct stat buf;

		if (-1 == stat(program_path, &buf)) {
			g_warning("cannot stat \"%s\": %s",
				program_path, g_strerror(errno));
			g_warning("will not be loading symbols for %s", argv0);
			goto done;
		}

		program_mtime = buf.st_mtime;
		return;
	}

	load_symbols(program_path);

	/* FALL THROUGH */

done:
	if (program_path != NULL) {
		free(program_path);
		program_path = NULL;
	}
	symbols_loaded = TRUE;		/* Don't attempt again */
}

/**
 * Close stack tracing.
 */
void
stacktrace_close(void)
{
	if (program_path != NULL) {
		free(program_path);
		program_path = NULL;
	}
	if (trace_array.base != NULL) {
		vmm_free(trace_array.base,
			trace_array.size * sizeof trace_array.base[0]);
		trace_array.base = NULL;
	}
	if (stack_atoms != NULL) {
		hash_table_destroy_real(stack_atoms);	/* Does not free keys/values */
		stack_atoms = NULL;
	}
}

/**
 * Load symbols if not done already.
 */
static void
stacktrace_load_symbols(void)
{
	if (symbols_loaded)
		return;

	symbols_loaded = TRUE;		/* Whatever happens, don't try again */

	/*
	 * Loading of symbols was deferred: make sure the executable is still
	 * there and has not been tampered with since we started.
	 */

	if (program_path != NULL) {
		struct stat buf;

		if (-1 == stat(program_path, &buf)) {
			g_warning("cannot stat \"%s\": %s",
				program_path, g_strerror(errno));
			goto error;
		}

		if (buf.st_mtime != program_mtime) {
			g_warning("executable file \"%s\" has been tampered with",
				program_path);
			goto error;
		}

		load_symbols(program_path);
	}

	goto done;

error:
	if (program_path != NULL) {
		g_warning("cannot load symbols for %s", program_path);
	}

	/* FALL THROUGH */

done:
	if (program_path != NULL)
		free(program_path);
	program_path = NULL;
}

/**
 * Post-init operations.
 */
void
stacktrace_post_init(void)
{
#ifdef MALLOC_FRAMES
	/*
	 * When we keep around allocation frames (to be able to report memory
	 * leaks later), it is best to load symbols immediately in case the
	 * program is changed (moved around) during the execution and we find out
	 * we cannot load the symbols later at exit time, when we have leaks to
	 * report and cannot map the PC addresses to functions.
	 */

	stacktrace_load_symbols();
#endif
}

/**
 * Unwind current stack into supplied stacktrace array.
 *
 * @param stack		array where stack should be written
 * @param count		amount of items in stack[]
 * @param offset	amount of immediate callers to remove (ourselves excluded)
 *
 * @return the amount of entries filled in stack[].
 */
static size_t
stack_unwind(void *stack[], size_t count, size_t offset)
{
    size_t i;

    for (i = offset; getframeaddr(i + 1) != NULL && i - offset < count; i++) {
        if (NULL == (stack[i - offset] = getreturnaddr(i)))
			break;
    }

	return i - offset;
}


/**
 * Fill supplied stacktrace structure with the backtrace.
 * Trace will start with our caller.
 */
void
stacktrace_get(struct stacktrace *st)
{
	st->len = stack_unwind(st->stack, G_N_ELEMENTS(st->stack), 1);
}

/**
 * Fill supplied stacktrace structure with the backtrace, removing ``offset''
 * amount of immediate callers (0 will make our caller be the first item).
 */
void
stacktrace_get_offset(struct stacktrace *st, size_t offset)
{
	st->len = stack_unwind(st->stack, G_N_ELEMENTS(st->stack), offset + 1);
}

/**
 * Print array of PCs, using symbolic names if possible.
 *
 * @param f			where to print the stack
 * @param stack		array of Program Counters making up the stack
 * @param count		number of items in stack[] to print, at most.
 */
static void
stack_print(FILE *f, void * const *stack, size_t count)
{
	size_t i;

	stacktrace_load_symbols();

	for (i = 0; i < count; i++) {
		const char *where = trace_name(stack[i]);
		fprintf(f, "\t%s\n", where);

		/* Stop as soon as we reach main() before backtracing into libc */
		if (is_strprefix(where, "main+"))	/* HACK ALERT */
			break;
	}
}

/**
 * Print stack trace to specified file, using symbolic names if possible.
 */
void
stacktrace_print(FILE *f, const struct stacktrace *st)
{
	g_assert(st != NULL);

	stack_print(f, st->stack, st->len);
}

/**
 * Print stack trace atom to specified file, using symbolic names if possible.
 */
void
stacktrace_atom_print(FILE *f, const struct stackatom *st)
{
	g_assert(st != NULL);

	stack_print(f, st->stack, st->len);
}

/**
 * Print current stack trace to specified file.
 */
void
stacktrace_where_print(FILE *f)
{
	void *stack[STACKTRACE_DEPTH_MAX];
	size_t count;

	count = stack_unwind(stack, G_N_ELEMENTS(stack), 1);
	stack_print(f, stack, count);
}

/**
 * Print current stack trace to specified file, with specified offset.
 *
 * @param offset	amount of immediate callers to remove (ourselves excluded)
 */
void
stacktrace_where_print_offset(FILE *f, size_t offset)
{
	void *stack[STACKTRACE_DEPTH_MAX];
	size_t count;

	count = stack_unwind(stack, G_N_ELEMENTS(stack), offset + 1);
	stack_print(f, stack, count);
}

/**
 * Hashing routine for a "struct stacktracea".
 */
size_t
stack_hash(const void *key)
{
	const struct stackatom *sa = key;

	if (0 == sa->len)
		return 0;

	return binary_hash(sa->stack, sa->len * sizeof sa->stack[0]);
}

/**
 * Comparison of two "struct stacktracea" structures.
 */
int
stack_eq(const void *a, const void *b)
{
	const struct stackatom *sa = a, *sb = b;

	return sa->len == sb->len &&
		0 == memcmp(sa->stack, sb->stack, sa->len * sizeof sa->stack[0]);
}

/**
 * Get a stack trace atom (never freed).
 */
struct stackatom *
stacktrace_get_atom(const struct stacktrace *st)
{
	struct stackatom key;
	struct stackatom *result;

	STATIC_ASSERT(sizeof st->stack[0] == sizeof result->stack[0]);

	if (NULL == stack_atoms) {
		stack_atoms = hash_table_new_full_real(stack_hash, stack_eq);
	}

	key.stack = deconstify_gpointer(st->stack);
	key.len = st->len;

	result = hash_table_lookup(stack_atoms, &key);

	if (NULL == result) {
		/* These objects will be never freed */
		result = omalloc0(sizeof *result);
		if (st->len != 0) {
			result->stack = omalloc(st->len * sizeof st->stack[0]);
			memcpy(result->stack, st->stack, st->len * sizeof st->stack[0]);
		} else {
			result->stack = NULL;
		}
		result->len = st->len;

		if (!hash_table_insert(stack_atoms, result, result))
			g_error("cannot record stack trace atom");
	}

	return result;
}

/***
 *** Low-level stack unwinding routines.
 ***
 *** The following routines rely on GCC internal macros, which are expanded
 *** at compile-time (hence the parameter must be specified explicitly and
 *** cannot be a variable).
 ***
 *** The advantage is that this is portable accross all architectures where
 *** GCC is available.
 ***
 *** The disadvantage is that GCC is required and the stack trace maximum
 *** size is constrained by the number of cases handled.
 ***
 *** Note that each GCC macro expansion yields the necessary assembly code to
 *** reach the given stackframe preceding the current frame, and therefore
 *** the code growth is exponential.  Handling 128 stack frames at most
 *** should be sufficient for our needs here, since we never need to unwind
 *** the stack back to main().
 ***
 ***		--RAM, 2010-10-24
 ***/

/*
 * getreturnaddr() and getframeaddr() are:
 *
 * Copyright (c) 2003 Maxim Sobolev <sobomax@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $X-Id: execinfo.c,v 1.3 2004/07/19 05:21:09 sobomax Exp $
 */

#if HAS_GCC(3, 0)
static void *
getreturnaddr(size_t level)
{
    switch (level) {
    case 0:		return __builtin_return_address(1);
    case 1:		return __builtin_return_address(2);
    case 2:		return __builtin_return_address(3);
    case 3:		return __builtin_return_address(4);
    case 4:		return __builtin_return_address(5);
    case 5:		return __builtin_return_address(6);
    case 6:		return __builtin_return_address(7);
    case 7:		return __builtin_return_address(8);
    case 8:		return __builtin_return_address(9);
    case 9:		return __builtin_return_address(10);
    case 10:	return __builtin_return_address(11);
    case 11:	return __builtin_return_address(12);
    case 12:	return __builtin_return_address(13);
    case 13:	return __builtin_return_address(14);
    case 14:	return __builtin_return_address(15);
    case 15:	return __builtin_return_address(16);
    case 16:	return __builtin_return_address(17);
    case 17:	return __builtin_return_address(18);
    case 18:	return __builtin_return_address(19);
    case 19:	return __builtin_return_address(20);
    case 20:	return __builtin_return_address(21);
    case 21:	return __builtin_return_address(22);
    case 22:	return __builtin_return_address(23);
    case 23:	return __builtin_return_address(24);
    case 24:	return __builtin_return_address(25);
    case 25:	return __builtin_return_address(26);
    case 26:	return __builtin_return_address(27);
    case 27:	return __builtin_return_address(28);
    case 28:	return __builtin_return_address(29);
    case 29:	return __builtin_return_address(30);
    case 30:	return __builtin_return_address(31);
    case 31:	return __builtin_return_address(32);
    case 32:	return __builtin_return_address(33);
    case 33:	return __builtin_return_address(34);
    case 34:	return __builtin_return_address(35);
    case 35:	return __builtin_return_address(36);
    case 36:	return __builtin_return_address(37);
    case 37:	return __builtin_return_address(38);
    case 38:	return __builtin_return_address(39);
    case 39:	return __builtin_return_address(40);
    case 40:	return __builtin_return_address(41);
    case 41:	return __builtin_return_address(42);
    case 42:	return __builtin_return_address(43);
    case 43:	return __builtin_return_address(44);
    case 44:	return __builtin_return_address(45);
    case 45:	return __builtin_return_address(46);
    case 46:	return __builtin_return_address(47);
    case 47:	return __builtin_return_address(48);
    case 48:	return __builtin_return_address(49);
    case 49:	return __builtin_return_address(50);
    case 50:	return __builtin_return_address(51);
    case 51:	return __builtin_return_address(52);
    case 52:	return __builtin_return_address(53);
    case 53:	return __builtin_return_address(54);
    case 54:	return __builtin_return_address(55);
    case 55:	return __builtin_return_address(56);
    case 56:	return __builtin_return_address(57);
    case 57:	return __builtin_return_address(58);
    case 58:	return __builtin_return_address(59);
    case 59:	return __builtin_return_address(60);
    case 60:	return __builtin_return_address(61);
    case 61:	return __builtin_return_address(62);
    case 62:	return __builtin_return_address(63);
    case 63:	return __builtin_return_address(64);
    case 64:	return __builtin_return_address(65);
    case 65:	return __builtin_return_address(66);
    case 66:	return __builtin_return_address(67);
    case 67:	return __builtin_return_address(68);
    case 68:	return __builtin_return_address(69);
    case 69:	return __builtin_return_address(70);
    case 70:	return __builtin_return_address(71);
    case 71:	return __builtin_return_address(72);
    case 72:	return __builtin_return_address(73);
    case 73:	return __builtin_return_address(74);
    case 74:	return __builtin_return_address(75);
    case 75:	return __builtin_return_address(76);
    case 76:	return __builtin_return_address(77);
    case 77:	return __builtin_return_address(78);
    case 78:	return __builtin_return_address(79);
    case 79:	return __builtin_return_address(80);
    case 80:	return __builtin_return_address(81);
    case 81:	return __builtin_return_address(82);
    case 82:	return __builtin_return_address(83);
    case 83:	return __builtin_return_address(84);
    case 84:	return __builtin_return_address(85);
    case 85:	return __builtin_return_address(86);
    case 86:	return __builtin_return_address(87);
    case 87:	return __builtin_return_address(88);
    case 88:	return __builtin_return_address(89);
    case 89:	return __builtin_return_address(90);
    case 90:	return __builtin_return_address(91);
    case 91:	return __builtin_return_address(92);
    case 92:	return __builtin_return_address(93);
    case 93:	return __builtin_return_address(94);
    case 94:	return __builtin_return_address(95);
    case 95:	return __builtin_return_address(96);
    case 96:	return __builtin_return_address(97);
    case 97:	return __builtin_return_address(98);
    case 98:	return __builtin_return_address(99);
    case 99:	return __builtin_return_address(100);
    case 100:	return __builtin_return_address(101);
    case 101:	return __builtin_return_address(102);
    case 102:	return __builtin_return_address(103);
    case 103:	return __builtin_return_address(104);
    case 104:	return __builtin_return_address(105);
    case 105:	return __builtin_return_address(106);
    case 106:	return __builtin_return_address(107);
    case 107:	return __builtin_return_address(108);
    case 108:	return __builtin_return_address(109);
    case 109:	return __builtin_return_address(110);
    case 110:	return __builtin_return_address(111);
    case 111:	return __builtin_return_address(112);
    case 112:	return __builtin_return_address(113);
    case 113:	return __builtin_return_address(114);
    case 114:	return __builtin_return_address(115);
    case 115:	return __builtin_return_address(116);
    case 116:	return __builtin_return_address(117);
    case 117:	return __builtin_return_address(118);
    case 118:	return __builtin_return_address(119);
    case 119:	return __builtin_return_address(120);
    case 120:	return __builtin_return_address(121);
    case 121:	return __builtin_return_address(122);
    case 122:	return __builtin_return_address(123);
    case 123:	return __builtin_return_address(124);
    case 124:	return __builtin_return_address(125);
    case 125:	return __builtin_return_address(126);
    case 126:	return __builtin_return_address(127);
    case 127:	return __builtin_return_address(128);
    default:	return NULL;
    }
}

static void *
getframeaddr(size_t level)
{
    switch (level) {
    case 0:		return __builtin_frame_address(1);
    case 1:		return __builtin_frame_address(2);
    case 2:		return __builtin_frame_address(3);
    case 3:		return __builtin_frame_address(4);
    case 4:		return __builtin_frame_address(5);
    case 5:		return __builtin_frame_address(6);
    case 6:		return __builtin_frame_address(7);
    case 7:		return __builtin_frame_address(8);
    case 8:		return __builtin_frame_address(9);
    case 9:		return __builtin_frame_address(10);
    case 10:	return __builtin_frame_address(11);
    case 11:	return __builtin_frame_address(12);
    case 12:	return __builtin_frame_address(13);
    case 13:	return __builtin_frame_address(14);
    case 14:	return __builtin_frame_address(15);
    case 15:	return __builtin_frame_address(16);
    case 16:	return __builtin_frame_address(17);
    case 17:	return __builtin_frame_address(18);
    case 18:	return __builtin_frame_address(19);
    case 19:	return __builtin_frame_address(20);
    case 20:	return __builtin_frame_address(21);
    case 21:	return __builtin_frame_address(22);
    case 22:	return __builtin_frame_address(23);
    case 23:	return __builtin_frame_address(24);
    case 24:	return __builtin_frame_address(25);
    case 25:	return __builtin_frame_address(26);
    case 26:	return __builtin_frame_address(27);
    case 27:	return __builtin_frame_address(28);
    case 28:	return __builtin_frame_address(29);
    case 29:	return __builtin_frame_address(30);
    case 30:	return __builtin_frame_address(31);
    case 31:	return __builtin_frame_address(32);
    case 32:	return __builtin_frame_address(33);
    case 33:	return __builtin_frame_address(34);
    case 34:	return __builtin_frame_address(35);
    case 35:	return __builtin_frame_address(36);
    case 36:	return __builtin_frame_address(37);
    case 37:	return __builtin_frame_address(38);
    case 38:	return __builtin_frame_address(39);
    case 39:	return __builtin_frame_address(40);
    case 40:	return __builtin_frame_address(41);
    case 41:	return __builtin_frame_address(42);
    case 42:	return __builtin_frame_address(43);
    case 43:	return __builtin_frame_address(44);
    case 44:	return __builtin_frame_address(45);
    case 45:	return __builtin_frame_address(46);
    case 46:	return __builtin_frame_address(47);
    case 47:	return __builtin_frame_address(48);
    case 48:	return __builtin_frame_address(49);
    case 49:	return __builtin_frame_address(50);
    case 50:	return __builtin_frame_address(51);
    case 51:	return __builtin_frame_address(52);
    case 52:	return __builtin_frame_address(53);
    case 53:	return __builtin_frame_address(54);
    case 54:	return __builtin_frame_address(55);
    case 55:	return __builtin_frame_address(56);
    case 56:	return __builtin_frame_address(57);
    case 57:	return __builtin_frame_address(58);
    case 58:	return __builtin_frame_address(59);
    case 59:	return __builtin_frame_address(60);
    case 60:	return __builtin_frame_address(61);
    case 61:	return __builtin_frame_address(62);
    case 62:	return __builtin_frame_address(63);
    case 63:	return __builtin_frame_address(64);
    case 64:	return __builtin_frame_address(65);
    case 65:	return __builtin_frame_address(66);
    case 66:	return __builtin_frame_address(67);
    case 67:	return __builtin_frame_address(68);
    case 68:	return __builtin_frame_address(69);
    case 69:	return __builtin_frame_address(70);
    case 70:	return __builtin_frame_address(71);
    case 71:	return __builtin_frame_address(72);
    case 72:	return __builtin_frame_address(73);
    case 73:	return __builtin_frame_address(74);
    case 74:	return __builtin_frame_address(75);
    case 75:	return __builtin_frame_address(76);
    case 76:	return __builtin_frame_address(77);
    case 77:	return __builtin_frame_address(78);
    case 78:	return __builtin_frame_address(79);
    case 79:	return __builtin_frame_address(80);
    case 80:	return __builtin_frame_address(81);
    case 81:	return __builtin_frame_address(82);
    case 82:	return __builtin_frame_address(83);
    case 83:	return __builtin_frame_address(84);
    case 84:	return __builtin_frame_address(85);
    case 85:	return __builtin_frame_address(86);
    case 86:	return __builtin_frame_address(87);
    case 87:	return __builtin_frame_address(88);
    case 88:	return __builtin_frame_address(89);
    case 89:	return __builtin_frame_address(90);
    case 90:	return __builtin_frame_address(91);
    case 91:	return __builtin_frame_address(92);
    case 92:	return __builtin_frame_address(93);
    case 93:	return __builtin_frame_address(94);
    case 94:	return __builtin_frame_address(95);
    case 95:	return __builtin_frame_address(96);
    case 96:	return __builtin_frame_address(97);
    case 97:	return __builtin_frame_address(98);
    case 98:	return __builtin_frame_address(99);
    case 99:	return __builtin_frame_address(100);
    case 100:	return __builtin_frame_address(101);
    case 101:	return __builtin_frame_address(102);
    case 102:	return __builtin_frame_address(103);
    case 103:	return __builtin_frame_address(104);
    case 104:	return __builtin_frame_address(105);
    case 105:	return __builtin_frame_address(106);
    case 106:	return __builtin_frame_address(107);
    case 107:	return __builtin_frame_address(108);
    case 108:	return __builtin_frame_address(109);
    case 109:	return __builtin_frame_address(110);
    case 110:	return __builtin_frame_address(111);
    case 111:	return __builtin_frame_address(112);
    case 112:	return __builtin_frame_address(113);
    case 113:	return __builtin_frame_address(114);
    case 114:	return __builtin_frame_address(115);
    case 115:	return __builtin_frame_address(116);
    case 116:	return __builtin_frame_address(117);
    case 117:	return __builtin_frame_address(118);
    case 118:	return __builtin_frame_address(119);
    case 119:	return __builtin_frame_address(120);
    case 120:	return __builtin_frame_address(121);
    case 121:	return __builtin_frame_address(122);
    case 122:	return __builtin_frame_address(123);
    case 123:	return __builtin_frame_address(124);
    case 124:	return __builtin_frame_address(125);
    case 125:	return __builtin_frame_address(126);
    case 126:	return __builtin_frame_address(127);
    case 127:	return __builtin_frame_address(128);
    default:	return NULL;
    }
}
#else	/* !GCC >= 3.0 */
static void *
getreturnaddr(size_t level)
{
	(void) level;
	return NULL;
}

static void *
getframeaddr(size_t level)
{
	(void) level;
	return NULL;
}
#endif	/* GCC >= 3.0 */

/* vi: set ts=4 sw=4 cindent:  */
