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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#include "notebook.h"
#include "internal.h"

#include <string.h>

static GtkWidget* fs_patterns_window = NULL;

static GtkWidget* table;
static GtkWidget* centervbox;
static GtkWidget* scrolled_window;

static GtkWidget* monmainvbox; /* Monitor manager main GtkVBox (containing vbox) */
static GtkWidget* vbox; /* GtkVBox containing controls */

static GtkWidget* images[4] = {NULL, NULL, NULL, NULL}; /* top-bottom-left-right images */

/* TODO make sure we are drawing to raw (device) pixels, not to logical pixels
 * in GTK, especially on HiDPI monitors / fractional scaling.
 * TODO make sure we are drawing with raw colors, ignoring redshift. Maybe we
 * also need to ignore color correction profiles (ICC)? */

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

static void drawShade(GdkDrawable* pixmap, int y_position, int shade_height, int shade_count) {
	GdkColor color;
	PangoLayout* layout;
	gchar* tmp;
	
	cairo_t* gc = gdk_cairo_create(pixmap);
	cairo_t* gcwhite = gdk_cairo_create(pixmap);
	cairo_set_line_cap(gcwhite, CAIRO_LINE_CAP_SQUARE);
	
	int width, height;
	gdk_pixmap_get_size(pixmap, &width, &height);
	
	int x_margin = width/12;
	int shade_width = (width-(x_margin*2))/shade_count;
	int x_position = x_margin; // will be incremented below
	int shade_index;
	int label_width, label_height;
	
	color.red = color.green = color.blue = 0x8000;
	gdk_cairo_set_source_color(gcwhite, &color);
	
	color.red = color.green = color.blue = 0x0000;
	for (shade_index = 0; shade_index < shade_count; shade_index++) {
		// draw shade
		gdk_cairo_set_source_color(gc, &color);
		cairo_rectangle(gc, x_position, y_position, shade_width, shade_height);
		cairo_fill(gc);

		// draw tick at shade border (bottom)
		cairo_move_to(gcwhite, x_position, y_position-6);
		cairo_line_to(gcwhite, x_position, y_position-1);
		cairo_stroke(gcwhite);
		// draw tick at shade border (top)
		cairo_move_to(gcwhite, x_position, y_position+shade_height);
		cairo_line_to(gcwhite, x_position, y_position+shade_height+5);
		cairo_stroke(gcwhite);

		// draw label displaying color value
		tmp = g_strdup_printf("%d", color.red*0xFF/0xFFFF);
		layout = gtk_widget_create_pango_layout(fs_patterns_window, tmp);
		g_free(tmp);
		pango_layout_get_pixel_size(layout, &label_width, &label_height);
		cairo_move_to(gcwhite, x_position+(shade_width-label_width)/2, y_position+shade_height+2);
		pango_cairo_show_layout(gcwhite, layout);
		
		x_position += shade_width;
		color.red = color.green = color.blue = (shade_index+1)*0xFFFF/(shade_count-1);
	}
	// draw tick at shade border (bottom)
	cairo_move_to(gcwhite, x_position, y_position-6);
	cairo_line_to(gcwhite, x_position, y_position-1);
	cairo_stroke(gcwhite);
	// draw tick at shade border (top)
	cairo_move_to(gcwhite, x_position, y_position+shade_height);
	cairo_line_to(gcwhite, x_position, y_position+shade_height+5);
	cairo_stroke(gcwhite);
	
	//g_object_unref(gc);
	//g_object_unref(gcwhite);
}

static void drawchecker(GdkDrawable* pixmap, int width, int height, gchar* text) {
	int label_width, label_height;
	cairo_t* gc = gdk_cairo_create(pixmap);
	cairo_set_line_cap(gc, CAIRO_LINE_CAP_SQUARE);
	GdkColor color;
	color.red = color.green = color.blue = 0xFFFF;
	gdk_cairo_set_source_color(gc, &color);
	
	int x, y;
	for (x = 0; x < width; x += 1) {
		for (y = x%2; y < height; y += 2) {
			gdk_draw_point(pixmap, gc, x, y);
		}
	}
	
	PangoLayout* layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
	pango_layout_set_markup(layout, text, -1);
	pango_layout_get_pixel_size(layout, &label_width, &label_height);
	
	color.red = color.green = color.blue = 0x8000;
	gdk_cairo_set_source_color(gc, &color);
	cairo_rectangle(gc, (width-label_width)/2-5, 3*height/8-5, label_width+10, label_height+10);
	cairo_fill(gc);
	
	color.red = color.green = color.blue = 0x0000;
	gdk_cairo_set_source_color(gc, &color);
	cairo_move_to(gc, (width-label_width)/2, 3*height/8);
	pango_cairo_show_layout(gc, layout);
	
	g_object_unref(gc);
}

