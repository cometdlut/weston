/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <cairo.h>
#include <sys/time.h>

#include <linux/input.h>
#include <wayland-client.h>
#include "window.h"

struct flower {
	struct display *display;
	struct window *window;
	struct widget *widget;
	int width, height;
};

static void
set_random_color(cairo_t *cr)
{
	cairo_set_source_rgba(cr,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 100) / 99.0);
}


static void
draw_stuff(cairo_surface_t *surface, int width, int height)
{
	const int petal_count = 3 + random() % 5;
	const double r1 = 60 + random() % 35;
	const double r2 = 20 + random() % 40;
	const double u = (10 + random() % 90) / 100.0;
	const double v = (random() % 90) / 100.0;

	cairo_t *cr;
	int i;
	double t, dt = 2 * M_PI / (petal_count * 2);
	double x1, y1, x2, y2, x3, y3;

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_translate(cr, width / 2, height / 2);
	cairo_move_to(cr, cos(0) * r1, sin(0) * r1);
	for (t = 0, i = 0; i < petal_count; i++, t += dt * 2) {
		x1 = cos(t) * r1;
		y1 = sin(t) * r1;
		x2 = cos(t + dt) * r2;
		y2 = sin(t + dt) * r2;
		x3 = cos(t + 2 * dt) * r1;
		y3 = sin(t + 2 * dt) * r1;

		cairo_curve_to(cr,
			       x1 - y1 * u, y1 + x1 * u,
			       x2 + y2 * v, y2 - x2 * v,
			       x2, y2);			       

		cairo_curve_to(cr,
			       x2 - y2 * v, y2 + x2 * v,
			       x3 + y3 * u, y3 - x3 * u,
			       x3, y3);
	}

	cairo_close_path(cr);
	set_random_color(cr);
	cairo_fill_preserve(cr);
	set_random_color(cr);
	cairo_stroke(cr);

	cairo_destroy(cr);
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height, void *data)
{
	struct flower *flower = data;

	/* Dont resize me */
	widget_set_size(flower->widget, flower->width, flower->height);
}

static void
compute_input_region(cairo_surface_t *surface, struct widget *widget)
{
	unsigned char *data;
	cairo_format_t format;
	int width, height, i, y;
	const struct rectangle *rect;
	struct rectangle r;

	format = cairo_image_surface_get_format(surface);

	if (format != CAIRO_FORMAT_ARGB32)
		return;

	cairo_surface_flush(surface);
	data = cairo_image_surface_get_data(surface);
	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	rect = &r;

	for (i = 0, y = 0; i <= width * height; i++) {
		if (*(data + i * 4) != 0) {
			r.x = i - y * width;
			r.y = y;
			for (i++; ; i++) {
				if (*(data + i * 4) == 0 ||
				    i - y * width >= width) {
					r.width = i - y * width - r.x;
					r.height = 1;
					widget_input_region_add(widget, rect);
					break;
				}
			}
		}

		if (i - y * width >= width)
			y++;
	}
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct flower *flower = data;
	cairo_surface_t *surface;

	surface = window_get_surface(flower->window);
	if (surface == NULL ||
	    cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to create cairo egl surface\n");
		return;
	}

	draw_stuff(surface, flower->width, flower->height);
	compute_input_region(surface, flower->widget);
	cairo_surface_destroy(surface);
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button, enum wl_pointer_button_state state, void *data)
{
	struct flower *flower = data;

	switch (button) {
	case BTN_LEFT:
		if (state == WL_POINTER_BUTTON_STATE_PRESSED)
			window_move(flower->window, input,
				    display_get_serial(flower->display));
		break;
	case BTN_MIDDLE:
		if (state == WL_POINTER_BUTTON_STATE_PRESSED)
			widget_schedule_redraw(widget);
		break;
	case BTN_RIGHT:
		if (state == WL_POINTER_BUTTON_STATE_PRESSED)
			window_show_frame_menu(flower->window, input, time);
		break;
	}
}

static void
touch_down_handler(struct widget *widget, struct input *input, 
		   uint32_t serial, uint32_t time, int32_t id, 
		   float x, float y, void *data)
{
	struct flower *flower = data;
	window_move(flower->window, input, display_get_serial(flower->display));
}

int main(int argc, char *argv[])
{
	struct flower flower;
	struct display *d;
	struct timeval tv;

	d = display_create(&argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	gettimeofday(&tv, NULL);
	srandom(tv.tv_usec);

	flower.width = 200;
	flower.height = 200;
	flower.display = d;
	flower.window = window_create(d);
	flower.widget = window_add_widget(flower.window, &flower);
	window_set_title(flower.window, "Flower");

	widget_set_resize_handler(flower.widget, resize_handler);
	widget_set_redraw_handler(flower.widget, redraw_handler);
	widget_set_button_handler(flower.widget, button_handler);
	widget_set_default_cursor(flower.widget, CURSOR_HAND1);
	widget_set_touch_down_handler(flower.widget, touch_down_handler);

	window_schedule_resize(flower.window, flower.width, flower.height);

	display_run(d);

	return 0;
}
