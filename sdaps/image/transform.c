/* SDAPS
 * Copyright (C) 2012  Benjamin Berg <benjamin@sipsolutions.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "transform.h"
#include <string.h>
#include <math.h>
#include <glib/gprintf.h>

cairo_surface_t*
surface_copy_partial(cairo_surface_t *surface, int x, int y, int width, int height)
{
	cairo_surface_t *result;
	cairo_t *cr;

	result = cairo_image_surface_create(cairo_image_surface_get_format(surface), width, height);
	cr = cairo_create(result);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	/* In case the area is outside of the source image, fill with zeros. */
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_source_surface(cr, surface, -x, -y);
	cairo_paint(cr);

	cairo_destroy(cr);

	cairo_surface_flush(result);

	return result;
}

cairo_surface_t*
surface_copy(cairo_surface_t *surface)
{
	int width, height;
	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);

	return surface_copy_partial(surface, 0, 0, width, height);
}

#if 0
/* This function is for debugging purposes. */
void
a1_surface_write_to_png(cairo_surface_t* surface, gchar* filename)
{
	guint img_width, img_height;
	cairo_surface_t *tmp_surface;
	cairo_t *cr;

	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);

	tmp_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, img_width, img_height);

	cr = cairo_create(tmp_surface);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_mask_surface(cr, surface, 0, 0);

	cairo_surface_write_to_png(tmp_surface, filename);

	cairo_destroy(cr);
	cairo_surface_destroy(tmp_surface);
}


/* The following is an adaption from the Animal imaging library.
 * The library was written by Ricardo Fabbri (2011) and is licensed under the
 * GPLv2 or any later version there.
 * The same algorithm is used in queXF for character recognition. */

/* Offsets to the neighbors of a pixel. */
static int n8[8][2] = {
  { 0,  1},
  {-1,  1},
  {-1,  0},
  {-1, -1},
  { 0, -1},
  { 1, -1},
  { 1,  0},
  { 1,  1} 
};

/*
	This function tells the number N of regions that would exist if the
	pixel P(r,c) where changed from FG to BG. It returns 2*N, that is,
	the number of pixel transitions in the neighbourhood.
*/
static int
crossing_index_np(guint32 *pixels, int stride, int x, int y)
{
	guint n=0, idx;
	guint current, next;

	/* The last one (so we get the crossing from last to first pixel). */
	current = GET_PIXEL(pixels, stride, x + n8[7][0], y + n8[7][1]);

	for (idx = 0; idx < 8; idx++) {
		next = GET_PIXEL(pixels, stride, x + n8[idx][0], y + n8[idx][1]);

		if (next != current)
			n++;

		current = next;
	}

	return n;
}

int 
nh8count_np(guint32 *pixels, int stride, int x, int y, int val) 
{
   guint i, n=0;

   for (i=0; i < 8; i++)
      if (GET_PIXEL(pixels, stride, x + n8[i][1], y + n8[i][0]) == val)
         n++;

   return n;
}