static void show_pattern(gchar* patternname)
{
	// TODO create and draw fullscreen window instead which we can close with [ESC] key
	GdkRectangle drect;
	
	GdkScreen* screen = gdk_screen_get_default();
	int i = gdk_screen_get_monitor_at_window(gdk_screen_get_default(), main_app_window->window);
	gdk_screen_get_monitor_geometry(screen, i, &drect);
		
	int top = 7*drect.height/12, bottom = 11*drect.height/12, left = drect.width/3, right = 2*drect.width/3;
	
	gtk_widget_set_size_request(fs_patterns_window, drect.width, drect.height);
	
	gtk_window_move(GTK_WINDOW(fs_patterns_window), drect.x, drect.y);
	
	GdkDrawable* pixmap = gdk_pixmap_new(0, drect.width, drect.height, gdk_colormap_get_visual(gdk_colormap_get_system())->depth);
	gdk_drawable_set_colormap(pixmap, gdk_colormap_get_system());
	
	int label_width, label_height;
	GdkColor color;
	cairo_t* gc = gdk_cairo_create(pixmap);
	cairo_set_line_cap(gc, CAIRO_LINE_CAP_SQUARE);
	color.red = color.green = color.blue = 0x0000;
	gdk_cairo_set_source_color(gc, &color);
	cairo_rectangle(gc, 0, 0, drect.width, drect.height);
	cairo_fill(gc);
	if (g_str_equal(patternname, "brightnesscontrast")) {
		drawShade(pixmap, drect.height/8, drect.height/8, 21);
		
		color.red = color.green = color.blue = 0xFFFF;
		gdk_cairo_set_source_color(gc, &color);
		PangoLayout* layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
		pango_layout_set_markup(layout,
			_("Adjust brightness and contrast following these rules:\n"
			  " - Black must be as dark as possible.\n"
			  " - White should be as bright as possible.\n"
			  " - You must be able to distinguish each gray level (particularly 0 and 12).\n"
			  )
			  , -1);
		pango_layout_get_pixel_size(layout, &label_width, &label_height);
		cairo_move_to(gc, (drect.width-label_width)/2, 3*drect.height/8);
		pango_cairo_show_layout(gc, layout);
		
		/* Fujitsu-Siemens blank lines for auto level (0xfe). */
		color.red = color.green = color.blue = 0xFFFF;
		gdk_cairo_set_source_color(gc, &color);
		cairo_set_line_cap(gc, CAIRO_LINE_CAP_SQUARE);
		cairo_move_to(gc, 0, (drect.height)/24);
		cairo_line_to(gc, drect.width, (drect.height)/24);
		cairo_stroke(gc);
		cairo_move_to(gc, 0, (23*drect.height)/24);
		cairo_line_to(gc, drect.width, (23*drect.height)/24);
		cairo_stroke(gc);
	}
	else if (g_str_equal(patternname, "moire")) {
		drawchecker(pixmap, drect.width, drect.height, _("Try to make moire patterns disappear."));
	}
	else if (g_str_equal(patternname, "clock")) {
		drawchecker(pixmap, drect.width, drect.height,
			_("Adjust Image Lock Coarse to make the vertical band disappear.\n"
			  "Adjust Image Lock Fine to minimize movement on the screen."));
	}
	else if (g_str_equal(patternname, "misconvergence")) {
		const int csize = 51;
		const int dsize = 25;
		int x, y, c = 0, d = 0;
		for (x = 0; x < drect.width; x += csize) {
			c = d;
			for (y = 0; y < drect.height; y += csize) {
				color.red = 0x0000;
				color.green = 0x0000;
				color.blue = 0x0000;
				switch (c % 4) {
				case 0:
					color.red = 0xFFFF;
					break;
				case 1:
					color.green = 0xFFFF;
					break;
				case 2:
					color.blue = 0xFFFF;
					break;
				default:
					color.red = 0xFFFF;
					color.green = 0xFFFF;
					color.blue = 0xFFFF;
				}
				gdk_cairo_set_source_color(gc, &color);
				// TODO maybe +0.5f pixel
				cairo_move_to(gc, x+dsize, y);
				cairo_line_to(gc, x+dsize, y+csize);
				cairo_stroke(gc);
				
				// TODO maybe +0.5f pixel
				cairo_move_to(gc, x, y+dsize);
				cairo_line_to(gc, x+csize, y+dsize);
				cairo_stroke(gc);
				c++;
			}
			d++;
		}
	}
	else {
		color.red = color.green = color.blue = 0xFFFF;
		gdk_cairo_set_source_color(gc, &color);
		gchar* tmp = g_strdup_printf(_("Unknown fullscreen pattern name: %s"), patternname);
		PangoLayout* layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
		pango_layout_set_markup(layout, tmp, -1);
		g_free(tmp);
		pango_layout_get_pixel_size(layout, &label_width, &label_height);
		cairo_move_to(gc, (drect.width-label_width)/2, drect.height/8);
		pango_cairo_show_layout(gc, layout);
	}
	
	g_object_unref(gc);
	
	GdkPixbuf* pixbufs[4];
	
	pixbufs[0] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, 0, 0, 0, drect.width, top);
	pixbufs[1] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, bottom, 0, 0, drect.width, drect.height-bottom);
	pixbufs[2] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, top, 0, 0, left, bottom-top);
	pixbufs[3] = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, right, top, 0, 0, drect.width-right, bottom-top);
	
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
			g_object_unref(pixbufs[i]);
		}
		
		gtk_table_attach(GTK_TABLE(table), images[0],       0, 3, 0, 1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), images[2],       0, 1, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), scrolled_window, 1, 2, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), images[3],       2, 3, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_table_attach(GTK_TABLE(table), images[1],       0, 3, 2, 3, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 0, 0);
		gtk_widget_show(table);
	}
	
	gtk_widget_set_size_request(scrolled_window, right-left, bottom-top);
	
	GdkGeometry hints;

	hints.min_width = drect.width;
	hints.min_height = drect.height;
	hints.max_width = drect.width;
	hints.max_height = drect.height;
	
	gtk_window_set_geometry_hints(GTK_WINDOW(fs_patterns_window), GTK_WIDGET(fs_patterns_window),
	                              &hints, GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
	
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
