/*
 * Copyright (c) 2008, Raphael Manfredi
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
 * Binary memory stream parsing.
 *
 * @author Raphael Manfredi
 * @date 2008
 */

#include "common.h"

#include "bstr.h"
#include "casts.h"
#include "endian.h"
#include "glib-missing.h"
#include "halloc.h"
#include "host_addr.h"
#include "str.h"
#include "stringify.h"
#include "unsigned.h"
#include "walloc.h"
#include "override.h"			/* Must be the last header included */

#define BSTR_ERRLEN		160			/**< Default length for error string */

enum bstr_magic { BSTR_MAGIC = 0x2dc80bd8 };

/**
 * A binary stream.
 */
struct bstr {
	enum bstr_magic magic;
	const unsigned char *start;		/**< First byte in buffer */
	const unsigned char *rptr;		/**< First unread byte in buffer */
	const unsigned char *end;		/**< First byte beyond buffer */
	str_t *error;					/**< Last parsing error */
	guint32 flags;					/**< Configuration flags */
	gboolean ok;					/**< Whether everything is OK so far */
};

static inline void
bstr_check(const struct bstr * const bs)
{
	g_assert(bs != NULL);
	g_assert(BSTR_MAGIC == bs->magic);
}

/*
 * Internal operating flags start at bit 16.
 */

#define BSTR_F_PRIVATE	0xffff0000	/**< Mask for private flags */
#define BSTR_F_EOS		(1 << 16)	/**< End of stream reached */
#define BSTR_F_PENDING	(1 << 17)	/**< Pending reset, stream invalid */
#define BSTR_F_TRAILING	(1 << 18)	/**< Has trailing unread bytes */

/**
 * Reset the stream.
 */
static void
reset_stream(bstr_t *bs, const void *arena, size_t len, guint32 flags)
{
	bs->ok = TRUE;
	bs->flags = flags;
	bs->rptr = arena;
	bs->start = bs->rptr;
	bs->end = bs->start + len;

	if (bs->error != NULL)
		str_setlen(bs->error, 0);
}

/**
 * Mark stream unusable.
 */
static void
mark_unusable(bstr_t *bs)
{
	bs->ok = FALSE;
	bs->flags |= BSTR_F_PENDING;
}

/**
 * Allocate error string if not already done.
 */
static void
alloc_error(bstr_t *bs)
{
	if (NULL == bs->error)
		bs->error = str_new(BSTR_ERRLEN);
}

/**
 * Record End of Stream condition.
 * @return FALSE
 */
static gboolean
error_eos(bstr_t *bs, size_t expected, const char *where)
{
	bs->flags |= BSTR_F_EOS;

	if (bs->flags & BSTR_F_ERROR) {
		alloc_error(bs);
		str_printf(bs->error,
			"%s: end of stream reached at offset %lu; expected %s more byte%s",
			where, (unsigned long) (bs->end - bs->start),
			expected ? size_t_to_string(expected) : "some",
			expected == 1 ? "" : "s");
	}

	return bs->ok = FALSE;
}

/**
 * Create a new memory stream to parse given arena
 *
 * @param arena		the base of the memory arena
 * @param len		total length of the arena to parse
 * @param flags		configuration flags
 *
 * @return a stream descriptor that must be freed with bstr_free(), or
 * which can be reused through bstr_close() and bstr_reset() at will.
 */
bstr_t *
bstr_open(const void *arena, size_t len, guint32 flags)
{
	bstr_t *bs;

	g_assert(arena);
	g_assert(0 == (flags & BSTR_F_PRIVATE));

	WALLOC0(bs);
	bs->magic = BSTR_MAGIC;
	reset_stream(bs, arena, len, flags);

	if (len == 0)
		error_eos(bs, 0, "bstr_open");

	return bs;
}

/**
 * Reset an already used stream or something created through bstr_create()
 * to use it on another arena, with possibly different operating flags.
 *
 * @param arena		the base of the memory arena
 * @param len		total length of the arena to parse
 * @param flags		configuration flags
 */
void
bstr_reset(bstr_t *bs, const void *arena, size_t len, guint32 flags)
{
	bstr_check(bs);
	g_assert(arena);
	g_assert(0 == (flags & BSTR_F_PRIVATE));

	reset_stream(bs, arena, len, flags);

	if (len == 0)
		error_eos(bs, 0, "bstr_reset");
}

