/*
 * Copyright (c) 2001-2002, Raphael Manfredi
 *
 * Handles upload of our files to others users.
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

#include "gnutella.h"
#include "interface.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "config.h"

#ifdef HAVE_SENDFILE_H
#include <sys/sendfile.h>
#endif

#include "sockets.h"
#include "share.h"
#include "gui.h"
#include "misc.h"
#include "getline.h"
#include "header.h"
#include "hosts.h"		/* for check_valid_host() */
#include "url.h"
#include "bsched.h"
#include "upload_stats.h"

GSList *uploads = NULL;
gint running_uploads = 0;
gint registered_uploads = 0;

guint32 count_uploads = 0;

/*
 * This structure is used to encapsulate the various arguments required
 * by the header parsing I/O callback.
 */
struct io_header {
	struct upload *upload;
	header_t *header;
	getline_t *getline;
	void (*process_header)(struct io_header *);
	gint flags;
};

#define IO_STATUS_LINE		0x00000002	/* First line is a status line */

static void upload_request(struct upload *u, header_t *header);
static void upload_error_remove(struct upload *u, int code, guchar *msg, ...);

/*
 * upload_timer
 *
 * Upload heartbeat timer.
 */
void upload_timer(time_t now)
{
	GSList *l;
	GSList *remove = NULL;
	guint32 t;

	for (l = uploads; l; l = l->next) {
		struct upload *u = (struct upload *) l->data;

		if (UPLOAD_IS_COMPLETE(u))
			continue;					/* Complete, no timeout possible */

		if (UPLOAD_IS_VISIBLE(u))
			gui_update_upload(u);

		/*
		 * Check for timeouts.
		 */

		t = UPLOAD_IS_CONNECTING(u) ?
				upload_connecting_timeout :
				upload_connected_timeout;

		/*
		 * We can't call upload_remove() since it will remove the upload
		 * from the list we are traversing.
		 */

		if (now - u->last_update > t)
			remove = g_slist_append(remove, u);
	}

	for (l = remove; l; l = l->next) {
		struct upload *u = (struct upload *) l->data;
		if (UPLOAD_IS_CONNECTING(u)) {
			upload_error_remove(u, 408, "Request timeout");
		} else
			upload_remove(u, "Data timeout");
	}
	g_slist_free(remove);
}

/*
 * io_free
 *
 * Free the opaque I/O data.
 */
static void io_free(gpointer opaque)
{
	struct io_header *ih = (struct io_header *) opaque;

	g_assert(ih);
	g_assert(ih->upload->io_opaque == opaque);

	ih->upload->io_opaque = NULL;

	if (ih->header)
		header_free(ih->header);
	if (ih->getline)
		getline_free(ih->getline);

	g_free(ih);
}

/*
 * extract_http_version
 *
 * Extract HTTP version major/minor out of the given request, whose string
 * length is `len' bytes.
 *
 * Returns TRUE when we identified the "HTTP/x.x" trailing string, filling
 * major and minor accordingly.
 */
static gboolean extract_http_version(
	gchar *request, gint len, gint *major, gint *minor)
{
	gint limit;
	gchar *p;
	gint i;

	/*
	 * The smallest request would be "GET / HTTP/1.0".
	 */

	limit = sizeof("GET / HTTP/1.0") - 1;

	if (len < limit)
		return FALSE;

	/*
	 * Scan backwards, until we find the first space with the last trailing
	 * chars.  If we don't, it can't be an HTTP request.
	 */

	for (p = request + len - 1, i = 0; i < limit; p--, i++) {
		gint c = *p;

		if (c == ' ')		/* Not isspace(), looking for space only */
			break;
	}

	if (i == limit)
		return FALSE;		/* Reached our limit without finding a space */

	/*
	 * Here, `p' point to the space character.
	 */

	g_assert(*p == ' ');

	if (2 != sscanf(p+1, "HTTP/%d.%d", major, minor))
		return FALSE;

	/*
	 * We don't check trailing chars after the HTTP/x.x indication.
	 * There should not be any, but even if there are, we'll just ignore them.
	 */

	return TRUE;			/* Parsed HTTP/x.x OK */
}

/*
 * upload_create
 *
 * Create a new upload structure, linked to a socket.
 */
static struct upload *upload_create(struct gnutella_socket *s, gboolean push)
{
	struct upload *u;

	u = (struct upload *) g_malloc0(sizeof(struct upload));

	u->socket = s;
	s->resource.upload = u;

	u->push = push;
	u->status = push ? GTA_UL_PUSH_RECEIVED : GTA_UL_HEADERS;
	u->last_update = time((time_t *) 0);
	u->file_desc = -1;

	/*
	 * Record pending upload in the GUI.
	 */

	registered_uploads++;
	gui_update_c_uploads();

	/*
	 * Add the upload structure to the upload slist, so it's monitored
	 * from now on within the main loop for timeouts.
	 */

	uploads = g_slist_append(uploads, u);

	return u;
}

/*
 * handle_push_request
 *
 * Called when we receive a Push request on Gnet.
 *
 * If it is not for us, discard it.
 * If we are the target, then connect back to the remote servent.
 */
