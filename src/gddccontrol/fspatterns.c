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

static GtkWidget* grid;
static GtkWidget* centervbox;
static GtkWidget* scrolled_window;

static GtkWidget* monmainvbox; /* Monitor manager main GtkBox (containing vbox) */
static GtkWidget* vbox; /* GtkBox containing controls */

static GtkWidget* images[4] = {NULL, NULL, NULL, NULL}; /* top-bottom-left-right images */

/* TODO make sure we are drawing to raw (device) pixels, not to logical pixels
 * in GTK, especially on HiDPI monitors / fractional scaling.
 * TODO make sure we are drawing with raw colors, ignoring redshift. Maybe we
 * also need to ignore color correction profiles (ICC)? */

static void destroy(GtkWidget *widget, gpointer data)
{
	g_object_ref(vbox);
	gtk_container_remove(GTK_CONTAINER(centervbox), vbox);
	gtk_box_pack_start(GTK_BOX(monmainvbox), vbox, 0, 5, 5);
	g_object_unref(vbox);
	
	gtk_widget_hide(fs_patterns_window);
}

static void create_fullscreen_patterns_window()
{
	fs_patterns_window = gtk_window_new(GTK_WINDOW_POPUP);
	
	gtk_container_set_border_width(GTK_CONTAINER(fs_patterns_window), 0);
	
	grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(fs_patterns_window), grid);
	
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	
	centervbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
	
	GtkWidget* close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(close_button),"clicked",G_CALLBACK (destroy), NULL);
	gtk_widget_show(close_button);
	gtk_widget_set_halign(close_button, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(close_button, GTK_ALIGN_START);
	gtk_box_pack_end(GTK_BOX(centervbox), close_button, TRUE, 5, 5);
	gtk_widget_show(centervbox);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), centervbox);
	
	gtk_widget_show(scrolled_window);
}

static void draw_shade(cairo_surface_t* surface, int y_position, int shade_height, int shade_count) {
	GdkColor color;
	PangoLayout* layout;
	gchar* tmp;
	
	cairo_t* cairo_shade = cairo_create(surface);
	cairo_t* cairo_white = cairo_create(surface);
	cairo_set_line_cap(cairo_white, CAIRO_LINE_CAP_SQUARE);
	
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	
	int x_margin = width/12;
	int shade_width = (width-(x_margin*2))/shade_count;
	int x_position = x_margin; // will be incremented below
	int shade_index;
	int label_width, label_height;
	
	cairo_set_source_rgb(cairo_white, 1.0, 1.0, 1.0);
	
	for (shade_index = 0; shade_index < shade_count; shade_index++) {
		// draw shade
		double gray_level = (double)shade_index / (double)(shade_count - 1);
		cairo_set_source_rgb(cairo_shade, gray_level, gray_level, gray_level);
		cairo_rectangle(cairo_shade, x_position, y_position, shade_width, shade_height);
		cairo_fill(cairo_shade);

		// draw tick at shade border (bottom)
		cairo_move_to(cairo_white, x_position, y_position-6);
		cairo_line_to(cairo_white, x_position, y_position-1);
		cairo_stroke(cairo_white);
		// draw tick at shade border (top)
		cairo_move_to(cairo_white, x_position, y_position+shade_height);
		cairo_line_to(cairo_white, x_position, y_position+shade_height+5);
		cairo_stroke(cairo_white);

		// draw label displaying color value
		tmp = g_strdup_printf("%d", (int)(gray_level * 255));
		layout = gtk_widget_create_pango_layout(fs_patterns_window, tmp);
		g_free(tmp);
		pango_layout_get_pixel_size(layout, &label_width, &label_height);
		cairo_move_to(cairo_white, x_position+(shade_width-label_width)/2, y_position+shade_height+2);
		pango_cairo_show_layout(cairo_white, layout);
		g_object_unref(layout);
		
		x_position += shade_width;
	}
	cairo_destroy(cairo_shade);
	// draw tick at shade border (bottom)
	cairo_move_to(cairo_white, x_position, y_position-6);
	cairo_line_to(cairo_white, x_position, y_position-1);
	cairo_stroke(cairo_white);
	// draw tick at shade border (top)
	cairo_move_to(cairo_white, x_position, y_position+shade_height);
	cairo_line_to(cairo_white, x_position, y_position+shade_height+5);
	cairo_stroke(cairo_white);

	layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
	pango_layout_set_markup(layout,
		_("Adjust brightness and contrast following these rules:\n"
		  " - Black must be as dark as possible.\n"
		  " - White should be as bright as possible.\n"
		  " - You must be able to distinguish each gray level (particularly 0 and 12).\n"
		  )
		  , -1);
	pango_layout_get_pixel_size(layout, &label_width, &label_height);
	cairo_move_to(cairo_white, (width-label_width)/2, 3*height/8);
	pango_cairo_show_layout(cairo_white, layout);
	g_object_unref(layout);
	
	// Fujitsu-Siemens blank lines for auto level (0xfe)
	cairo_set_line_width(cairo_white, 1);
	cairo_move_to(cairo_white, 0, 0.5f+height/24);
	cairo_line_to(cairo_white, width, 0.5f+height/24);
	cairo_stroke(cairo_white);
	cairo_move_to(cairo_white, 0, (23*height)/24+0.5f);
	cairo_line_to(cairo_white, width, (23*height)/24+0.5f);
	cairo_stroke(cairo_white);

	cairo_destroy(cairo_white);
}