/**
 * Create the stream, but make it unusable until a bstr_reset() has
 * been done.
 *
 * @return a stream descriptor that must be freed with bstr_free(), or
 * which can be reused through bstr_close() and bstr_reset() at will.
 */
bstr_t *
bstr_create(void)
{
	bstr_t *bs;

	WALLOC0(bs);
	bs->magic = BSTR_MAGIC;
	mark_unusable(bs);

	return bs;
}

/**
 * Close memory stream.
 * Stream can then be freed by bstr_free() or reused via bstr_reset().
 */
void
bstr_close(bstr_t *bs)
{
	bstr_check(bs);

	str_destroy_null(&bs->error);
	mark_unusable(bs);
}

/**
 * Destroy memory stream.
 */
void
bstr_free(bstr_t **bs_ptr)
{
	bstr_t *bs;

	g_assert(bs_ptr != NULL);

	bs = *bs_ptr;

	if (bs != NULL) {
		bstr_close(bs);
		bs->magic = 0;
		WFREE(bs);
		*bs_ptr = NULL;
	}
}

/**
 * Clear last error.
 */
void
bstr_clear_error(bstr_t *bs)
{
	bstr_check(bs);

	if (bs->ok)
		return;

	/*
	 * Do not clear errors on a stream that is pending resetting.
	 */

	if (bs->flags & BSTR_F_PENDING)
		return;

	bs->ok = TRUE;

	if (bs->error != NULL)
		str_setlen(bs->error, 0);
}

/**
 * Check whether there was an error.
 */
gboolean
bstr_has_error(const bstr_t *bs)
{
	bstr_check(bs);

	return !bs->ok;
}

/**
 * Check whether the stream was fully deserialized with no error.
 */
gboolean
bstr_ended(const bstr_t *bs)
{
	bstr_check(bs);

	return bs->ok && bs->end == bs->rptr;
}

/**
 * Report error string.
 */
const char *
bstr_error(const bstr_t *bs)
{
	bstr_check(bs);

	if (bs->ok)
		return "OK";
	else if (bs->flags & BSTR_F_PENDING)
		return "stream waiting for a reset";
	else if (bs->flags & BSTR_F_ERROR)
		return str_2c(bs->error);
	else if (bs->flags & BSTR_F_EOS)
		return "end of stream reached";
	else if (bs->flags & BSTR_F_TRAILING)
		return "has trailing unread bytes";
	else
		return "parsing error";
}

/**
 * Report invalid length condition.
 * @return FALSE
 */
static gboolean
invalid_len(bstr_t *bs, size_t len, const char *what, const char *where)
{
	if (bs->flags & BSTR_F_ERROR) {
		alloc_error(bs);
		str_printf(bs->error,
			"%s: invalid %s length %lu at offset %lu",
			where, what, (unsigned long ) len,
			(unsigned long) (bs->rptr - bs->start));
	}

	return bs->ok = FALSE;
}

/**
 * Report invalid length condition, bounded by a maximum value.
 * @return FALSE
 */
static gboolean
invalid_len_max(
	bstr_t *bs, size_t len, size_t max, const char *what, const char *where)
{
	if (bs->flags & BSTR_F_ERROR) {
		alloc_error(bs);
		str_printf(bs->error,
			"%s: invalid %s length %lu (max is %lu) at offset %lu",
			where, what, (unsigned long) len, (unsigned long) max,
			(unsigned long) (bs->rptr - bs->start));
	}

	return bs->ok = FALSE;
}

/**
 * Report invalid encoding detected.
 * @return FALSE
 */
static gboolean
invalid_encoding(bstr_t *bs, const char *what, const char *where)
{
	if (bs->flags & BSTR_F_ERROR) {
		alloc_error(bs);
		str_printf(bs->error,
			"%s: invalid encoding at offset %lu: %s",
			where, (unsigned long) (bs->rptr - bs->start), what);
	}

	return bs->ok = FALSE;
}

/**
 * Report error.
 * @return FALSE
 */