void handle_push_request(struct gnutella_node *n)
{
	struct upload *u;
	struct gnutella_socket *s;
	struct shared_file *req_file;
	guint32 file_index;
	guint32 ip;
	guint16 port;
	guchar *info;

	if (0 != memcmp(n->data, guid, 16))		/* Servent ID matches our GUID? */
		return;								/* No: not for us */

	/*
	 * We are the target of the push.
	 */

	info = n->data + 16;					/* Start of file information */

	READ_GUINT32_LE(info, file_index);
	READ_GUINT32_BE(info + 4, ip);
	READ_GUINT16_LE(info + 8, port);

	/*
	 * Quick sanity check on file index.
	 */

	req_file = shared_file(file_index);

	if (req_file == SHARE_REBUILDING) {
		g_warning("PUSH request (hops=%d, ttl=%d) whilst rebuilding library",
			n->header.hops, n->header.ttl);
		return;		/* Sorry, race not supported for now -- RAM, 12/03/2002 */
	}

	if (req_file == NULL) {
		g_warning("PUSH request (hops=%d, ttl=%d) for invalid file index %u",
			n->header.hops, n->header.ttl, file_index);
		return;
	}

	/*
	 * XXX might be run inside corporations (private IPs), must be smarter.
	 * XXX maybe a configuration variable? --RAM, 31/12/2001
	 *
	 * Don't waste time and resources connecting to something that will fail.
	 */

	if (!check_valid_host(ip, port)) {
		g_warning("PUSH request (hops=%d, ttl=%d) from invalid address %s",
			n->header.hops, n->header.ttl, ip_port_to_gchar(ip, port));
		return;
	}

	if (dbg > 4)
		printf("PUSH (hops=%d, ttl=%d) to %s: %s\n",
			n->header.hops, n->header.ttl, ip_port_to_gchar(ip, port),
			req_file->file_name);

	/*
	 * OK, start the upload by opening a connection to the remote host.
	 */

	s = socket_connect(ip, port, GTA_TYPE_UPLOAD);
	if (!s) {
		g_warning("PUSH request (hops=%d, ttl=%d) dropped: can't connect to %s",
			n->header.hops, n->header.ttl, ip_port_to_gchar(ip, port));
		return;
	}

	u = upload_create(s, TRUE);
	u->index = file_index;
	u->name = g_strdup(req_file->file_name);

	/* Now waiting for the connection CONF -- will call upload_push_conf() */
}

void upload_real_remove(void)
{
	// XXX UNUSED
	// XXX Currently, we remove failed uploads from the list, but we should
	// XXX do as we do for downloads, and have an extra option to remove
	// XXX failed uploads immediately.	--RAM, 24/12/2001
}

static void upload_free_resources(struct upload *u)
{
	if (u->name != NULL) {
		g_free(u->name);
		u->name = NULL;
	}
	if (u->file_desc != -1) {
		close(u->file_desc);
		u->file_desc = -1;
	}
	if (u->socket != NULL) {
		g_assert(u->socket->resource.upload == u);
		socket_free(u->socket);
		u->socket = NULL;
	}
	if (u->buffer != NULL) {
		g_free(u->buffer);
		u->buffer = NULL;
	}
	if (u->io_opaque)				/* I/O data */
		io_free(u->io_opaque);
	if (u->bio != NULL) {
		bsched_source_remove(u->bio);
		u->bio = NULL;
	}
	if (u->user_agent)
		g_free(u->user_agent);
}

/*
 * send_upload_error_v
 *
 * The vectorized (message-wise) version of send_upload_error().
 */
static void send_upload_error_v(
	struct upload *u, int code, guchar *msg, va_list ap)
{
	gchar reason[1024];

	if (msg) {
		g_vsnprintf(reason, sizeof(reason), msg, ap);
		reason[sizeof(reason) - 1] = '\0';		/* May be truncated */
	} else
		reason[0] = '\0';

	if (u->error_sent) {
		g_warning("Already sent an error %d to %s, not sending %d (%s)",
			u->error_sent, ip_to_gchar(u->socket->ip), code, reason);
		return;
	}

	socket_http_error(u->socket, code, reason);
	u->error_sent = code;
}

/*
 * send_upload_error
 *
 * Send error message to requestor.
 * This can only be done once per connection.
 */
static void send_upload_error(struct upload *u, int code, guchar *msg, ...)
{
	va_list args;

	va_start(args, msg);
	send_upload_error_v(u, code, msg, args);
	va_end(args);
}

/*
 * upload_remove_v
 *
 * The vectorized (message-wise) version of upload_remove().
 */
static void upload_remove_v(struct upload *u, const gchar *reason, va_list ap)
{
	gint row;
	gchar errbuf[1024];

	if (reason) {
		g_vsnprintf(errbuf, sizeof(errbuf), reason, ap);
		errbuf[sizeof(errbuf) - 1] = '\0';		/* May be truncated */

		if (dbg > 1) {
			if (u->name) {
				if (dbg) printf("Cancelling upload for %s from %s: %s\n",
					u->name, ip_to_gchar(u->socket->ip), errbuf);
			} else {
				if (dbg) printf("Cancelling upload from %s: %s\n",
					ip_to_gchar(u->socket->ip), errbuf);
			}
		}
	} else
		errbuf[0] = '\0';

	/*
	 * If the upload is still connecting, we have not started sending
	 * any data yet, so we send an HTTP error code before closing the
	 * connection.
	 *		--RAM, 24/12/2001
	 *
	 * Push requests still connecting don't have anything to send, hence
	 * we check explicitely for GTA_UL_PUSH_RECEIVED.
	 *		--RAM, 31/12/2001
	 */

	if (
		UPLOAD_IS_CONNECTING(u) &&
		!u->error_sent &&
		u->status != GTA_UL_PUSH_RECEIVED
	)
		send_upload_error(u, 400, reason ? errbuf : "Bad Request");

	/*
	 * If COMPLETE, we've already decremented `running_uploads' and
	 * `registered_uploads'.
	 * Moreover, if it's still connecting, then we've not even
	 * incremented the `running_uploads' counter yet.
	 */

	if (!UPLOAD_IS_COMPLETE(u))
		registered_uploads--;

	if (!UPLOAD_IS_COMPLETE(u) && !UPLOAD_IS_CONNECTING(u))
		running_uploads--;

	/*
	 * If we were sending data, and we have not accounted the download yet,
	 * then update the stats, not marking the upload as completed.
	 */

	if (UPLOAD_IS_SENDING(u) && !u->accounted)
		ul_stats_file_aborted(u);

	upload_free_resources(u);
	g_free(u);

	/*
	 * `registered_uploads', and possibly `running_uploads' may have changed
	 */
	gui_update_c_uploads();

	row = gtk_clist_find_row_from_data(GTK_CLIST(clist_uploads), (gpointer) u);
	gtk_clist_remove(GTK_CLIST(clist_uploads), row);
	uploads = g_slist_remove(uploads, (gpointer) u);
}

