/*
 * $Id$
 *
 * Copyright (c) 2002, Vidar Madsen
 *
 * Structure for storing meta-information about files being downloaded.
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

#ifndef __fileinfo_h__
#define __fileinfo_h__

enum dl_chunk_status {
	DL_CHUNK_EMPTY = 0,
	DL_CHUNK_BUSY = 1,
	DL_CHUNK_DONE = 2
};

struct dl_file_chunk {
	guint32 from;					/* Range offset start (byte included) */
	guint32 to;						/* Range offset end (byte EXCLUDED) */
	enum dl_chunk_status status;	/* Status of range */
	struct download *download;		/* Download that "reserved" the range */
};

struct dl_file_info {
	gchar *file_name;	/* Output file name (atom) */
	gchar *path;		/* Output file path (atom) */
	GSList *alias;		/* List of file name aliases (atoms) */
	guint32 size;		/* File size */
	gchar *sha1;		/* SHA1 (atom) if known, NULL if not. */
	guint32 refcount;	/* Reference count of file */
	time_t stamp;		/* Time stamp */
	time_t last_flush;	/* When last flush to disk occurred */
	guint32 done;		/* Total number of bytes completed */
	GSList *chunklist;	/* List of ranges within file */
	guint32 generation;	/* Generation number, incremented on disk update */
	gboolean use_swarming; /* Use swarming? */
	gboolean keep;		/* Can the entry be skipped at exit? */
	gboolean dirty;		/* Does it need saving? */
};

void file_info_init(void);
off_t file_info_filesize(gchar *path);
void file_info_retrieve(void);
void file_info_store(void);
void file_info_store_if_dirty(void);
enum dl_chunk_status file_info_find_hole(
	struct download *d, guint32 *from, guint32 *to);
void file_info_merge_adjacent(struct dl_file_info *fi);
void file_info_clear_download(struct download *d);
enum dl_chunk_status file_info_chunk_status(
	struct dl_file_info *fi, guint32 from, guint32 to);
struct dl_file_info *file_info_get(
	gchar *file, gchar *path, guint32 size, gchar *sha1);
void file_info_free(struct dl_file_info *fi, gboolean keep);
void file_info_strip_binary(struct dl_file_info *fi);
void file_info_update(
	struct download *d, guint32 from, guint32 to, enum dl_chunk_status status);
enum dl_chunk_status file_info_pos_status(struct dl_file_info *fi, guint32 pos);
void file_info_close(void);

#endif /* __fileinfo_h__ */

