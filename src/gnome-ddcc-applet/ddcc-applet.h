/* ddcc-applet
 * Copyright (C) 2005 Christian Schilling
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */


#ifndef __DDCC_APPLET_H__
#define __DDCC_APPLET_H__


/* ****************
 * Constants
 * ****************/

/* Errorlevels to indicate how far the initialisation has succeded
 * and what still needs to be done */
typedef enum
_DdccError
{
	ERR_OK = 0,
	ERR_FILL_PROFILES_MENU,
	ERR_DDCCI_OPEN,
	ERR_FILL_MONITOR_COMBO,
	ERR_DDCCI_INIT,
	ERR_NO_INIT,
} DdccError;


/* ****************
 * Datatypes
 * ****************/

/* groups all data the applet together, a pointer to an
 * instance of this is passed to most funktions of the applet */
typedef struct
_DdccApplet
{
	PanelApplet* w_applet;
	GtkWidget* w_label;
	GtkWidget* w_profiles_menu;
	GtkWidget* w_properties_dialog;
	GtkWidget* w_properties_monitor;

	struct monitor* monitor;
	char monitor_name[256];
	struct profile* profile;
	
	DdccError error;
} DdccApplet;


/* ****************
 * Protoypes
 * ****************/

static void
ddcc_applet_init (DdccApplet* applet);

static void
menu_properties_cb(BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname);

static void
menu_about_cb(BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname);

static void
menu_rungdcc_cb(BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname);

#endif /* __DDCC_APPLET_H__ */