/*
 * upload_remove
 *
 * Remove upload entry, log reason.
 *
 * If no status has been sent back on the HTTP stream yet, give them
 * a 400 error with the reason.
 */
void upload_remove(struct upload *u, const gchar *reason, ...)
{
	va_list args;

	va_start(args, reason);
	upload_remove_v(u, reason, args);
	va_end(args);
}

/*
 * upload_error_remove
 *
 * Utility routine.  Cancel the upload, sending back the HTTP eerror message.
 */
static void upload_error_remove(struct upload *u, int code, guchar *msg, ...)
{
	va_list args;

	va_start(args, msg);
	send_upload_error_v(u, code, msg, args);
	upload_remove_v(u, msg, args);
	va_end(args);
}

/***
 *** Header parsing callbacks
 ***
 *** We could call those directly, but I'm thinking about factoring all
 *** that processing into a generic set of functions, and the processing
 *** callbacks will all have the same signature.  --RAM, 31/12/2001
 ***/

static void call_upload_request(struct io_header *ih)
{
	upload_request(ih->upload, ih->header);
}

/*
 * upload_header_parse
 *
 * This routine is called to parse the input buffer, a line at a time,
 * until EOH is reached.
 */
static void upload_header_parse(struct io_header *ih)
{
	struct upload *u = ih->upload;
	struct gnutella_socket *s = u->socket;
	getline_t *getline = ih->getline;
	header_t *header = ih->header;
	guint parsed;
	gint error;

	/*
	 * Read header a line at a time.  We have exacly s->pos chars to handle.
	 * NB: we're using a goto label to loop over.
	 */

nextline:
	switch (getline_read(getline, s->buffer, s->pos, &parsed)) {
	case READ_OVERFLOW:
		g_warning("upload_header_parse: line too long, disconnecting from %s",
			ip_to_gchar(s->ip));
		dump_hex(stderr, "Leading Data", s->buffer, MIN(s->pos, 256));
		upload_error_remove(u, 413, "Header too large");
		return;
		/* NOTREACHED */
	case READ_DONE:
		if (s->pos != parsed)
			memmove(s->buffer, s->buffer + parsed, s->pos - parsed);
		s->pos -= parsed;
		break;
	case READ_MORE:		/* ok, but needs more data */
	default:
		g_assert(parsed == s->pos);
		s->pos = 0;
		return;
	}

	/*
	 * We come here everytime we get a full header line.
	 */

	if (ih->flags & IO_STATUS_LINE) {
		/*
		 * Save status line away in socket's "getline" object, then clear
		 * the fact that we're expecting a status line and continue to get
		 * the following header lines.
		 *
		 * XXX Refactoring note: it's not a status line here, it's the HTTP
		 * XXX request, which we get from the remote servent through a
		 * XXX connection we initiated with the GIV string (PUSH reply).
		 * XXX The flag must be renamed, kept it as-is for now to not add
		 * XXX futher factoring difficulty.
		 */

		g_assert(s->getline == 0);
		s->getline = getline_make();

		getline_copy(getline, s->getline);
		getline_reset(getline);
		ih->flags &= ~IO_STATUS_LINE;
		goto nextline;
	}

	error = header_append(header,
		getline_str(getline), getline_length(getline));

	switch (error) {
	case HEAD_OK:
		getline_reset(getline);
		goto nextline;			/* Go process other lines we may have read */
		/* NOTREACHED */
	case HEAD_EOH:				/* We reached the end of the header */
		break;
	case HEAD_TOO_LARGE:
	case HEAD_MANY_LINES:
		send_upload_error(u, 413, header_strerror(error));
		/* FALL THROUGH */
	case HEAD_EOH_REACHED:
		g_warning("upload_header_parse: %s, disconnecting from %s",
			header_strerror(error),  ip_to_gchar(s->ip));
		fprintf(stderr, "------ Header Dump:\n");
		header_dump(header, stderr);
		fprintf(stderr, "------\n");
		dump_hex(stderr, "Header Line", getline_str(getline),
			MIN(getline_length(getline), 128));
		upload_remove(u, "Failed (%s)", header_strerror(error));
		return;
		/* NOTREACHED */
	default:					/* Error, but try to continue */
		g_warning("upload_header_parse: %s, from %s",
			header_strerror(error), ip_to_gchar(s->ip));
		dump_hex(stderr, "Header Line",
			getline_str(getline), getline_length(getline));
		getline_reset(getline);
		goto nextline;			/* Go process other lines we may have read */
	}

	/*
	 * We reached the end of headers.  Make sure there's no more data.
	 */

	if (s->pos) {
		g_warning("remote %s sent extra bytes after HTTP headers",
			ip_to_gchar(s->ip));
		dump_hex(stderr, "Extra Data", s->buffer, MIN(s->pos, 256));
		upload_remove(u, "Failed (Extra HTTP header data)");
		return;
	}

	/*
	 * OK, we got the whole headers.
	 *
	 * Free up the I/O callback structure, remove the input callback and
	 * initialize the upload.  NB: the initial HTTP request on the first
	 * line is still held in the s->getline structure.
	 */

	gdk_input_remove(s->gdk_tag);
	s->gdk_tag = 0;

	ih->process_header(ih);
}

