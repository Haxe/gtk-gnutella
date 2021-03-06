/*
 * Copyright (c) 2009, Raphael Manfredi
 * Copyright (c) 2006-2008, Christian Biere
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
 * File descriptor functions.
 *
 * @author Raphael Manfredi
 * @date 2009
 * @author Christian Biere
 * @date 2006-2008
 */

#ifndef _fd_h_
#define _fd_h_

void close_file_descriptors(const int first_fd);
int reserve_standard_file_descriptors(void);
void set_close_on_exec(int fd);
void fd_set_nonblocking(int fd);
int fd_forget_and_close(int *fd_ptr);
int fd_close(int *fd_ptr);
int get_non_stdio_fd(int fd);
gboolean need_get_non_stdio_fd();
gboolean is_a_socket(int fd);
gboolean is_a_fifo(int fd);
gboolean is_open_fd(int fd);

static inline int
is_valid_fd(int fd)
{
	return fd >= 0;
}

static inline int
cast_to_fd(unsigned int fd)
{
	return fd;
}

#endif /* _fd_h_ */

/* vi: set ts=4 sw=4 cindent: */
