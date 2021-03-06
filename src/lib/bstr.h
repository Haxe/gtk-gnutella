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

#ifndef _bstr_h_
#define _bstr_h_

#include "common.h"

#include "host_addr.h"

/*
 * A binary stream.
 */

struct bstr;
typedef struct bstr bstr_t;

/*
 * Flags for bstr_open(), on 16 bits.
 */

#define BSTR_F_ERROR	(1 << 0)	/**< Build error message string */

/*
 * Public interface
 */

bstr_t *bstr_open(const void *arena, size_t len, guint32 flags);
bstr_t *bstr_create(void);
void bstr_close(bstr_t *);
void bstr_reset(bstr_t *, const void *arena, size_t len, guint32 flags);
gboolean bstr_has_error(const bstr_t *);
void bstr_clear_error(bstr_t *);
void bstr_free(bstr_t **);
const char *bstr_error(const bstr_t *);
gboolean bstr_ended(const bstr_t *bs);
void bstr_trailing_error(bstr_t *bs);

size_t bstr_unread_size(const bstr_t *);
const void *bstr_read_base(const bstr_t *);
gboolean bstr_skip(bstr_t *, size_t count);
gboolean bstr_read(bstr_t *, void *buf, size_t count);
gboolean bstr_read_u8(bstr_t *, guint8 *pv);
gboolean bstr_read_boolean(bstr_t *, gboolean *pv);
gboolean bstr_read_le16(bstr_t *, guint16 *pv);
gboolean bstr_read_be16(bstr_t *, guint16 *pv);
gboolean bstr_read_le32(bstr_t *, guint32 *pv);
gboolean bstr_read_be32(bstr_t *, guint32 *pv);
gboolean bstr_read_be64(bstr_t *, guint64 *pv);
gboolean bstr_read_time(bstr_t *, time_t *pv);
gboolean bstr_read_float_be(bstr_t *, float *pv);
gboolean bstr_read_ipv4_addr(bstr_t *, host_addr_t *);
gboolean bstr_read_ipv6_addr(bstr_t *, host_addr_t *);
gboolean bstr_read_packed_ipv4_or_ipv6_addr(bstr_t *, host_addr_t *);
gboolean bstr_read_packed_array_u8(bstr_t *,
	size_t max, void *ptr, guint8 *pr);
gboolean bstr_read_ule64(bstr_t *, guint64 *pv);
gboolean bstr_read_fixed_string(bstr_t *, size_t *slen, char *buf, size_t len);
gboolean bstr_read_string(bstr_t *, size_t *slen, char **sptr);

#endif	/* _bstr_h_ */

/* vi: set ts=4 sw=4 cindent: */