/*
	In place Zhang-Suen thinning. The algorithm leaves a 1px border untouched.
*/
void
thinzs_np(cairo_surface_t *surface)
{
	gboolean modified;
	guint x, y, n;
	guint img_width, img_height;
	guint stride, tmp_stride;
	guint32 *pixels, *tmp_pixels;
	guint pixel;
	guint FG=1, BG=0;

	cairo_surface_t *tmp_surface;
	tmp_surface = surface_copy(surface);

	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);

	pixels = (guint32*) cairo_image_surface_get_data(surface);
	stride = cairo_image_surface_get_stride(surface);

	tmp_pixels = (guint32*) cairo_image_surface_get_data(tmp_surface);
	tmp_stride = cairo_image_surface_get_stride(tmp_surface);


	do {
		modified = FALSE;

		for (y=1; y < img_height-1; y++)  for (x=1; x < img_width-1; x++) {
			pixel = GET_PIXEL(pixels, stride, x, y);

			SET_PIXEL(tmp_pixels, tmp_stride, x, y, pixel);

			if (pixel == FG) {
				n = nh8count_np(pixels, stride, x, y, FG);

				if ( n >= 2 && n <= 6 && crossing_index_np(pixels, stride, x, y) == 2) {
					if((GET_PIXEL(pixels, stride, x, y-1) == BG ||
					    GET_PIXEL(pixels, stride, x+1, y) == BG ||
					    GET_PIXEL(pixels, stride, x, y+1) == BG)
					&& (GET_PIXEL(pixels, stride, x, y-1) == BG ||
					    GET_PIXEL(pixels, stride, x, y+1) == BG ||
					    GET_PIXEL(pixels, stride, x-1, y) == BG))
					{
						SET_PIXEL(tmp_pixels, tmp_stride, x, y, BG);
						modified = TRUE;
					}
				}
			}
		}

		for (y=1; y < img_height-1; y++)  for (x=1; x < img_width-1; x++) {
			pixel = GET_PIXEL(tmp_pixels, tmp_stride, x, y);

			SET_PIXEL(pixels, stride, x, y, pixel);

			if (pixel == FG) {
				n = nh8count_np(tmp_pixels, tmp_stride, x, y, FG);

				if ( n>=2 && n<=6 && crossing_index_np(tmp_pixels, tmp_stride, x, y) == 2) {
					if((GET_PIXEL(pixels, stride, x+1, y) == BG ||
					    GET_PIXEL(pixels, stride, x-1, y) == BG ||
					    GET_PIXEL(pixels, stride, x, y-1) == BG)
					&& (GET_PIXEL(pixels, stride, x, y-1) == BG ||
					    GET_PIXEL(pixels, stride, x-1, y) == BG ||
					    GET_PIXEL(pixels, stride, x, y+1) == BG))
					{
						SET_PIXEL(pixels, stride, x, y, BG);
						modified = TRUE;
					}
				}
			}
		}
	} while (modified);

	cairo_surface_destroy(tmp_surface);

	cairo_surface_mark_dirty(surface);
}
#endif


/* The following is an adaption from gamera. It is a modified
 * kfill algorithm.
 *
 * The original code was released under the GPLv2 or any later version.
 *
 * The Gamera authors for the file are:
 * Copyright (C) 2001-2005 Ichiro Fujinaga, Michael Droettboom, Karl MacMillan
 *               2010      Christoph Dalitz, Oliver Christen
 *               2011-2012 Christoph Dalitz, David Kolanus
 *
 * The same algorithm is used in queXF for character recognition. */

void
kfill_get_condition_variables(guint32 *pixels,
                              guint    stride,
                              guint    k,
                              guint    x,
                              guint    y,
                              gint    *n,
                              gint    *r,
                              gint    *c)
{
	guint corner_pixel_count;
	guint ccs;
	guint pixel_count;
	guint pixel, last_px;
	guint _x, _y;


	corner_pixel_count = GET_PIXEL(pixels, stride, x, y);
	corner_pixel_count += GET_PIXEL(pixels, stride, x + k - 1, y);
	corner_pixel_count += GET_PIXEL(pixels, stride, x, y + k - 1);
	corner_pixel_count += GET_PIXEL(pixels, stride, x + k - 1, y + k - 1);

	/* Count border pixels and number of color changes when walking around
	 * the border. */
	ccs = 0;
	pixel_count = 0;
	/* The last pixel is also the "first". */
	last_px = GET_PIXEL(pixels, stride, x, y + 1);
	_y = y;
	_x = x;

	for (; _x < x + k - 1; _x++) {
		pixel = GET_PIXEL(pixels, stride, _x, _y);
		if (last_px != pixel)
			ccs += 1;
		pixel_count += pixel;
		last_px = pixel;
	}

	for (; _y < y + k - 1; _y++) {
		pixel = GET_PIXEL(pixels, stride, _x, _y);
		if (last_px != pixel)
			ccs += 1;
		pixel_count += pixel;
		last_px = pixel;
	}

	for (; _x > x; _x--) {
		pixel = GET_PIXEL(pixels, stride, _x, _y);
		if (last_px != pixel)
			ccs += 1;
		pixel_count += pixel;
		last_px = pixel;
	}

	for (; _y > y; _y--) {
		pixel = GET_PIXEL(pixels, stride, _x, _y);
		if (last_px != pixel)
			ccs += 1;
		pixel_count += pixel;
		last_px = pixel;
	}

	*n = pixel_count;
	*r = corner_pixel_count;
	*c = ccs;
}