/*
 * upload_header_read
 *
 * This routine is installed as an input callback to read the HTTP headers
 * of the request.
 */
static void upload_header_read(
	gpointer data, gint source, GdkInputCondition cond)
{
	struct io_header *ih = (struct io_header *) data;
	struct upload *u = ih->upload;
	struct gnutella_socket *s = u->socket;
	guint count;
	gint r;

	if (cond & GDK_INPUT_EXCEPTION) {
		upload_remove(u, "Failed (Input Exception)");
		return;
	}

	count = sizeof(s->buffer) - s->pos - 1;		/* -1 to allow trailing NUL */
	if (count <= 0) {
		g_warning("upload_header_read: incoming buffer full, "
			"disconnecting from %s", ip_to_gchar(s->ip));
		dump_hex(stderr, "Leading Data", s->buffer, MIN(s->pos, 256));
		upload_remove(u, "Failed (Input buffer full)");
		return;
	}

	r = bws_read(bws.in, s->file_desc, s->buffer + s->pos, count);
	if (r == 0) {
		upload_remove(u, "Failed (EOF)");
		return;
	} else if (r < 0 && errno == EAGAIN)
		return;
	else if (r < 0) {
		upload_remove(u, "Failed (Input error: %s)", g_strerror(errno));
		return;
	}

	/*
	 * During the header reading phase, we don't update "u->last_update"
	 * on purpose.  The timeout is defined for the whole connection phase,
	 * i.e. until we read the end of the headers.
	 */

	s->pos += r;

	upload_header_parse(ih);
}

/*
 * upload_add
 *
 * Create a new upload request, and begin reading HTTP headers.
 */
void upload_add(struct gnutella_socket *s)
{
	struct upload *u;
	struct io_header *ih;

	u = upload_create(s, FALSE);

	s->type = GTA_TYPE_UPLOAD;

	/*
	 * Prepare callback argument used during the header reading phase.
	 */

	ih = (struct io_header *) g_malloc(sizeof(struct io_header));
	ih->upload = u;
	ih->header = header_make();
	ih->getline = getline_make();
	ih->flags = 0;
	ih->process_header = call_upload_request;
	u->io_opaque = (gpointer) ih;

	g_assert(s->gdk_tag == 0);

	s->gdk_tag = gdk_input_add(s->file_desc,
		(GdkInputCondition) GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
		upload_header_read, (gpointer) ih);

	/*
	 * There may be pending input in the socket buffer, so go handle
	 * it immediately.
	 */

	upload_header_parse(ih);
}

/*
 * upload_push_conf
 *
 * Got confirmation that the connection to the remote host was OK.
 * Send the GIV string, then prepare receiving back the HTTP request.
 */
void upload_push_conf(struct upload *u)
{
	gchar giv[MAX_LINE_SIZE];
	struct gnutella_socket *s;
	struct io_header *ih;
	gint rw;
	gint sent;

	g_assert(u);
	g_assert(u->name);

	/*
	 * Send the GIV string, using our servent GUID.
	 */

	rw = g_snprintf(giv, sizeof(giv), "GIV %u:%s/%s\n\n",
		u->index, guid_hex_str(guid), u->name);
	giv[sizeof(giv)-1] = '\0';			/* Might have been truncated */
	
	s = u->socket;
	if (-1 == (sent = bws_write(bws.out, s->file_desc, giv, rw))) {
		g_warning("Unable to send back GIV for \"%s\" to %s: %s",
			u->name, ip_to_gchar(s->ip), g_strerror(errno));
	} else if (sent < rw) {
		g_warning("Only sent %d out of %d bytes of GIV for \"%s\" to %s: %s",
			sent, rw, u->name, ip_to_gchar(s->ip), g_strerror(errno));
	} else if (dbg > 4) {
		printf("----Sent GIV to %s:\n%.*s----\n", ip_to_gchar(s->ip), rw, giv);
		fflush(stdout);
	}

	if (sent != rw) {
		upload_remove(u, "Unable to send GIV");
		return;
	}

	/*
	 * Prepare callback argument used during the header reading phase.
	 *
	 * We're requesting the reading of a "status line", which will be the
	 * HTTP request.  It will be stored in a created s->getline entry.
	 * Once we're done, we'll end-up in upload_request(): the path joins
	 * with the one used for direct uploading.
	 */

	g_assert(s->getline == 0);

	ih = (struct io_header *) g_malloc(sizeof(struct io_header));
	ih->upload = u;
	ih->header = header_make();
	ih->getline = getline_make();
	ih->flags = IO_STATUS_LINE;		/* XXX will be really the HTTP request */
	ih->process_header = call_upload_request;
	u->io_opaque = (gpointer) ih;

	g_assert(s->gdk_tag == 0);

	s->gdk_tag = gdk_input_add(s->file_desc,
		(GdkInputCondition) GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
		upload_header_read, (gpointer) ih);
}

