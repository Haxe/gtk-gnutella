/*
 * Copyright (c) 2009, Raphael Manfredi
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
 * @file
 *
 * Gtk-gnutella version and other configuration parameters.
 *
 * @author Raphael Manfredi
 * @date 2009
 */

#ifndef _gtk_gnutella_h_
#define _gtk_gnutella_h_

/*
 * Constants
 */

#define GTA_PRODUCT_NAME "gtk-gnutella"	/**< Normally "gtk-gnutella" */
#define GTA_VERSION 0				/**< major version */
#define GTA_SUBVERSION 97			/**< minor version */
#define GTA_PATCHLEVEL 0			/**< patch level or teeny version */
#define GTA_REVISION "stable"		/**< unstable, beta, stable */
#define GTA_REVCHAR ""				/**< (u)nstable, (b)eta, none -> stable */
#define GTA_RELEASE "2011-07-18"	/**< ISO 8601 format YYYY-MM-DD */
#define GTA_WEBSITE "http://gtk-gnutella.sourceforge.net/"

#if defined(USE_GTK1)
#define GTA_INTERFACE "GTK1"
#elif defined(USE_GTK2)
#define GTA_INTERFACE "GTK2"
#elif defined(USE_TOPLESS)
#define GTA_INTERFACE "Topless"
#else
#error "Expected Gtk+ 1.2, Gtk+ 2.x or headless as user-interface."
#endif

#if defined(GTA_PATCHLEVEL) && (GTA_PATCHLEVEL != 0)
#define GTA_VERSION_NUMBER \
	STRINGIFY(GTA_VERSION) "." \
	STRINGIFY(GTA_SUBVERSION) "." \
	STRINGIFY(GTA_PATCHLEVEL) GTA_REVCHAR
#else
#define GTA_VERSION_NUMBER \
	STRINGIFY(GTA_VERSION) "." STRINGIFY(GTA_SUBVERSION) GTA_REVCHAR
#endif

#define GTA_PORT			6346	/**< Default "standard" port */

#ifndef GTA_BUILD
#define GTA_BUILD					"$Revision$"
#endif

#endif	/* _gtk_gnutella_h_ */

/* vi: set ts=4: */