/* This implementation of kfill_modified works in place. */
void
kfill_modified(cairo_surface_t* surface, gint k)
{
	guint x, y;
	guint img_width, img_height;
	guint stride, tmp_stride;
	guint32 *pixels, *tmp_pixels;
	guint core_pixels;
	guint core_color;

	/* r: number of pixels in the neighborhood corners
	 * n: number of pixels in the neighberhood
	 * c: number of ccs in neighborhood
	 */
	gint r, n, c;

	cairo_surface_t *tmp_surface;
	tmp_surface = surface_copy(surface);

	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);

	pixels = (guint32*) cairo_image_surface_get_data(surface);
	stride = cairo_image_surface_get_stride(surface);

	tmp_pixels = (guint32*) cairo_image_surface_get_data(tmp_surface);
	tmp_stride = cairo_image_surface_get_stride(tmp_surface);

	for (y = 0; y < img_height - k; y++) {
		for (x = 0; x < img_width - k; x++) {
			core_pixels = count_black_pixel_unchecked(tmp_pixels, tmp_stride, x+1, y+1, k-2, k-2);

			kfill_get_condition_variables(tmp_pixels, tmp_stride, k, x, y, &n, &r, &c);

			/* more than half black or white? This would be the new core color. */
			core_color = core_pixels * 2 >= (k-2)*(k-2);

			if (core_color) {
				n = ( 4*(k-1) ) - n;
				r = 4 - r;
			}

			if ((c <= 1) && ((n > 3*k - 4) || (n == 3*k - 4) && (r == 2))) {
				set_pixels_unchecked(pixels, stride, x+1, y+1, k-2, k-2, !core_color);
			} else {
				set_pixels_unchecked(pixels, stride, x+1, y+1, k-2, k-2, core_color);
			}
		}
	}
}

static void
mark_pixel(cairo_surface_t *debug_surf, gint x, gint y)
{
	cairo_t *cr;

	cr = cairo_create(debug_surf);
	cairo_set_source_rgba(cr, 1, 0, 0, 0.5);
	cairo_rectangle(cr, x-0.5, y-0.5, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);
}

/* XXX: This needs rather a lot of stack space ...
 * TODO: Rewrite in a saner and faster way.
 * Returns the size of the filled area. */
guint
flood_fill(cairo_surface_t *surface, cairo_surface_t *debug_surf, gint x, gint y, gint orig_color)
{
	guint img_width, img_height;
	guint stride;
	guint32 *pixels;
	guint result;

	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);

	pixels = (guint32*) cairo_image_surface_get_data(surface);
	stride = cairo_image_surface_get_stride(surface);

	if (x < 0)
		return 0;
	if (y < 0)
		return 0;

	if (x >= img_width)
		return 0;
	if (y >= img_height)
		return 0;

	if (GET_PIXEL(pixels, stride, x, y) != orig_color)
		return 0;

	/* Swap pixel value. */
	SET_PIXEL(pixels, stride, x, y, !orig_color);

	result = 1;

	result += flood_fill(surface, debug_surf, x+1, y, orig_color);
	result += flood_fill(surface, debug_surf, x, y+1, orig_color);
	result += flood_fill(surface, debug_surf, x-1, y, orig_color);
	result += flood_fill(surface, debug_surf, x, y-1, orig_color);

	if (debug_surf != NULL)
		mark_pixel(debug_surf, x, y);

	return result;
}


/*************************************************
 * Hough transformation!
 *************************************************/