static gboolean
report_error(bstr_t *bs, const char *what, const char *where)
{
	if (bs->flags & BSTR_F_ERROR) {
		alloc_error(bs);
		str_printf(bs->error,
			"%s: error at offset %lu: %s",
			where, (unsigned long) (bs->rptr - bs->start), what);
	}

	return bs->ok = FALSE;
}

/**
 * @return amount of unread data.
 */
size_t
bstr_unread_size(const bstr_t *bs)
{
	bstr_check(bs);
	g_assert(bs->end >= bs->rptr);

	return bs->end - bs->rptr;
}

/**
 * Record presence of trailing bytes as an error.
 */
void
bstr_trailing_error(bstr_t *bs)
{
	size_t n;

	bstr_check(bs);

	n = bstr_unread_size(bs);

	if (n != 0) {
		if (bs->flags & BSTR_F_ERROR) {
			alloc_error(bs);
			str_printf(bs->error, "has %lu trailing unread byte%s",
				(unsigned long) n, 1 == n ? "" : "s");
		} else {
			bs->flags |= BSTR_F_TRAILING;
		}
	}
}

/**
 * Check that stream contains at least the expected amount of bytes.
 * Raise the "End Of Stream" error.
 */
static gboolean
expect(bstr_t *bs, size_t expected, const char *where)
{
	g_assert(bs);
	g_assert(size_is_positive(expected));

	if (!bs->ok)
		return FALSE;

	if (bstr_unread_size(bs) >= expected)
		return TRUE;

	return error_eos(bs, expected, where);
}

/**
 * @return current reading position.
 */
const void *
bstr_read_base(const bstr_t *bs)
{
	bstr_check(bs);

	return bs->rptr;
}

/**
 * Skip specified amount of bytes.
 *
 * @param bs	the binary stream
 * @param count	amount of data to skip over (can be 0 for a NOP)
 *
 * @return TRUE if OK.
 */
gboolean
bstr_skip(bstr_t *bs, size_t count)
{
	bstr_check(bs);
	g_assert(size_is_non_negative(count));

	if (!count)
		return TRUE;

	if (!expect(bs, count, "bstr_skip"))
		return FALSE;

	bs->rptr += count;
	return TRUE;
}

/**
 * Read specified amount of bytes into buffer.
 *
 * @param bs	the binary stream
 * @param buf	where to write read data
 * @param count	amount of data to read
 * 
 * @return TRUE if OK.
 */
gboolean
bstr_read(bstr_t *bs, void *buf, size_t count)
{
	bstr_check(bs);
	g_assert(size_is_positive(count));
	g_assert(buf);

	if (!expect(bs, count, "bstr_read"))
		return FALSE;

	memcpy(buf, bs->rptr, count);
	bs->rptr += count;
	return TRUE;
}

/**
 * Read a byte.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_u8(bstr_t *bs, guint8 *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 1, "bstr_read_u8"))
		return FALSE;

	*pv = peek_u8(bs->rptr++);
	return TRUE;
}

/**
 * Read a boolean.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_boolean(bstr_t *bs, gboolean *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 1, "bstr_read_boolean"))
		return FALSE;

	*pv = peek_u8(bs->rptr++) ? TRUE : FALSE;
	return TRUE;
}

/**
 * Read a little-endian 16-bit quantity.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_le16(bstr_t *bs, guint16 *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 2, "bstr_read_le16"))
		return FALSE;

	*pv = peek_le16(bs->rptr);
	bs->rptr += 2;
	return TRUE;
}

/**
 * Read a big-endian 16-bit quantity.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_be16(bstr_t *bs, guint16 *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 2, "bstr_read_be16"))
		return FALSE;

	*pv = peek_be16(bs->rptr);
	bs->rptr += 2;
	return TRUE;
}

/**
 * Read a little-endian 32-bit quantity.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_le32(bstr_t *bs, guint32 *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 4, "bstr_read_le32"))
		return FALSE;

	*pv = peek_le32(bs->rptr);
	bs->rptr += 4;
	return TRUE;
}

/**
 * Read a big-endian 32-bit quantity.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_be32(bstr_t *bs, guint32 *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 4, "bstr_read_be32"))
		return FALSE;

	*pv = peek_be32(bs->rptr);
	bs->rptr += 4;
	return TRUE;
}

/**
 * Read a big-endian 64-bit quantity.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_be64(bstr_t *bs, guint64 *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 8, "bstr_read_be64"))
		return FALSE;

	*pv = peek_be64(bs->rptr);
	bs->rptr += 8;
	return TRUE;
}

/**
 * Read time.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_time(bstr_t *bs, time_t *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 4, "bstr_read_time"))
		return FALSE;

	*pv = (time_t) peek_be32(bs->rptr);
	bs->rptr += 4;
	return TRUE;
}

/**
 * Read big-endian float.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_float_be(bstr_t *bs, float *pv)
{
	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 4, "bstr_read_float"))
		return FALSE;

	*pv = peek_float_be32(bs->rptr);
	bs->rptr += 4;

	return TRUE;
}

/**
 * Read ipv4 address.
 *
 * @param bs	the binary stream
 * @param ha	pointer to the host address structure to fill
 *
 * @return TRUE if OK
 */
