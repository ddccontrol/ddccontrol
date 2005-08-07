/***************************************************************************
 *   Copyright (C) 2005 by Nicolas Boichat                                 *
 *   nicolas@boichat.ch                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "notebook.h"

static GtkWidget* fs_patterns_window = NULL;

static GtkWidget* table;
static GtkWidget* centervbox;
static GtkWidget* scrolled_window;

static GtkWidget* monmainvbox; /* Monitor manager main GtkVBox (containing vbox) */
static GtkWidget* vbox; /* GtkVBox containing controls */

static GtkWidget* images[4] = {NULL, NULL, NULL, NULL}; /* top-bottom-left-right images */

static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_widget_ref(vbox);
	gtk_container_remove(GTK_CONTAINER(centervbox), vbox);
	gtk_box_pack_start(GTK_BOX(monmainvbox), vbox, 0, 5, 5);
	gtk_widget_unref(vbox);
	
	gtk_widget_hide(fs_patterns_window);
}

static void create_fullscreen_patterns_window()
{
	fs_patterns_window = gtk_window_new(GTK_WINDOW_POPUP);
	
	gtk_container_set_border_width(GTK_CONTAINER(fs_patterns_window), 0);
	
	table = gtk_table_new(3, 3, FALSE);
	gtk_container_add(GTK_CONTAINER(fs_patterns_window), table);
	
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	
	centervbox = gtk_vbox_new(FALSE,10);
	
	GtkWidget* align = gtk_alignment_new(0.5, 0, 0, 0);
	GtkWidget* close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(close_button),"clicked",G_CALLBACK (destroy), NULL);
	gtk_widget_show(close_button);
	gtk_container_add(GTK_CONTAINER(align), close_button);
	gtk_widget_show(align);
	gtk_box_pack_end(GTK_BOX(centervbox), align, TRUE, 5, 5);
	gtk_widget_show(centervbox);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), centervbox);
	gtk_widget_show(scrolled_window);
}

static void drawShade(GdkDrawable* pixmap, int ry, int rh, int number) {
	GdkColor color;
	PangoLayout* layout;
	gchar* tmp;
	
	GdkGC* gc = gdk_gc_new(pixmap);
	GdkGC* gcwhite = gdk_gc_new(pixmap);
	
	int width, height;
	gdk_drawable_get_size(pixmap, &width, &height);
	
	int rx = width/12;
	int rw = (width-(rx*2))/number;
	int i;
	int w, h;
	
	color.red = color.green = color.blue = 0x8000;
	gdk_gc_set_rgb_fg_color(gcwhite, &color);
	gdk_gc_set_rgb_bg_color(gcwhite, &color);
	
	color.red = color.green = color.blue = 0x0000;
	for (i = 0; i < number; i++) {
		gdk_gc_set_rgb_fg_color(gc, &color);
		gdk_gc_set_rgb_bg_color(gc, &color);
		gdk_draw_rectangle(pixmap, gc, TRUE, rx, ry, rw, rh);
		gdk_draw_line(pixmap, gcwhite, rx, ry+rh, rx, ry+rh+5);
		gdk_draw_line(pixmap, gcwhite, rx, ry-5, rx, ry);
		tmp = g_strdup_printf("%d", color.red*0xFF/0xFFFF);
		layout = gtk_widget_create_pango_layout(fs_patterns_window, tmp);
		g_free(tmp);
		pango_layout_get_pixel_size(layout, &w, &h);
		gdk_draw_layout(pixmap, gcwhite,
			rx+(rw-w)/2, ry+rh+2, layout);
		rx += rw;
		color.red = color.green = color.blue = (i+1)*0xFFFF/(number-1);
	}
	gdk_draw_line(pixmap, gcwhite, rx, ry+rh, rx, ry+rh+5);
	gdk_draw_line(pixmap, gcwhite, rx, ry-5, rx, ry);
}