typedef struct {
	guint32 *data;
	guint angle_bins;
	guint distance_bins;

	guint max_distance;
} hough_data;

/* Adds a point to the hough transformation. A (gaussion) filter is applied
 * at the same time.
 * I guess this is slightly less efficient than filtering it later, but it
 * means we do not require a larger result space for filtering. */
void
hough_add_point(hough_data *hough, guint x, guint y, guint filter_width, guint *filter_coff)
{
	guint angle_step;
	gdouble angle;
	gdouble r;
	gint r_bin, filt_bin;
	gint i;


	for (angle_step = 0; angle_step < hough->angle_bins; angle_step++) {
		angle = (2*G_PI * angle_step) / hough->angle_bins;

		/*r = abs((y - x*tan(angle)) / sqrt(1 + tan(angle)*tan(angle)));*/
		r = x * cos(angle) + y * sin(angle);

		/* Only add if r is larger than zero */
		r_bin = (int)round(hough->distance_bins * r / hough->max_distance);

		for (i = 0; i < filter_width; i++) {
			filt_bin = r_bin + i - filter_width / 2;
			
			if ((filt_bin >= 0) && (filt_bin < hough->distance_bins)) {
				hough->data[angle_step*hough->distance_bins + filt_bin] += filter_coff[i];
			}
		}
	}
}

guint
get_gaussion(gdouble sigma, guint **filter_coff)
{
	gint filt_length;
	gint center;
	gint i;
	g_assert(filter_coff != NULL);

	/* We just multiply the coefficients with 10, and we simply evaluate
	 * the exponential function. */

	/* Simply use two sigma for the filter width. */
	center = (gint)ceil(sigma * 2);
	filt_length = ((gint)ceil(sigma * 2)) * 2 + 1;

	*filter_coff = g_new(guint, filt_length);
	for (i = 0; i < center; i++) {
		(*filter_coff)[i] = floor(10*exp(-(gdouble)((i - center)*(i - center)) / pow(sigma, 2) / 2.0));
		(*filter_coff)[filt_length - i - 1] = (*filter_coff)[i];
	}
	(*filter_coff)[center] = 10;

	return filt_length;
}

/* Hough transforms the image.
 * 
 * */
hough_data*
hough_transform(cairo_surface_t *surface, guint angle_bins, guint distance_bins, gdouble sigma_px)
{
	guint img_width, img_height;
	guint stride;
	guint x, y;
	guint32 *pixels;
	guint filter_length;
	guint *filter;
	hough_data *result = g_malloc(sizeof(hough_data));
	result->data = NULL;

	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);

	result->angle_bins = angle_bins;
	result->distance_bins = distance_bins;
	result->max_distance = (guint) sqrt(img_width*img_width + img_height*img_height);

	result->data = g_malloc0_n(sizeof(*result->data), result->angle_bins*result->distance_bins);

	pixels = (guint32*) cairo_image_surface_get_data(surface);
	stride = cairo_image_surface_get_stride(surface);

	filter_length = get_gaussion(sigma_px * result->distance_bins / result->max_distance, &filter);

	for (y = 0; y < img_height; y++) {
		for (x = 0; x < img_width; x++) {
			if (GET_PIXEL(pixels, stride, x, y))
				hough_add_point(result, x, y, filter_length, filter);
		}
	}

	g_free(filter);

	return result;
}

static void
remove_line(cairo_surface_t *surface, gdouble width, gdouble r_max, gdouble phi_max, gboolean debug)
{
	guint img_width, img_height;
	cairo_t *cr;

	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);

	cr = cairo_create(surface);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	if (!debug) {
		cairo_set_source_rgba(cr, 0, 0, 0, 0);
	} else {
		cairo_set_source_rgba(cr, 1, 0, 0, 0.5);
	}
	cairo_set_line_width(cr, width);
	if (sin(phi_max) > 0.1) {
		cairo_move_to(cr, 0, r_max / sin(phi_max));
		cairo_line_to(cr, img_width, (r_max - img_width * cos(phi_max)) / sin(phi_max));
	} else {
		cairo_move_to(cr, r_max / cos(phi_max), 0);
		cairo_line_to(cr, (r_max - img_height * sin(phi_max)) / cos(phi_max), img_height);
	}
	cairo_stroke(cr);
	cairo_destroy(cr);
}