gboolean
bstr_read_ipv4_addr(bstr_t *bs, host_addr_t *ha)
{
	static const char where[] = "bstr_read_ipv4_addr";

	bstr_check(bs);
	g_assert(ha);

	if (!expect(bs, 4, where))
		return FALSE;

	*ha = host_addr_peek_ipv4(bs->rptr);
	bs->rptr += 4;

	return TRUE;
}

/**
 * Read ipv6 address.
 *
 * @param bs	the binary stream
 * @param ha	pointer to the host address structure to fill
 *
 * @return TRUE if OK
 */
gboolean
bstr_read_ipv6_addr(bstr_t *bs, host_addr_t *ha)
{
	static const char where[] = "bstr_read_ipv6_addr";

	bstr_check(bs);
	g_assert(ha);

	if (!expect(bs, 16, where))
		return FALSE;

	*ha = host_addr_peek_ipv6(bs->rptr);
	bs->rptr += 16;

	return TRUE;
}

/**
 * Read a packed IP address in the following format:
 *
 * 1 byte for the address length: 4 for IPv4, 16 for IPv6
 * 4 or 16 bytes of the address (in big endian format)
 *
 * @param bs	the binary stream
 * @param ha	pointer to the host address structure to fill
 *
 * @return TRUE if OK
 */
gboolean
bstr_read_packed_ipv4_or_ipv6_addr(bstr_t *bs, host_addr_t *ha)
{
	static const char where[] = "bstr_read_packed_ipv4_or_ipv6_addr";
	guint8 len;

	bstr_check(bs);
	g_assert(ha);

	if (!expect(bs, 1, where))
		return FALSE;

	len = peek_u8(bs->rptr++);
	if (len != 4 && len != 16)
		return invalid_len(bs, len, "IP address", where);

	if (!expect(bs, len, where))
		return FALSE;

	switch (len) {
	case 4:  *ha = host_addr_peek_ipv4(bs->rptr); break;
	case 16: *ha = host_addr_peek_ipv6(bs->rptr); break;
	default:
		g_assert_not_reached();
	}

	bs->rptr += len;

	return TRUE;
}

/**
 * Read packed array of bytes of at most "max" bytes.
 *
 * The serialized format is:
 * 1 byte for the length of the following array (0 means empty array)
 * the bytes of the array, in increasing index order.
 *
 * @param bs	the binary stream
 * @param max	maximum number of bytes expected in the array
 * @param ptr	where to put the read bytes (must be at least "max" byte long)
 * @param pr	where to put the amount of bytes read in ptr
 *
 * @return TRUE if OK
 */
gboolean
bstr_read_packed_array_u8(bstr_t *bs, size_t max, void *ptr, guint8 *pr)
{
	static const char where[] = "bstr_read_packed_array_u8";
	guint8 len;

	bstr_check(bs);
	g_assert(ptr);
	g_assert(pr);

	if (!expect(bs, 1, where))
		return FALSE;

	len = peek_u8(bs->rptr++);

	if (len > max)
		return invalid_len_max(bs, len, max, "array size", where);

	if (len) {
		if (!expect(bs, len, where))
			return FALSE;

		memcpy(ptr, bs->rptr, len);
		bs->rptr += len;
	}

	*pr = len;

	return TRUE;
}