static void draw_checker(cairo_surface_t* surface, int width, int height, gchar* text) {
	int label_width, label_height;
	cairo_t* cairo = cairo_create(surface);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

	// draw checker pattern (alternating white pixels)
	int x, y;
	for (x = 0; x < width; x += 1) {
		for (y = x%2; y < height; y += 2) {
			cairo_rectangle(cairo, x, y, 1, 1);
			cairo_fill(cairo);
		}
	}

	// draw black label on grey background
	PangoLayout* layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
	pango_layout_set_markup(layout, text, -1);
	pango_layout_get_pixel_size(layout, &label_width, &label_height);
	cairo_set_source_rgb(cairo, 0.5, 0.5, 0.5);
	cairo_rectangle(cairo, (width-label_width)/2-5, 3*height/8-5, label_width+10, label_height+10);
	cairo_fill(cairo);
	cairo_set_source_rgb(cairo, 0.0, 0.0, 0.0);
	cairo_move_to(cairo, (width-label_width)/2, 3*height/8);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);
	
	cairo_destroy(cairo);
}

static void draw_color_crosses(cairo_surface_t* surface, int width, int height) {
	const int cross_size = 51;
	const int arm_length = 25;

	cairo_t* cairo = cairo_create(surface);
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_SQUARE);
	cairo_set_line_width(cairo, 1);
	
	int x, y, color_index = 0, color_column = 0;
	for (x = 0; x < width; x += cross_size) {
		color_index = color_column;
		for (y = 0; y < height; y += cross_size) {
			switch (color_index % 4) {
			case 0:
				cairo_set_source_rgb(cairo, 1.0, 0.0, 0.0);
				break;
			case 1:
				cairo_set_source_rgb(cairo, 0.0, 1.0, 0.0);
				break;
			case 2:
				cairo_set_source_rgb(cairo, 0.0, 0.0, 1.0);
				break;
			default:
				cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
			}
			
			cairo_move_to(cairo, 0.5f+x+arm_length, 0.5f+y);
			cairo_line_to(cairo, 0.5f+x+arm_length, 0.5f+y+cross_size);
			cairo_stroke(cairo);
			
			cairo_move_to(cairo, 0.5f+x, 0.5f+y+arm_length);
			cairo_line_to(cairo, 0.5f+x+cross_size, 0.5f+y+arm_length);
			cairo_stroke(cairo);
			
			color_index++;
		}
		color_column++;
	}
	cairo_destroy(cairo);
}