static void show_pattern(gchar* patternname)
{
	int x, y, width, height;
	/* FIXME: Really ugly, change this... */
#ifdef HAVE_XINERAMA
	if (xineramainfo) {
		x = xineramainfo[xineramacurrent].x_org;
		y = xineramainfo[xineramacurrent].y_org;
		width = xineramainfo[xineramacurrent].width;
		height = xineramainfo[xineramacurrent].height;
	}
	else {
		GdkScreen* screen = gdk_screen_get_default();
		x = 0;
		y = 0;
		width = gdk_screen_get_width(screen);
		height = gdk_screen_get_height(screen);
	}
#else
	GdkScreen* screen = gdk_screen_get_default();
	x = 0;
	y = 0;
	width = gdk_screen_get_width(screen);
	height = gdk_screen_get_height(screen);
#endif
	
	int top = 7*height/12, bottom = 11*height/12, left = width/3, right = 2*width/3;

	gtk_widget_set_size_request(fs_patterns_window, width, height);
	
	gtk_window_move(GTK_WINDOW(fs_patterns_window), x, y);
	
	GdkDrawable* pixmap = gdk_pixmap_new(0, width, height, gdk_colormap_get_visual(gdk_colormap_get_system())->depth);
	gdk_drawable_set_colormap(pixmap, gdk_colormap_get_system());
	
	GdkColor color;
	GdkGC* gc = gdk_gc_new(pixmap);
	if (g_str_equal(patternname, "brightnesscontrast")) {
		drawShade(pixmap, height/8, height/8, 21);
		//drawShade(pixmap, 3*height/8, height/8, 17);
		
		/* Fujitsu-Siemens blank lines for auto level (0xfe). */
		color.red = color.green = color.blue = 0xFFFF;
		gdk_gc_set_rgb_fg_color(gc, &color);
		gdk_draw_line(pixmap, gc, 0, (height)/24, width, (height)/24);
		gdk_draw_line(pixmap, gc, 0, (23*height)/24, width, (23*height)/24);
	}
	else {
		int w, h;
		color.red = color.green = color.blue = 0xFFFF;
		gdk_gc_set_rgb_fg_color(gc, &color);
		gchar* tmp = g_strdup_printf(_("<span size=\"large\">Unknown fullscreen pattern name: %s</span>"), patternname);
		PangoLayout* layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
		pango_layout_set_markup(layout, tmp, -1);
		g_free(tmp);
		pango_layout_get_pixel_size(layout, &w, &h);
		gdk_draw_layout(pixmap, gc, (width-w)/2, height/8, layout);
	}
	
	GdkPixbuf* pixbufs[4];
	
	pixbufs[0] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, 0, 0, 0, width, top);
	pixbufs[1] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, bottom, 0, 0, width, height-bottom);
	pixbufs[2] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, top, 0, 0, left, bottom-top);
	pixbufs[3] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, right, top, 0, 0, width-right, bottom-top);
	
	int i;
	if (images[0])
	{ /* GtkImages already exist */
		for (i = 0; i < 4; i++) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(images[i]), pixbufs[i]);
		}
	}
	else
	{
		for (i = 0; i < 4; i++) {
			images[i] = gtk_image_new_from_pixbuf(pixbufs[i]);
			gtk_widget_show(images[i]);
		}
		
		gtk_table_attach(GTK_TABLE(table), images[0],       0, 3, 0, 1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), images[2],       0, 1, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), scrolled_window, 1, 2, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), images[3],       2, 3, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), images[1],       0, 3, 2, 3, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_widget_show(table);
	}
	
	gtk_widget_set_size_request(scrolled_window, right-left, bottom-top);
	
	gtk_widget_show(fs_patterns_window);
}

void fullscreen_callback(GtkWidget *widget, gpointer data) {
	monmainvbox = g_object_get_data(G_OBJECT(widget),"mainvbox");
	vbox = g_object_get_data(G_OBJECT(widget),"vbox");
	
	if (!fs_patterns_window)
		create_fullscreen_patterns_window();
	
	gtk_widget_ref(vbox);
	gtk_container_remove(GTK_CONTAINER(monmainvbox), vbox);
	gtk_box_pack_start(GTK_BOX(centervbox), vbox, 0, 5, 5);
	gtk_widget_unref(vbox);
	
	show_pattern(g_object_get_data(G_OBJECT(widget),"pattern"));
}
