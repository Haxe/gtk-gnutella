/*
 * $Id$
 *
 * Copyright (c) 2001-2003, Richard Eckart
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
 * @ingroup core
 * @file
 *
 * Common header for Gtk-Gnutella.
 *
 * @author Richard Eckart
 * @date 2001-2003
 */

#ifndef _common_h_
#define _common_h_

#include "config.h"

#ifndef HAS_LIBXML2
#error "You need libxml2 (http://www.xmlsoft.org/) to compile Gtk-Gnutella"
#endif

/*
 * Main includes
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#ifdef I_SYS_TIME_KERNEL
#define KERNEL
#include <sys/time.h>
#undef KERNEL
#endif

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>		/* For writev(), readv(), struct iovec */
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>		/* For ntohl(), htonl() */
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <setjmp.h>

#ifdef I_TIME
#include <time.h>
#endif

#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#ifdef I_SYS_SYSCTL
#include <sys/sysctl.h>
#endif
#ifdef I_INVENT
#include <invent.h>
#endif

#ifdef I_INTTYPES
#include <inttypes.h>
#endif /* I_INTTYPES */

#ifdef I_SYS_SENDFILE
#include <sys/sendfile.h>
#else	/* !I_SYS_SENDFILE */
#ifdef HAS_SENDFILE
#define USE_BSD_SENDFILE	/**< No <sys/sendfile.h>, assume BSD version */
#else

/* mmap() support requires ISO C functions like sigsetjmp(). */
#if defined(__STDC_VERSION__)
#define USE_MMAP 1
#include <sys/mman.h>
#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif	/* !MMAP_FAILED */
#endif	/* ISO C */

#endif	/* HAS_SENDFILE */
#endif	/* I_SYS_SENDFILE_H */

#if defined(USE_IP_TOS) && defined(I_NETINET_IP)
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

/* For pedantic lint checks, define USE_LINT. We override some definitions
 * and hide ``inline'' to prevent certain useless warnings. */
#ifdef USE_LINT
#undef G_GNUC_INTERNAL
#define G_GNUC_INTERNAL
#undef G_INLINE_FUNC
#define G_INLINE_FUNC
#define inline
#endif

#include <glib.h>

#ifdef USE_LINT
#undef G_GNUC_INTERNAL
#define G_GNUC_INTERNAL
#undef G_INLINE_FUNC
#define G_INLINE_FUNC
#define inline
#endif

typedef guint64 filesize_t; /**< Use filesize_t to hold filesizes */

#include <stdarg.h>
#include <regex.h>

#include <zlib.h>

#ifdef USE_GLIB1
typedef void (*GCallback) (void);
#endif
#ifdef USE_GLIB2
#include <glib-object.h>
#endif

/*
 * Portability macros.
 */

/*
 * Can only use the `args' obtained via va_start(args) ONCE.  If we need
 * to call another vararg routine, we need to copy the original args.
 * The __va_copy macro is a GNU extension.
 */
#ifdef va_copy
#define VA_COPY(dest, src) va_copy(dest, src)
#elif defined(__va_copy)
#define VA_COPY(dest, src)	__va_copy(dest, src)
#else
#define VA_COPY(dest, src)	(dest) = (src)
#endif

/*
 * Other common macros.
 */

#define SRC_PREFIX	"src/"		/**< Common prefix to remove in filenames */

/*
 * Sources should use _WHERE_ instead of __FILE__ and call short_filename()
 * on the resulting string before perusing it to remove the common prefix
 * defined by SRC_PREFIX.
 */
#ifdef CURDIR					/* Set by makefile */
#define _WHERE_	STRINGIFY(CURDIR) "/" __FILE__
#else
#define _WHERE_	__FILE__
#endif

/**
 * Calls g_free() and sets the pointer to NULL afterwards. You should use
 * this instead of a bare g_free() to prevent double-free bugs and dangling
 * pointers.
 */
#define G_FREE_NULL(p)		\
do {				\
	if (p) {		\
		g_free(p);	\
		p = NULL;	\
	}			\
} while (0)

/**
 * Stores a RCS ID tag inside the object file. Every .c source file should
 * use this macro once as `RCSID("<dollar>Id$")' on top. The ID tag is
 * automagically updated each time the file is committed to the CVS repository.
 * The RCS IDs can be looked up from the compiled binary with e.g. `what',
 * `ident' or `strings'. See also rcs(1) and ident(1).
 */
#ifdef __GNUC__
#define RCSID(x) \
	static const char rcsid[] __attribute__((__unused__)) = "@(#) " x