void
remove_maximum_line(cairo_surface_t *surface, cairo_surface_t *debug_surf, gdouble width)
{
	int r, phi;
	gdouble r_max, phi_max, maximum;
	hough_data *hough = hough_transform(surface, 60, 30, width / 2.0);

	/* Shut up the compiler*/
	r_max = 0;
	phi_max = 0;
	maximum = -1;

	/* Find global maximum. */
	for (phi = 0; phi < hough->angle_bins; phi++) {
		for (r = 0; r < hough->distance_bins; r++) {
			guint32 *cur = &hough->data[phi * hough->distance_bins + r];

			if (*cur > maximum) {
				maximum = *cur;
				r_max = r / (gdouble) hough->distance_bins * hough->max_distance;
				phi_max = phi / (gdouble) hough->angle_bins * G_PI*2;
			}
		}
	}

	remove_line(surface, width, r_max, phi_max, FALSE);
	if (debug_surf != NULL)
		remove_line(debug_surf, width, r_max, phi_max, TRUE);

	g_free(hough->data);
	g_free(hough);

	cairo_surface_flush(surface);
}


/* Generic pixel routines */
gint
count_black_pixel(cairo_surface_t *surface, gint x, gint y, gint width, gint height)
{
	guint32 *pixels;
	guint stride;
	guint img_width, img_height;

	pixels = (guint32*) cairo_image_surface_get_data(surface);
	img_width = cairo_image_surface_get_width(surface);
	img_height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);

	if (y < 0) {
		height += y;
		y = 0;
	}
	if (x < 0) {
		width += x;
		x = 0;
	}
	if ((width <= 0) || (height <= 0))
		return 0;
	if (x + width > img_width) {
		width = img_width - x;
	}
	if (y + height > img_height) {
		height = img_height - y;
	}

	return count_black_pixel_unchecked(pixels, stride, x, y, width, height);
}

gint
count_black_pixel_unchecked(guint32* pixels, guint32 stride, gint x, gint y, gint width, gint height)
{
	static guint8 bitcount[256];
	static gint table_initialized = FALSE;
	guint x_pos, y_pos;
	guint black_pixel = 0;

	if (G_UNLIKELY(!table_initialized)) {
		int i;

		for (i = 0; i < 256; i++) {
			int j;
			bitcount[i] = 0;
			for (j = i; j; j = j >> 1) {
				bitcount[i] += j & 0x1;
			}
		}
		table_initialized = TRUE;
	}

	for (y_pos = y; y_pos < y + height; y_pos++) {
#define COUNT_WORD(x) bitcount[(x) & 0xff] + bitcount[((x) >> 8) & 0xff] + bitcount[((x) >> 16) & 0xff] + bitcount[((x) >> 24) & 0xff]

		guint32 start_mask;
		guint32 end_mask;
		int start;
		int end;
		int pos;

#if G_BYTE_ORDER == G_BIG_ENDIAN
		start_mask = (1 << (x & 0x1f)) - 1;
		end_mask = -(1 << ((x + width) & 0x1f));
#else
		start_mask = -(1 << (x & 0x1f));
		end_mask = (1 << ((x + width) & 0x1f)) - 1;
#endif
		start = x >> 5;
		end = (x + width) >> 5;

		if (start == end) {
			black_pixel += COUNT_WORD(pixels[start + y_pos * stride / 4] & start_mask & end_mask);
		} else {
			black_pixel += COUNT_WORD(pixels[start + y_pos * stride / 4] & start_mask);
			for (pos = start + 1; pos < end; pos++) {
				black_pixel += COUNT_WORD(pixels[pos + y_pos * stride / 4]);
			}
			black_pixel += COUNT_WORD(pixels[end + y_pos * stride / 4] & end_mask);
		}

#undef COUNT_WORD
	}

	return black_pixel;
}