/**
 * Read unsigned 64-bit quantity encoded using variable-length representation.
 * Bytes appear in little-endian and contain 7 bits of the value.  The last
 * byte of the sequence is flagged with its highest bit set.
 *
 * @param bs	the binary stream
 * @param pv	where to write the value
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_ule64(bstr_t *bs, guint64 *pv)
{
	static const char where[] = "bstr_read_ule64";
	guint64 value = 0;
	guint8 shift = 0;
	guint8 n = 0;

	bstr_check(bs);
	g_assert(pv);

	if (!expect(bs, 1, where))	/* Need at least one byte */
		return FALSE;

	for (;;) {
		guint8 byt;
		if (!bstr_read_u8(bs, &byt))
			return FALSE;
		n++;
		if (n > 10)
			return invalid_encoding(bs, "no end seen after 10 bytes", where);
		value |= (byt & 0x7f) << shift;	/* Got 7 more bits */
		if (byt & 0x80)
			break;			/* Highest bit set, end of encoding */
		shift += 7;
	}

	*pv = value;
	return TRUE;
}

/**
 * Read fixed-length string into buffer.
 * The string is expected to be encoded as: <ule64(length)><bytes>, with
 * no trailing NUL.
 *
 * @param bs	the binary stream
 * @param slen	where to write the length of the string read (if non NULL)
 * @param buf	buffer where string is copied
 * @param len	length of supplied buffer
 *
 * A trailing NUL is appended to the buffer.  It is an error if the buffer
 * is too short.
 *
 * @return TRUE if OK.
 */
gboolean
bstr_read_fixed_string(bstr_t *bs, size_t *slen, char *buf, size_t len)
{
	static const char where[] = "bstr_read_fixed_string";
	guint64 length;
	size_t n;

	bstr_check(bs);
	g_assert(slen);
	g_assert(buf);
	g_assert(size_is_positive(len));

	if (!expect(bs, 1, where))	/* Need at least one byte */
		return FALSE;

	if (!bstr_read_ule64(bs, &length))
		return FALSE;

	if (length > MAX_INT_VAL(size_t))
		return report_error(bs, "encoded length too large", where);

	n = (size_t) length;

	g_assert((guint64) n == length);	/* Nothing lost by casting */

	if (slen != NULL) {
		*slen = n;
	}

	if (len < n + 1)
		return report_error(bs, "buffer smaller than string", where);

	buf[n] = '\0';				/* NUL-terminate in advance */

	return 0 == n ? TRUE : bstr_read(bs, buf, n);
}

/**
 * Read string into buffer, allocating a new string buffer via halloc().
 *
 * The string is expected to be encoded as: <ule64(length)><bytes>, with
 * no trailing NUL.
 *
 * A trailing NUL is appended to the allocated string buffer so that the
 * returned buffer can be safely handled as a string.
 *
 * @param bs	the binary stream
 * @param slen	where to write the length of the string read (if non NULL)
 * @param sptr	where start of string is returned (must not be NULL)
 *
 * @return TRUE if OK, FALSE otherwise in which case nothing is allocated.
 */
gboolean
bstr_read_string(bstr_t *bs, size_t *slen, char **sptr)
{
	static const char where[] = "bstr_read_string";
	guint64 length;
	size_t n;
	char *buf;
	gboolean ok;

	bstr_check(bs);
	g_assert(sptr);

	if (!expect(bs, 1, where))	/* Need at least one byte */
		return FALSE;

	if (!bstr_read_ule64(bs, &length))
		return FALSE;

	if (length > MAX_INT_VAL(size_t))
		return report_error(bs, "encoded length too large", where);

	n = (size_t) length;

	g_assert((guint64) n == length);	/* Nothing lost by casting */

	if (slen != NULL) {
		*slen = n;
	}

	buf = halloc(n + 1);				/* Provision for trailing NUL */
	buf[n] = '\0';						/* NUL-terminate in advance */
	*sptr = buf;

	ok = 0 == n ? TRUE : bstr_read(bs, buf, n);

	if (!ok) {
		*sptr = NULL;
		hfree(buf);
	}

	return ok;
}

/* vi: set ts=4 sw=4 cindent: */