#else
#define RCSID(x) static const char rcsid[] = "@(#) " x
#endif

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#define HAVE_GCC(major, minor) \
	((__GNUC__ > (major)) || \
	 (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#else
#define HAVE_GCC(major, minor) 0
#endif

/* Functions using this attribute cause a warning if the returned
 * value is not used. */
#if HAVE_GCC(3, 4)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else /* GCC < 3.4 */
#define WARN_UNUSED_RESULT
#endif

/* Functions using this attribute cause a warning if the variable
 * argument list does not contain a NULL pointer. */
#if HAVE_GCC(4, 0)
#define WARN_NEED_SENTINEL __attribute__((sentinel))
#else /* GCC < 4 */
#define WARN_NEED_SENTINEL 
#endif /* GCC >= 4 */

/**
 * CMP() returns the sign of a-b, that means -1, 0, or 1.
 */
#define CMP(a, b) ((a) == (b) ? 0 : (a) > (b) ? 1 : (-1))

/**
 * STATIC_ASSERT() can be used to verify conditions at compile-time. For
 * example, it can be used to ensure that an array has a minimum or exact
 * size. This is better than a run-time assertion because the condition is
 * checked even if the code would seldomly or never reached at run-time.
 * However, this can only be used for static conditions which can be verified
 * at compile-time.
 *
 * @attention
 * N.B.: The trick is using a switch case, if the term is false
 *	 there are two cases for zero - which is invalid C. This cannot be
 *	 used outside a function.
 */
#define STATIC_ASSERT(x) \
	do { switch (0) { case ((x) ? 1 : 0): case 0: break; } } while(0)

/*
 * Constants
 */

#define GTA_VERSION 0			  /**< major version */
#define GTA_SUBVERSION 96		  /**< minor version */
#define GTA_PATCHLEVEL 0		  /**< patch level or teeny version */
#define GTA_REVISION "unstable"	  /**< unstable, beta, stable */
#define GTA_REVCHAR "u"			  /**< u - unstable, b - beta, none - stable */
#define GTA_RELEASE "2005-08-13"  /**< ISO 8601 format YYYY-MM-DD */
#define GTA_WEBSITE "http://gtk-gnutella.sourceforge.net/"

#if defined(USE_GTK1)
#define GTA_INTERFACE "GTK1"
#elif defined(USE_GTK2)
#define GTA_INTERFACE "GTK2"
#elif defined(USE_TOPLESS)
#define GTA_INTERFACE "Topless"
#else
#define GTA_INTERFACE "X11"
#endif

#define xstr(x) STRINGIFY(x)

#if defined(GTA_PATCHLEVEL) && (GTA_PATCHLEVEL != 0)
#define GTA_VERSION_NUMBER \
	xstr(GTA_VERSION) "." xstr(GTA_SUBVERSION) "." xstr(GTA_PATCHLEVEL) \
		GTA_REVCHAR
#else
#define GTA_VERSION_NUMBER \
	xstr(GTA_VERSION) "." xstr(GTA_SUBVERSION) GTA_REVCHAR
#endif

#define GTA_PORT			6346	/**< Default "standard" port */
#define MAX_HOSTLEN			256		/**< Max length for FQDN host */

/* The next two defines came from huge.h --- Emile */
#define SHA1_BASE32_SIZE 	32		/**< 160 bits in base32 representation */
#define SHA1_RAW_SIZE		20		/**< 160 bits in binary representation */


/*
 * Forbidden glib calls.
 */

#define g_snprintf	DONT_CALL_g_snprintf /**< Use gm_snprintf instead */
#define g_vsnprintf	DONT_CALL_g_vsnprintf /**< Use gm_vsnprintf instead */

/*
 * Typedefs
 */

typedef gboolean (*reclaim_fd_t)(void);

/*
 * Standard gettext macros.
 */

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext(PACKAGE, String)
#  define Q_(String) g_strip_context ((String), gettext (String))
#  ifdef gettext_noop
#    define N_(String) gettext_noop(String)
#  else
#    define N_(String) (String)
#  endif


#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define ngettext(Single, Plural, Number) ((Number) == 1 ? (Single) : (Plural))
#  define _(String) (String)
#  define N_(String) (String)
#  define Q_(String) g_strip_context ((String), (String))
#endif /* ENABLE_NLS */

static inline const gchar *
ngettext_(const gchar *msg1, const gchar *msg2, gulong n)
G_GNUC_FORMAT(1) G_GNUC_FORMAT(2);

static inline const gchar *
ngettext_(const gchar *msg1, const gchar *msg2, gulong n)
{
	return ngettext(msg1, msg2, n);
}

/**
 * Short-hand for ngettext().
 */
#define NG_(Single, Plural, Number) ngettext_((Single), (Plural), (Number))

enum net_type {
	NET_TYPE_NONE	= 0,
	NET_TYPE_IP4	= 4,
	NET_TYPE_IP6	= 6,
};

#ifdef USE_IPV6
typedef struct host_addr {
	guint32 net;	/**< The address network type */
	union {
		guint32 ip4;	/**< @attention: Always in host byte order! */
		guint8 ip6[16];	/**< This is valid if "net == NET_TYPE_IP6" */
	} addr;
} host_addr_t;
#else

/* For an IPv4-only configuration */
typedef guint32 host_addr_t; /**< @attention: Always in host byte order! */

#endif

#endif /* _common_h_ */

/* vi: set ts=4 sw=4 cindent: */