gint
count_black_pixel_masked(cairo_surface_t *surface, cairo_surface_t *mask, gint x, gint y)
{
	guint32 *pixels;
	guint32 *mask_pixels;
	guint stride;
	guint mask_stride;
    gint width, height;
    guint img_width, img_height;

    width = cairo_image_surface_get_width (mask);
    height = cairo_image_surface_get_height (mask);
	mask_pixels = (guint32*) cairo_image_surface_get_data (mask);
	mask_stride = cairo_image_surface_get_stride (mask);

	pixels = (guint32*) cairo_image_surface_get_data (surface);
	img_width = cairo_image_surface_get_width (surface);
	img_height = cairo_image_surface_get_height (surface);
	stride = cairo_image_surface_get_stride (surface);

    /* Ignore if the mask is not completely in the image ... */
	if (y < 0) {
		return 0;
	}
	if (x < 0) {
		return 0;
	}
	if ((width <= 0) || (height <= 0))
		return 0;
	if (x + width > img_width) {
		return 0;
	}
	if (y + height > img_height) {
		return 0;
	}

	return count_black_pixel_masked_unchecked(pixels, stride, mask_pixels, mask_stride, x, y, width, height);
}

gint
count_black_pixel_masked_unchecked(guint32* pixels, guint32 stride, guint32 *mask_pixels, guint32 mask_stride, gint x, gint y, gint width, gint height)
{
	static guint8 bitcount[256];
	static gint table_initialized = FALSE;
	guint y_pos;
	guint black_pixel = 0;

	if (G_UNLIKELY(!table_initialized)) {
		int i;

		for (i = 0; i < 256; i++) {
			int j;
			bitcount[i] = 0;
			for (j = i; j; j = j >> 1) {
				bitcount[i] += j & 0x1;
			}
		}
		table_initialized = TRUE;
	}

	for (y_pos = 0; y_pos < height; y_pos++) {
#define COUNT_WORD(x) bitcount[(x) & 0xff] + bitcount[((x) >> 8) & 0xff] + bitcount[((x) >> 16) & 0xff] + bitcount[((x) >> 24) & 0xff]

		guint32 end_mask;
		guint32 curr_pixels;
		int end;
		int pos;

#if G_BYTE_ORDER == G_BIG_ENDIAN
		end_mask = -(1 << (width & 0x1f));
#else
		end_mask = (1 << (width & 0x1f)) - 1;
#endif
		end = width >> 5;

		for (pos = 0; pos <= end; pos++) {
		    /* Note that a shift of 32 is not defined, it may also be 0. */
#if G_BYTE_ORDER == G_BIG_ENDIAN
		    curr_pixels = pixels[(x / 32) + pos + (y_pos + y) * stride / 4] << (x % 32);
		    curr_pixels |= pixels[(x / 32) + pos + (y_pos + y) * stride / 4 + 1] >> (32 - (x % 32));
#else
		    curr_pixels = pixels[(x / 32) + pos + (y_pos + y) * stride / 4] >> (x % 32);
		    curr_pixels |= pixels[((x + 31) / 32) + pos + (y_pos + y) * stride / 4] << (32 - (x % 32));
#endif

		    curr_pixels &= mask_pixels[pos + y_pos * mask_stride / 4];

            if (pos == end)
                curr_pixels &= end_mask;

			black_pixel += COUNT_WORD(curr_pixels);
		}

#undef COUNT_WORD
	}

	return black_pixel;
}



void
set_pixels_unchecked(guint32* pixels, guint32 stride, gint x, gint y, gint width, gint height, int value)
{
	guint x_pos, y_pos;

	for (y_pos = y; y_pos < y + height; y_pos++) {
		for (x_pos = x; x_pos < x + width; x_pos++) {
			SET_PIXEL(pixels, stride, x_pos, y_pos, value);
		}
	}
}