static void show_pattern(gchar* patternname)
{
	// TODO create and draw fullscreen window instead which we can close with [ESC] key
	GdkRectangle drect;
	
	GdkScreen* screen = gdk_screen_get_default();
	int i = gdk_screen_get_monitor_at_window(gdk_screen_get_default(), gtk_widget_get_window(main_app_window));
	gdk_screen_get_monitor_geometry(screen, i, &drect);
		
	int top = 7*drect.height/12, bottom = 11*drect.height/12, left = drect.width/3, right = 2*drect.width/3;
	
	gtk_widget_set_size_request(fs_patterns_window, drect.width, drect.height);
	
	gtk_window_move(GTK_WINDOW(fs_patterns_window), drect.x, drect.y);

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, drect.width, drect.height);

	// draw black background
	cairo_t* cairo = cairo_create(surface);
	cairo_set_source_rgb(cairo, 0.0, 0.0, 0.0);
	cairo_rectangle(cairo, 0, 0, drect.width, drect.height);
	cairo_fill(cairo);
	cairo_destroy(cairo);

	// TODO turn this string matching into an enum or similar
	if (g_str_equal(patternname, "brightnesscontrast")) {
		draw_shade(surface, drect.height/8, drect.height/8, 21);
	}
	else if (g_str_equal(patternname, "moire")) {
		draw_checker(surface, drect.width, drect.height, _("Try to make moire patterns disappear."));
	}
	else if (g_str_equal(patternname, "clock")) {
		draw_checker(surface, drect.width, drect.height,
			_("Adjust Image Lock Coarse to make the vertical band disappear.\n"
			  "Adjust Image Lock Fine to minimize movement on the screen."));
	}
	else if (g_str_equal(patternname, "misconvergence")) {
		draw_color_crosses(surface, drect.width, drect.height);
	}
	else { // TODO when using enum above, this code can be deleted
		int label_width, label_height;
		cairo_t* cairo = cairo_create(surface);
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		gchar* tmp = g_strdup_printf(_("Unknown fullscreen pattern name: %s"), patternname);
		PangoLayout* layout = pango_layout_new(gtk_widget_get_pango_context(fs_patterns_window));
		pango_layout_set_markup(layout, tmp, -1);
		g_free(tmp);
		pango_layout_get_pixel_size(layout, &label_width, &label_height);
		cairo_move_to(cairo, (drect.width-label_width)/2, drect.height/8);
		pango_cairo_show_layout(cairo, layout);
		g_object_unref(layout);
		cairo_destroy(cairo);
	}

	GdkPixbuf* pixbufs[4];
	
	pixbufs[0] = gdk_pixbuf_get_from_surface(surface, 0, 0, drect.width, top);
	pixbufs[1] = gdk_pixbuf_get_from_surface(surface, 0, bottom, drect.width, drect.height-bottom);
	pixbufs[2] = gdk_pixbuf_get_from_surface(surface, 0, top, left, bottom-top);
	pixbufs[3] = gdk_pixbuf_get_from_surface(surface, right, top, drect.width-right, bottom-top);
	cairo_surface_destroy(surface);
	
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
		
		gtk_grid_attach(GTK_GRID(grid), images[0], 0, 0, 3, 1);
		gtk_widget_set_hexpand(images[0], TRUE);
		gtk_widget_set_vexpand(images[0], TRUE);
		gtk_grid_attach(GTK_GRID(grid), images[2], 0, 1, 1, 1);
		gtk_widget_set_hexpand(images[2], TRUE);
		gtk_widget_set_vexpand(images[2], TRUE);
		gtk_grid_attach(GTK_GRID(grid), scrolled_window, 1, 1, 1, 1);
		gtk_widget_set_hexpand(scrolled_window, TRUE);
		gtk_widget_set_vexpand(scrolled_window, TRUE);
		gtk_grid_attach(GTK_GRID(grid), images[3], 2, 1, 1, 1);
		gtk_widget_set_hexpand(images[3], TRUE);
		gtk_widget_set_vexpand(images[3], TRUE);
		gtk_grid_attach(GTK_GRID(grid), images[1], 0, 2, 3, 1);
		gtk_widget_set_hexpand(images[1], TRUE);
		gtk_widget_set_vexpand(images[1], TRUE);
		gtk_widget_show(grid);
	}
	for (i = 0; i < 4; i++) {
		g_object_unref(pixbufs[i]);
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
	
	g_object_ref(vbox);
	gtk_container_remove(GTK_CONTAINER(monmainvbox), vbox);
	gtk_box_pack_start(GTK_BOX(centervbox), vbox, 0, 5, 5);
	g_object_unref(vbox);
	
	show_pattern(g_object_get_data(G_OBJECT(widget),"pattern"));
}