/*
 * upload_error_not_found
 * 
 * Send back an HTTP error 404: file not found,
 */
static void upload_error_not_found(struct upload *u, const gchar *request)
{
	g_warning("bad request from %s: %s\n", ip_to_gchar(u->socket->ip), request);
	upload_error_remove(u, 404, "Not Found");
}

/* 
 * upload_error_request_is_ok
 * 
 * Check the request.
 * Return TRUE if ok or FALSE otherwise (upload must then be aborted)
 */
static gboolean upload_request_is_ok(
	struct upload *u,
	header_t *header,
	gchar *request)
{
	struct gnutella_socket *s = u->socket;
	gint http_major, http_minor;

	/*
	 * Check HTTP protocol version. --RAM, 11/04/2002
	 */

	if (
		!extract_http_version(request, getline_length(s->getline),
			&http_major, &http_minor)
	) {
		upload_error_remove(u, 500, "Unknown/Missing Protocol Tag");
		return FALSE;
	}

	if (http_major != 1) {
		upload_error_remove(u, 505,
			"HTTP Version %d Not Supported", http_major);
		return FALSE;
	}

	/*
	 * If HTTP/1.1 or above, check the Host header.
	 *
	 * We require it because HTTP does, but we don't really care for
	 * now.  Moreover, we might not know our external IP correctly,
	 * so we have little ways to check that the Host refers to us.
	 *
	 *		--RAM, 11/04/2002
	 */

	if (http_minor >= 1) {
		gchar *host = header_get(header, "Host");

		if (host == NULL) {
			upload_error_remove(u, 400, "Missing Host Header");
			return FALSE;
		}
	}

	/*
	 * If we don't share, abort immediately. --RAM, 11/01/2002
	 * Use 5xx error code, it's a server-side problem --RAM, 11/04/2002
	 */

	if (max_uploads == 0) {
		upload_error_remove(u, 503, "Sharing currently disabled");
		return FALSE;
	}

	return TRUE;
}

/* 
 * get_file_to_upload_from_index
 * 
 * Get the shared_file to upload. Request has been extracted already, and is
 * passed as request. The same holds for the file index, which is passed as
 * index.
 * Return the shared_file if found, NULL otherwise.
 */

static struct shared_file *get_file_to_upload_from_index(
	struct upload *u,
	gchar *request,
	guint index)
{
	guchar c;
	gchar *buf;

	/*
	 * We must be cautious about file index changing between two scans,
	 * which may happen when files are moved around on the local library.
	 * If we serve the wrong file, and it's a resuming request, this will
	 * result in a corrupted file!
	 *		--RAM, 26/09/2001
	 *
	 * We now support URL-escaped queries.
	 *		--RAM, 16/01/2002
	 */

	struct shared_file *reqfile = shared_file(index);

	if (reqfile == SHARE_REBUILDING) {
		/* Retry-able by user, hence 503 */
		upload_error_remove(u, 503, "Library being rebuilt");
		return NULL;
	}

	if (reqfile == NULL) {
		upload_error_not_found(u, request);
		return NULL;
	}

	buf = request +
		((request[0] == 'G') ? sizeof("GET /get/") : sizeof("HEAD /get/"));

	(void) url_unescape(buf, TRUE);		/* Index is escape-safe anyway */

	while ((c = *(guchar *) buf++) && c != '/')
		/* empty */;

	if (
		c == '/' &&
		0 != strncmp(buf, reqfile->file_name, reqfile->file_name_len)
	) {
		gchar *error = "File index/name mismatch";
		g_warning("%s from %s for %d/%s", error,
			ip_to_gchar(u->socket->ip), index, reqfile->file_name);
		upload_error_remove(u, 409, error);
		return NULL;
	}

	return reqfile;
}

static const char sha1_query[] = "GET /uri-res/N2R?urn:sha1:";
static const int sha1_query_length = sizeof(sha1_query) - 1;

/* 
 * get_file_to_upload_from_sha1
 * 
 * Get the shared_file to upload from a given SHA1 hash. request is the
 * base32-encoded SHA1 hash.
 * Return the shared_file if we have it, NULL otherwise
 */
static struct shared_file *get_file_to_upload_from_sha1(const gchar *request)
{
	char hash[SHA1_BASE32_SIZE + 2];

	sscanf(request + sha1_query_length, "%32s", hash);

	return shared_file_from_sha1_hash(hash);
}

/*
 * get_file_to_upload
 * 
 * A dispatcher function to call either get_file_to_upload_from_index or
 * get_file_to_upload_from_sha1 depending on the syntax of the request.
 * Return the shared_file if we got it, or NULL otherwise.
 */
static struct shared_file *get_file_to_upload(struct upload *u, gchar *request)
{
	guint index = 0;

	if (
		sscanf(request, "GET /get/%u/", &index) ||
		sscanf(request, "HEAD /get/%u/", &index)
	)
		return get_file_to_upload_from_index(u, request, index);
	else if (!strncmp(request, sha1_query, sha1_query_length))
		return get_file_to_upload_from_sha1(request);

	upload_error_not_found(u, request);
	return NULL;
}

/*
 * upload_request
 *
 * Called to initiate the upload once all the HTTP headers have been
 * read.  Validate the request, and begin processing it if all OK.
 * Otherwise cancel the upload.
 */
