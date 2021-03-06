/*
 * Copyright (c) 2010, Raphael Manfredi
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
 * Random numbers.
 *
 * @author Raphael Manfredi
 * @date 2010
 */

#ifndef _random_h_
#define _random_h_

#include "common.h"

/*
 * Random numbers
 */

void random_init(void);
guint32 random_value(guint32 max) WARN_UNUSED_RESULT;
guint32 random_u32(void) WARN_UNUSED_RESULT;
void random_bytes(void *dst, size_t size);
guint32 random_cpu_noise(void);
void random_collect(void (*cb)(void));
void random_pool_append(void *buf, size_t len, void (*cb)(void));
void random_add(const void *data, size_t datalen);

#endif /* _random_h_ */

/* vi: set ts=4 sw=4 cindent: */
