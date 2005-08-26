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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */


#ifndef __DDCC_APPLET_H__
#define __DDCC_APPLET_H__


/* ****************
 * Datatypes
 * ****************/

typedef struct
_DdccApplet
{
	GtkWidget* w_applet;
	GtkWidget* w_label;
	GtkWidget* w_profiles_menu;

	struct monitor* monitor;
	char monitor_name[256];
} DdccApplet;


/* ****************
 * Protoypes
 * ****************/

void
build_profiles_menu (DdccApplet *applet);


#endif /* __DDCC_APPLET_H__ */