static void upload_request(struct upload *u, header_t *header)
{
	struct gnutella_socket *s = u->socket;
	struct shared_file *reqfile = NULL;
	guint index = 0, skip = 0, end = 0, rw = 0, row = 0, upcount = 0;
	gchar http_response[1024], *fpath = NULL, sl[] = "/";
	gchar *user_agent = 0;
	gchar *buf;
	gchar *titles[6];
	gchar *request = getline_str(s->getline);
	GSList *l;
	gint sent;
	gboolean head_only;
	gboolean has_end = FALSE;
	gchar size_tmp[256];
	gchar range_tmp[256];
	gint range_len;
	gint needed_room;
	struct stat statbuf;
	gboolean partial;
	time_t mtime, now;

	titles[0] = titles[1] = titles[2] = titles[3] = titles[4] = NULL;

	if (dbg > 4) {
		printf("----Incoming Request from %s:\n", ip_to_gchar(s->ip));
		printf("%s\n", request);
		header_dump(header, stdout);
		printf("----\n");
		fflush(stdout);
	}

	/*
	 * Technically, we have not started sending anything yet, but this
	 * also serves as a marker in case we need to call upload_remove().
	 * It will not send an HTTP reply by itself.
	 */

	u->status = GTA_UL_SENDING;
	u->last_update = time((time_t *) 0);	/* Done reading headers */

	/*
	 * If we remove the upload in upload_remove(), we'll decrement
	 * running_uploads.
	 */

	running_uploads++;

	/*
	 * If `head_only' is true, the request was a HEAD and we're only going
	 * to send back the headers.
	 */

	head_only = (request[0] == 'H');

	/*
	 * IDEA
	 *
	 * To prevent people from hammering us, we should setup a priority queue
	 * coupled to a hash table for fast lookups, where we would record the
	 * last failed attempt and when it was.	As soon as there is a request,
	 * we would move the record for the IP address at the beginning of the
	 * queue, and drop the tail when we reach our size limit.
	 *
	 * Then, if we discover that a given IP re-issues too frequent requests,
	 * we would start differing our reply by not sending the error immediately
	 * but scheduling that some time in the future.	We would begin to use
	 * many file descriptors that way, so we trade CPU time for another scarce
	 * resource.  However, if someone is hammering us with connections,
	 * he would have to wait for our reply before knowing the failure, and
	 * it would slow him down, even if he retried immediately.
	 *
	 * Alternatively, instead of differing the 503 reply, we could send a
	 * "403 Forbidden to bad citizens" instead, and chances are that servents
	 * abort retries on failures other than 503...
	 *
	 *				--RAM, 09/09/2001
	 */

	reqfile = get_file_to_upload(u, request);

	if (!reqfile) {
		/* get_file_to_upload is supposed to have signaled the error already */
		return;
	}

	index = reqfile->file_index;

	if (!upload_request_is_ok(u, header, request))
		return;

	/*
	 * Even though this test is less costly than the previous, doing it
	 * afterwards allows them to be notified of a mismatch whilst they
	 * wait for a download slot.  It would be a pity for them to get
	 * a slot and be told about the mismatch only then.
	 *		--RAM, 15/12/2001
	 */

	if (running_uploads > max_uploads) {
		upload_error_remove(u,
			503, "Too many uploads (%d max)", max_uploads);
		return;
	}

	/*
	 * Ensure that noone tries to download the same file twice, and
	 * that they don't get beyond the max authorized downloads per IP.
	 */

	for (l = uploads; l; l = l->next) {
		struct upload *up = (struct upload *) (l->data);
		g_assert(up);
		if (up == u)
			continue;				/* Current upload is already in list */
		if (!UPLOAD_IS_SENDING(up))
			continue;
		if (up->index == index && up->socket->ip == s->ip) {
			upload_error_remove(u, 409, "Already downloading that file");
			return;
		}
		if (up->socket->ip == s->ip && ++upcount >= max_uploads_ip) {
			upload_error_remove(u,
				503, "Only %u download%s per IP address",
				max_uploads_ip, max_uploads_ip == 1 ? "" : "s");
			return;
		}
	}

	/*
	 * Range: bytes=10453-23456
	 * User-Agent: whatever
	 * Server: whatever (in case no User-Agent)
	 */

	buf = header_get(header, "Range");
	if (buf) {
		if (strchr(buf, ',')) {
			char *msg = "Multiple Range requests unsupported";
			send_upload_error(u, 400, msg);
			upload_remove(u, msg);
			return;
		} else if (2 == sscanf(buf, "bytes=%u-%u", &skip, &end)) {
			has_end = TRUE;
			if (skip > end) {
				upload_error_remove(u, 400, "Malformed Range request");
				return;
			}
		} else if (1 == sscanf(buf, "bytes=-%u", &skip)) {
			/*
			 * Backwards specification -- they want latest `skip' bytes.
			 */
			if (skip >= reqfile->file_size)
				skip = 0;
			else
				skip = reqfile->file_size - skip;
		} else
			(void) sscanf(buf, "bytes=%u-", &skip);
	}

	user_agent = header_get(header, "User-Agent");

	/* Maybe they sent a Server: line, thinking they're a server? */
	if (!user_agent)
		user_agent = header_get(header, "Server");

	if (user_agent) {
		/* XXX match against web user agents, possibly */
	}

	/*
	 * We're accepting the upload.  Setup accordingly.
	 */

	/* Set the full path to the file */

	if (reqfile->file_directory[strlen(reqfile->file_directory) - 1] == sl[0])
		fpath = g_strconcat(reqfile->file_directory, reqfile->file_name, NULL);
	else
		fpath = g_strconcat(reqfile->file_directory,
			sl, reqfile->file_name, NULL);

	/*
	 * Validate the rquested range.
	 */

	u->file_size = reqfile->file_size;

	if (!has_end)
		end = u->file_size - 1;

	if (skip >= u->file_size || end >= u->file_size) {
		upload_error_remove(u, 416, "Requested range not satisfiable");
		return;
	}

	if (-1 == stat(fpath, &statbuf)) {
		upload_error_not_found(u, request);
		g_free(fpath);
		return;
	}

	/* Open the file for reading , READONLY just in case. */
	if ((u->file_desc = open(fpath, O_RDONLY)) < 0) {
		upload_error_not_found(u, request);
		g_free(fpath);
		return;
	}

	g_free(fpath);

	/*
	 * If we pushed this upload, and they are not requesting the same
	 * file, that's OK, but warn.
	 *		--RAM, 31/12/2001
	 */

	if (u->push && index != u->index)
		g_warning("Host %s sent PUSH for %u (%s), now requesting %u (%s)",
			ip_to_gchar(s->ip),
			u->index, u->name, index, reqfile->file_name);

	/*
	 * Set all the upload information
	 *
	 * When comming from a push request, we already have a non-NULL
	 * u->name in the structure.  However, even if the index is the same,
	 * it's not a 100% certainety that the filename matches (the library
	 * could have been rebuilt).  Most of the time, it will, so it
	 * pays to strcmp() it.
	 *		--RAM, 25/03/2002, memory leak found by Michael Tesch
	 */

	u->index = index;

	if (u->push && 0 != strcmp(u->name, reqfile->file_name)) {
		g_free(u->name);
		u->name = NULL;
	}

	if (u->name == NULL)
		u->name = g_strdup(reqfile->file_name);

	if (user_agent)
		u->user_agent = g_strdup(user_agent);

	u->skip = skip;
	u->end = end;
	u->pos = 0;
	u->start_date = time((time_t *) NULL);
	u->last_update = time((time_t *) 0);

	u->buf_size = 4096 * sizeof(gchar);
	u->buffer = (gchar *) g_malloc(u->buf_size);
	u->bpos = 0;
	u->bsize = 0;

	/*
	 * Prepare date and modification time of file.
	 */

	now = time((time_t *) NULL);
	mtime = statbuf.st_mtime;
	if (mtime > now)
		mtime = now;			/* Clock skew on file server */

	/*
	 * On linux, turn TCP_CORK on so that we only send out full TCP/IP
	 * frames.  The exact size depends on your LAN interface, but on
	 * Ethernet, it's about 1500 bytes.
	 */

	sock_cork(s, TRUE);

	/*
	 * Setup and write the HTTP 200 header , including the file size.
	 * If partial content (range request), emit a 206 reply.
	 */

	partial = skip || end != (u->file_size - 1);
	if (partial)
		rw = g_snprintf(http_response, sizeof(http_response),
			  "HTTP/1.0 206 Partial Content\r\n");
	else
		rw = g_snprintf(http_response, sizeof(http_response),
			  "HTTP/1.0 200 OK\r\n");
	
	rw += g_snprintf(http_response + rw, sizeof(http_response) - rw,
		"Server: %s\r\n"
		"X-Live-Since: %s\r\n"
		"Connection: close\r\n"
		"Date: %s\r\n"
		"Last-Modified: %s\r\n"
		"Content-Type: application/binary\r\n"
		"Content-Length: %u\r\n",
			version_string, start_rfc822_date,
			date_to_rfc822_gchar(now), date_to_rfc822_gchar2(mtime),
			u->end - u->skip + 1);

	if (partial)
	  rw += g_snprintf(http_response + rw, sizeof(http_response) - rw,
		"Content-Range: bytes %u-%u/%u\r\n", u->skip, u->end, u->file_size);

	/*
	 * Propagate the SHA1 information for the file, if we have it.
	 */

	needed_room = 33 + SHA1_BASE32_SIZE + 2; /* Header + base32 SHA1 + crlf */

	if (
		sizeof(http_response) - rw > needed_room &&
		sha1_hash_available(reqfile)
	)
		rw += g_snprintf(http_response + rw, sizeof(http_response) - rw,
			"X-Gnutella-Content-URN: urn:sha1:%.*s\r\n",
			SHA1_BASE32_SIZE, reqfile->sha1_digest);

	rw += g_snprintf(http_response + rw, sizeof(http_response) - rw,
			 "\r\n");

	sent = bws_write(bws.out, s->file_desc, http_response, rw);
	if (sent == -1) {
		gint errcode = errno;
		g_warning("Unable to send back HTTP OK reply to %s: %s",
			ip_to_gchar(s->ip), g_strerror(errcode));
		upload_remove(u, "Cannot send ACK: %s", g_strerror(errcode));
		return;
	} else if (sent < rw) {
		g_warning("Could only send %d out of %d HTTP OK bytes to %s",
			sent, rw, ip_to_gchar(s->ip));
		upload_remove(u, "Cannot send whole ACK");
		return;
	}

	if (dbg > 4) {
		printf("----Sent Reply to %s:\n%.*s----\n",
			ip_to_gchar(s->ip), (int) rw, http_response);
		fflush(stdout);
	}

	/*
	 * If we need to send only the HEAD, we're done. --RAM, 26/12/2001
	 */

	if (head_only) {
		upload_remove(u, NULL);		/* No message, everything was OK */
		return;
	}

	/*
	 * Install the output I/O, which is via a bandwidth limited source.
	 */

	io_free(u->io_opaque);

	g_assert(s->gdk_tag == 0);
	g_assert(u->bio == NULL);
	
	u->bio = bsched_source_add(bws.out, s->file_desc,
		BIO_F_WRITE, upload_write, (gpointer) u);

	/*
	 * Add upload to the GUI
	 */

	g_snprintf(size_tmp, sizeof(size_tmp), "%s", short_size(u->file_size));

	range_len = g_snprintf(range_tmp, sizeof(range_tmp), "%s",
		compact_size(u->end - u->skip + 1));

	if (u->skip)
		range_len += g_snprintf(
			&range_tmp[range_len], sizeof(range_tmp)-range_len,
			" @ %s", compact_size(u->skip));

	titles[c_ul_filename] = u->name;
	titles[c_ul_host] = ip_to_gchar(s->ip);
	titles[c_ul_size] = size_tmp;
	titles[c_ul_range] = range_tmp;
    titles[c_ul_agent] = (u->user_agent != NULL) ? u->user_agent : "";
	titles[c_ul_status] = "";

	row = gtk_clist_append(GTK_CLIST(clist_uploads), titles);
	gtk_clist_set_row_data(GTK_CLIST(clist_uploads), row, (gpointer) u);

	gui_update_c_uploads();
	ul_stats_file_begin(u);
}

/* Uplaod Write
 * FIFO type action to deal with low baud rates. If we try to force
 * 4k then the lower speed downloads will be garbled.
 */
void upload_write(gpointer up, gint source, GdkInputCondition cond)
{
	struct upload *u = (struct upload *) up;
	guint32 write_bytes;
	guint32 amount;
	guint32 available;

	if (!(cond & GDK_INPUT_WRITE)) {
		/* If we can't write then we don't want it, kill the socket */
		if (dbg)
			printf("upload_write(); Condition %i, Exception = %i\n",
				   cond, GDK_INPUT_EXCEPTION);
		upload_remove(u, "Write exception");
		return;
	}

#ifdef HAVE_SENDFILE_H

	/* If we got a valid skip amount then jump ahead to that position */
	if (u->pos == 0 && u->skip > 0)
		u->pos = u->skip;

	/*
	 * Compute the amount of bytes to send.
	 * Use the two variables to avoid warnings about unused vars by compiler.
	 */

	amount = u->end - u->pos + 1;
	available = amount > u->buf_size ? u->buf_size : amount;

	write_bytes = bio_sendfile(u->bio, u->file_desc, &u->pos, available);

#else	/* !HAVE_SENDFILE_H */

	/* If we got a valid skip amount then jump ahead to that position */
	if (u->pos == 0 && u->skip > 0) {
		if (lseek(u->file_desc, u->skip, SEEK_SET) == -1) {
			upload_remove(u, "File seek error: %s", g_strerror(errno));
			return;
		}
		u->pos = u->skip;
	}

	/*
	 * Compute the amount of bytes to send.
	 */

	amount = u->end - u->pos + 1;

	/*
	 * If the buffer position reached the size, then we need to read
	 * more data from the file. We read in under or equal to the buffer
	 * memory size
	 */

	if (u->bpos == u->bsize) {
		if ((u->bsize = read(u->file_desc, u->buffer, u->buf_size)) == -1) {
			upload_remove(u, "File read error: %s", g_strerror(errno));
			return;
		}
		if (u->bsize == 0) {
			upload_remove(u, "File EOF?");
			return;
		}
		u->bpos = 0;
	}

	available = u->bsize - u->bpos;
	if (available > amount)
		available = amount;

	g_assert(available > 0);

	write_bytes = bio_write(u->bio, &u->buffer[u->bpos], available);

#endif	/* HAVE_SENDFILE_H */

	if (write_bytes == -1) {
		if (errno != EAGAIN)
			upload_remove(u, "Data write error: %s", g_strerror(errno));
		return;
	}

#ifndef HAVE_SENDFILE_H
	/*
	 * Only required when not using sendfile(), otherwise the u->pos field
	 * is directly updated by the kernel, and u->bpos is unused.
	 *		--RAM, 21/02/2002
	 */

	u->pos += write_bytes;
	u->bpos += write_bytes;
#endif

	u->last_update = time((time_t *) NULL);

	/* This upload is complete */
	if (u->pos > u->end) {
		count_uploads++;
		gui_update_count_uploads();
		gui_update_c_uploads();
		ul_stats_file_complete(u);
		u->accounted = TRUE;			/* Called ul_stats_file_complete() */

		if (clear_uploads == TRUE)
			upload_remove(u, NULL);
		else {
			u->status = GTA_UL_COMPLETE;
			gui_update_upload(u);
			registered_uploads--;
			running_uploads--;
			gui_update_c_uploads();
			gtk_widget_set_sensitive(button_uploads_clear_completed, 1);
			upload_free_resources(u);
		}
		return;
	}
}

void upload_close(void)
{
	GSList *l;

	for (l = uploads; l; l = l->next) {
		struct upload *u = (struct upload *) l->data;
		if (UPLOAD_IS_SENDING(u) && !u->accounted)
			ul_stats_file_aborted(u);
		upload_free_resources(u);
		g_free(u);
	}

	g_slist_free(uploads);
}


/* 
 * Emacs stuff:
 * Local Variables: ***
 * c-indentation-style: "bsd" ***
 * fill-column: 80 ***
 * tab-width: 4 ***
 * indent-tabs-mode: nil ***
 * End: ***
 */
/* vi: set ts=4: */
