/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
   Copyright (C) 1997 Torben Weis (weis@kde.org)
   Copyright (C) 1999 Anders Carlsson (andersca@gnu.org)
   Copyright (C) 2000 Helix Code, Inc.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>
#include <string.h>
#include "htmlcolor.h"
#include "htmlcolorset.h"
#include "htmlengine.h"
#include "htmlengine-save.h"
#include "htmlpainter.h"
#include "htmlsearch.h"
#include "htmltable.h"
#include "htmltablecell.h"


#define COLUMN_MIN(table, i)				\
	(g_array_index (table->columnMin, gint, i))

#define COLUMN_PREF(table, i)				\
	(g_array_index (table->columnPref, gint, i))

#define COLUMN_OPT(table, i)				\
	(g_array_index (table->columnOpt, gint, i))

#define ROW_HEIGHT(table, i)				\
	(g_array_index (table->rowHeights, gint, i))


HTMLTableClass html_table_class;
static HTMLObjectClass *parent_class = NULL;

static void alloc_cell (HTMLTable *table, gint r, gint c);
static void set_cell   (HTMLTable *table, gint r, gint c, HTMLTableCell *cell);
static void do_cspan   (HTMLTable *table, gint row, gint col, HTMLTableCell *cell);
static void do_rspan   (HTMLTable *table, gint row);



static inline gboolean
invalid_cell (HTMLTable *table, gint r, gint c)
{
	return (table->cells [r][c] == NULL
		|| c != table->cells [r][c]->col
		|| r != table->cells [r][c]->row);
}

/* HTMLObject methods.  */

static void
destroy (HTMLObject *o)
{
	HTMLTable *table = HTML_TABLE (o);
	HTMLTableCell *cell;
	guint r, c;

	/* for (r = 0; r < table->allocRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (cell->row != r || cell->col != c)
				continue;

			html_object_destroy (HTML_OBJECT (cell));
		}
		g_free (table->cells [r]);
		} */
	if (table->allocRows && table->totalCols)
		for (r = table->allocRows - 1; ; r--) {
			for (c = table->totalCols - 1; ; c--) {
				if ((cell = table->cells[r][c]) && cell->row == r && cell->col == c)
					html_object_destroy (HTML_OBJECT (cell));
				if (c == 0)
					break;
			}
			g_free (table->cells [r]);
			if (r == 0)
				break;
		}
	g_free (table->cells);

	g_array_free (table->columnMin, TRUE);
	g_array_free (table->columnPref, TRUE);
	g_array_free (table->columnOpt, TRUE);
	g_array_free (table->rowHeights, TRUE);

	if (table->bgColor)
		gdk_color_free (table->bgColor);

	HTML_OBJECT_CLASS (parent_class)->destroy (o);
}

static void
copy (HTMLObject *self,
      HTMLObject *dest)
{
	(* HTML_OBJECT_CLASS (parent_class)->copy) (self, dest);

	/* FIXME TODO*/
	g_warning ("HTMLTable::copy is broken.");
}

static gboolean
is_container (HTMLObject *object)
{
	return TRUE;
}

static void
forall (HTMLObject *self,
	HTMLEngine *e,
	HTMLObjectForallFunc func,
	gpointer data)
{
	HTMLTableCell *cell;
	HTMLTable *table;
	guint r, c;

	table = HTML_TABLE (self);

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];

			if (cell == NULL || cell->col != c || cell->row != r)
				continue;

			html_object_forall (HTML_OBJECT (cell), e, func, data);
		}
	}
	(* func) (self, e, data);
}



static void
previous_rows_do_cspan (HTMLTable *table, gint c)
{
	gint i;
	if (c)
		for (i=0; i < table->totalRows - 1; i++)
			if (table->cells [i][c - 1])
				do_cspan (table, i, c, table->cells [i][c - 1]);
}

static void
expand_columns (HTMLTable *table, gint num)
{
	gint r;

	for (r = 0; r < table->allocRows; r++) {
		table->cells [r] = g_renew (HTMLTableCell *, table->cells [r], table->totalCols + num);
		memset (table->cells [r] + table->totalCols, 0, num * sizeof (HTMLTableCell *));
	}
	table->totalCols += num;
}

static void
inc_columns (HTMLTable *table, gint num)
{
	expand_columns (table, num);
	previous_rows_do_cspan (table, table->totalCols - num);
}

static void
expand_rows (HTMLTable *table, gint num)
{
	gint r;

	table->cells = g_renew (HTMLTableCell **, table->cells, table->allocRows + num);

	for (r = table->allocRows; r < table->allocRows + num; r++) {
		table->cells [r] = g_new (HTMLTableCell *, table->totalCols);
		memset (table->cells [r], 0, table->totalCols * sizeof (HTMLTableCell *));
	}

	table->allocRows += num;
}

static void
inc_rows (HTMLTable *table, gint num)
{
	if (table->totalRows + num > table->allocRows)
		expand_rows (table, num + MAX (10, table->allocRows >> 2));
	table->totalRows += num;
	if (table->totalRows - num > 0)
		do_rspan (table, table->totalRows - num);
}

static inline gint
cell_end_col (HTMLTable *table, HTMLTableCell *cell)
{
	return MIN (table->totalCols, cell->col + cell->cspan);
}

static inline gint
cell_end_row (HTMLTable *table, HTMLTableCell *cell)
{
	return MIN (table->totalRows, cell->row + cell->rspan);
}

#define ARR(i) (g_array_index (array, gint, i))
#define LL (unsigned long long)
// #define LL (gdouble)

static gboolean
calc_column_width_step (HTMLTable *table, HTMLPainter *painter, GArray *array, gint *sizes,
			gint (*calc_fn)(HTMLObject *, HTMLPainter *), gint span)
{
	gboolean has_greater_cspan = FALSE;
	gint r, c, i, pixel_size = html_painter_get_pixel_size (painter);
	gint border_extra = table->border ? 2 : 0;

	for (c = 0; c < table->totalCols - span + 1; c++) {
		for (r = 0; r < table->totalRows; r++) {
			HTMLTableCell *cell = table->cells[r][c];
			gint col_width, span_width, cspan, new_width, added;

			if (!cell || cell->col != c || cell->row != r)
				continue;
			cspan = MIN (cell->cspan, table->totalCols - cell->col);
			if (cspan > span)
				has_greater_cspan = TRUE;
			if (cspan != span)
				continue;

			col_width  = (*calc_fn) (HTML_OBJECT (cell), painter) + pixel_size * table->padding * 2
				- (span - 1) * (table->padding * 2 + table->spacing + border_extra) * pixel_size;
			if (col_width <= 0)
				continue;
			span_width = ARR (cell->col + span) - ARR (cell->col);
			added      = 0;
			for (i = 0; i < span; i++) {
				if (span_width) {
					new_width = (LL col_width * (ARR (cell->col + i + 1) - ARR (cell->col)))
						/ span_width;
					if (LL col_width * (ARR (cell->col + i + 1) - ARR (cell->col))
					    - LL new_width * span_width > LL (new_width + 1) * span_width
					    - LL col_width * (ARR (cell->col + i + 1) - ARR (cell->col)))
						new_width ++;
				} else {
					new_width = added + col_width / span;
					if (col_width - LL span * new_width > LL span * (new_width + 1) - col_width)
						new_width ++;
				}
				new_width -= added;
				added     += new_width;

				if (sizes [cell->col + i] < new_width)
					sizes [cell->col + i] = new_width;
			}
			/* printf ("%d added %d col_width %d span_width %d\n",
			   col_width - added, added, col_width, span_width); */
		}
	}

	return has_greater_cspan;
}

static void
calc_column_width_template (HTMLTable *table, HTMLPainter *painter, GArray *array,
			    gint (*calc_fn)(HTMLObject *, HTMLPainter *), GArray *pref)
{
	gint c, add, span;
	gint pixel_size = html_painter_get_pixel_size (painter);
	gint border_extra = table->border ? 1 : 0;
	gint cell_space = pixel_size * (table->spacing + 2 * border_extra);
	gint *arr;
	gboolean next = TRUE;

	g_array_set_size (array, table->totalCols + 1);
	for (c = 0; c <= table->totalCols; c++)
		ARR (c) = pixel_size * (table->border + table->spacing);

	span = 1;
	while (span <= table->totalCols && next) {
		arr  = g_new0 (gint, table->totalCols);
		next = calc_column_width_step (table, painter, pref, arr, calc_fn, span);
		add  = 0;
		for (c = 0; c < table->totalCols; c++) {
			ARR (c + 1) += add;
			if (ARR (c + 1) - ARR (c) < arr [c]) {
				add += arr [c] - (ARR (c + 1) - ARR (c));
				ARR (c + 1) = ARR (c) + arr [c];
			}
		}
		g_free (arr);
		span ++;
	}

	for (c = 0; c < table->totalCols; c++)
		ARR (c + 1) += (c + 1) * cell_space;
}

static void
do_cspan (HTMLTable *table, gint row, gint col, HTMLTableCell *cell)
{
	gint i;

	g_assert (cell);
	g_assert (cell->col <= col);

	for (i=col - cell->col; i < cell->cspan && cell->col + i < table->totalCols; i++)
		set_cell (table, row, cell->col + i, cell);
}

static void
prev_col_do_cspan (HTMLTable *table, gint row)
{
	g_assert (row >= 0);

	/* add previous column cell which has cspan > 1 */
	while (table->col < table->totalCols && table->cells [row][table->col] != 0) {
		alloc_cell (table, row, table->col + table->cells [row][table->col]->cspan);
		do_cspan (table, row, table->col + 1, table->cells [row][table->col]);
		table->col += (table->cells [row][table->col])->cspan;
	}
}

static void
do_rspan (HTMLTable *table, gint row)
{
	gint i;

	g_assert (row > 0);

	for (i=0; i<table->totalCols; i++)
		if (table->cells [row - 1][i]
		    && (table->cells [row - 1][i])->row + (table->cells [row - 1][i])->rspan
		    > row) {
			set_cell (table, table->row, i, table->cells [table->row - 1][i]);
			do_cspan (table, table->row, i + 1, table->cells [table->row -1][i]);
		}
}

static void
set_cell (HTMLTable *table, gint r, gint c, HTMLTableCell *cell)
{
	if (!table->cells [r][c]) {
		table->cells [r][c] = cell;
		html_table_cell_link (cell);
		HTML_OBJECT (cell)->parent = HTML_OBJECT (table);
	}
}

static void
alloc_cell (HTMLTable *table, gint r, gint c)
{
	if (c >= table->totalCols)
		inc_columns (table, c + 1 - table->totalCols);

	if (r >= table->totalRows)
		inc_rows (table, r + 1 - table->totalRows);
}

#define RSPAN (MIN (cell->row + cell->rspan, table->totalRows) - cell->row - 1)

static void
calc_row_heights (HTMLTable *table,
		  HTMLPainter *painter)
{
	HTMLTableCell *cell;
	gint r, c, rl, height, pixel_size = html_painter_get_pixel_size (painter);
	gint border_extra = table->border ? 2 : 0;

	g_array_set_size (table->rowHeights, table->totalRows + 1);
	for (r = 0; r <= table->totalRows; r++)
		ROW_HEIGHT (table, r) = pixel_size * (table->border + table->spacing);

	for (r = 0; r < table->totalRows; r++) {
		if (ROW_HEIGHT (table, r + 1) < ROW_HEIGHT (table, r))
			ROW_HEIGHT (table, r + 1) = ROW_HEIGHT (table, r);
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell && cell->row == r && cell->col == c) {
				rl = cell_end_row (table, cell);
				height = (ROW_HEIGHT (table, cell->row)
					  + HTML_OBJECT (cell)->ascent + HTML_OBJECT (cell)->descent
					  + (pixel_size * (table->spacing + border_extra)));
				if (height > ROW_HEIGHT (table, rl))
					ROW_HEIGHT (table, rl) = height;
			}
		}
	}
}

static void
calc_cells_size (HTMLTable *table, HTMLPainter *painter)
{
	HTMLTableCell *cell;
	gint r, c;

	for (r = 0; r < table->totalRows; r++)
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell && cell->col == c && cell->row == r)
				html_object_calc_size (HTML_OBJECT (cell), painter);
		}
}

static void
set_cells_position (HTMLTable *table, HTMLPainter *painter)
{
	HTMLTableCell *cell;
	gint r, c, rl, pixel_size = html_painter_get_pixel_size (painter);
	gint border_extra = table->border ? 1 : 0;

	for (r = 0; r < table->totalRows; r++)
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell && cell->row == r && cell->col == c) {
				rl = cell_end_row (table, cell);
				HTML_OBJECT (cell)->x = COLUMN_OPT (table, c) + pixel_size * border_extra;
				HTML_OBJECT (cell)->y = ROW_HEIGHT (table, rl) + pixel_size * (- table->spacing)
					- HTML_OBJECT (cell)->descent;
				html_object_set_max_ascent (HTML_OBJECT (cell), painter,
							    ROW_HEIGHT (table, rl) - ROW_HEIGHT (table, cell->row)
							    - pixel_size * (table->spacing + border_extra));
			}
		}
}

static gboolean
calc_size (HTMLObject *o,
	   HTMLPainter *painter)
{
	HTMLTable *table = HTML_TABLE (o);
	gint old_width, old_ascent, pixel_size;

	old_width   = o->width;
	old_ascent  = o->ascent;
	pixel_size  = html_painter_get_pixel_size (painter);

	calc_cells_size (table, painter);
	calc_row_heights (table, painter);
	set_cells_position (table, painter);

	o->ascent = ROW_HEIGHT (table, table->totalRows) + pixel_size * table->border;
	o->width  = COLUMN_OPT (table, table->totalCols) + pixel_size * table->border;
	
	if (o->width != old_width || o->ascent != old_ascent)
		return TRUE;

	return FALSE;
}

#define NEW_INDEX(l,h) ((l+h) >> 1)
#undef  ARR
#define ARR(i) g_array_index (a, gint, i)

static gint
bin_search_index (GArray *a, gint l, gint h, gint val)
{
	gint i;

	i = NEW_INDEX (l, h);

	while (l < h && val != ARR (i)) {
		if (val < ARR (i))
			h = i - 1;
		else
			l = i + 1;
		i = NEW_INDEX (l, h);
	}

	return i;
}

static inline gint
to_index (gint val, gint l, gint h)
{
	return MIN (MAX (val, l), h);
}

static void
get_bounds (HTMLTable *table, gint x, gint y, gint width, gint height, gint *sc, gint *ec, gint *sr, gint *er)
{
	*sr = to_index (bin_search_index (table->rowHeights, 0, table->totalRows, y), 0, table->totalRows - 1);
	if (y < ROW_HEIGHT (table, *sr) && (*sr) > 0)
		(*sr)--;
	*er = to_index (bin_search_index (table->rowHeights, *sr, table->totalRows, y + height), 0, table->totalRows - 1);
	if (y > ROW_HEIGHT (table, *er) && (*er) < table->totalRows - 1)
		(*er)++;

	*sc = to_index (bin_search_index (table->columnOpt, 0, table->totalCols, x), 0, table->totalCols-1);
	if (x < COLUMN_OPT (table, *sc) && (*sc) > 0)
		(*sc)--;
	*ec = to_index (bin_search_index (table->columnOpt, *sc, table->totalCols, x + width), 0, table->totalCols - 1);
	if (x > COLUMN_OPT (table, *ec) && (*ec) < table->totalCols - 1)
		(*ec)++;
}

static void
draw (HTMLObject *o,
      HTMLPainter *p, 
      gint x, gint y,
      gint width, gint height,
      gint tx, gint ty)
{
	HTMLTableCell *cell;
	HTMLTable *table = HTML_TABLE (o);
	gint pixel_size;
	gint r, c, start_row, end_row, start_col, end_col;
	ArtIRect paint;

	html_object_calc_intersection (o, &paint, x, y, width, height);
	if (art_irect_empty (&paint))
		return;

	pixel_size = html_painter_get_pixel_size (p);
	
	tx += o->x;
	ty += o->y - o->ascent;

	/* Draw the cells */
	get_bounds (table, x, y, width, height, &start_col, &end_col, &start_row, &end_row);
	for (r = start_row; r <= end_row; r++) {
		for (c = start_col; c <= end_col; c++) {
			cell = table->cells[r][c];

			if (cell == NULL)
				continue;
			if (c < end_col && cell == table->cells [r][c + 1])
				continue;
			if (r < end_row && table->cells [r + 1][c] == cell)
				continue;

			html_object_draw (HTML_OBJECT (cell), p, 
					  x - o->x, y - o->y + o->ascent,
					  width,
					  height,
					  tx, ty);
		}
	}

	/* Draw the border */
	if (table->border > 0 && table->rowHeights->len > 0) {
		gint capOffset;

		capOffset = 0;

		if (table->caption && table->capAlign == HTML_VALIGN_TOP)
			g_print ("FIXME: Support captions\n");

		html_painter_draw_panel (p, html_object_get_bg_color (o->parent, p),
					 tx, ty + capOffset, 
					 HTML_OBJECT (table)->width,
					 ROW_HEIGHT (table, table->totalRows) +
					 pixel_size * table->border, GTK_HTML_ETCH_OUT,
					 pixel_size * table->border);
		
		/* Draw borders around each cell */
		for (r = start_row; r <= end_row; r++) {
			for (c = start_col; c <= end_col; c++) {
				if ((cell = table->cells[r][c]) == 0)
					continue;
				if (c < end_col &&
				    cell == table->cells[r][c + 1])
					continue;
				if (r < end_row &&
				    table->cells[r + 1][c] == cell)
					continue;

				html_painter_draw_panel (p, html_object_get_bg_color (HTML_OBJECT (cell), p),
							 tx + COLUMN_OPT (table, cell->col),
							 ty + ROW_HEIGHT (table, cell->row) + capOffset,
							 (COLUMN_OPT (table, c + 1)
							  - COLUMN_OPT (table, cell->col)
							  - pixel_size * table->spacing),
							 (ROW_HEIGHT (table, r + 1)
							  - ROW_HEIGHT (table, cell->row)
							  - pixel_size * table->spacing),
							 GTK_HTML_ETCH_IN, pixel_size);
						      
			}
		}
	}
}

static gint
calc_min_width (HTMLObject *o,
		HTMLPainter *painter)
{
	HTMLTable *table = HTML_TABLE (o);

	calc_column_width_template (table, painter, table->columnMin, html_object_calc_min_width, table->columnMin);

	return o->flags & HTML_OBJECT_FLAG_FIXEDWIDTH
		? MAX (html_painter_get_pixel_size (painter) * table->specified_width,
		       COLUMN_MIN (table, table->totalCols) + table->border * html_painter_get_pixel_size (painter))
		: COLUMN_MIN (table, table->totalCols) + table->border * html_painter_get_pixel_size (painter);
}

static gint
calc_preferred_width (HTMLObject *o,
		      HTMLPainter *painter)
{
	HTMLTable *table = HTML_TABLE (o);

	calc_column_width_template (table, painter, table->columnPref,
				    html_object_calc_preferred_width, table->columnPref);
	calc_column_width_template (table, painter, table->columnFixed,
				    (gint (*)(HTMLObject *, HTMLPainter *)) html_table_cell_get_fixed_width,
				    table->columnPref);

	return o->flags & HTML_OBJECT_FLAG_FIXEDWIDTH
		? MAX (html_painter_get_pixel_size (painter) * table->specified_width, html_object_calc_min_width (o, painter))
		: COLUMN_PREF (table, table->totalCols) + table->border * html_painter_get_pixel_size (painter);
}

#define PERC(c) (col_percent [c + 1] - col_percent [c])

static gboolean
calc_percentage_step (HTMLTable *table, gint *col_percent, gint *span_percent, gint span)
{
	HTMLTableCell *cell;
	gboolean higher_span = FALSE;
	gint r, c, cl, cspan;

	for (c = 0; c < table->totalCols; c++)
		for (r = 0; r < table->totalRows; r++) {
			cell = table->cells[r][c];

			if (!cell || cell->col != c || cell->row != r)
				continue;

			if (HTML_OBJECT (cell)->flags & HTML_OBJECT_FLAG_FIXEDWIDTH || HTML_OBJECT (cell)->percent <= 0)
				continue;

			cspan = MIN (cell->cspan, table->totalCols - cell->col);
			if (cspan > span)
				higher_span = TRUE;
			if (cspan != span)
				continue;

			cl = cell_end_col (table, cell);
			if (col_percent [cl] - col_percent [c] < HTML_OBJECT (cell)->percent) {
				gint cp, part, added, pleft, not_percented, np;

				not_percented = 0;
				for (cp = 0; cp < span; cp++)
					if (!PERC (c + cp))
						not_percented ++;

				np    = 1;
				added = 0;
				pleft = HTML_OBJECT (cell)->percent - (col_percent [cl] - col_percent [c]);
				for (cp = 0; cp < span; cp++) {
					if (not_percented) {
						if (!PERC (c + cp)) {
							part = np * pleft / not_percented;
							if (np * pleft - part * not_percented >
							    (part + 1) * not_percented - np * pleft)
								part++;
							np ++;
						}
					} else {
						part = ((col_percent [c + cp + 1] - col_percent [c]) * pleft)
							/ (col_percent [cl] - col_percent [cell->col]);
						if ((col_percent [c + cp + 1] - col_percent [c]) * pleft
						    - part * (col_percent [cl] - col_percent [c])
						    > (part + 1) * (col_percent [cl] - col_percent [c])
						    - (col_percent [c + cp + 1] - col_percent [c]) * pleft)
							part ++;
					}
					part  -= added;
					added += part;
					span_percent [c + cp] = PERC (c + cp) + part;
				}
			}
		}

	return higher_span;
}

static void
calc_col_percentage (HTMLTable *table, gint *col_percent)
{
	gint c, span, *percent, add;
	gboolean next = TRUE;

	percent = g_new0 (gint, table->totalCols);
	for (span = 1; next && span <= table->totalCols; span ++) {
		for (c = 0; c < table->totalCols; c++)
			percent [c] = 0;

		next    = calc_percentage_step (table, col_percent, percent, span);
		add     = 0;

		for (c = 0; c < table->totalCols; c++) {
			col_percent [c + 1] += add;
			if (PERC (c) < percent [c]) {
				add += percent [c] - PERC (c);
				col_percent [c + 1] = col_percent [c] + percent [c];
			}
		}
	}
	g_free (percent);
}

static gint
calc_not_percented (HTMLTable *table, gint *col_percent)
{
	gint c, not_percented;

	not_percented = 0;
	for (c = 0; c < table->totalCols; c++)
		if (col_percent [c + 1] == col_percent [c])
			not_percented ++;

	return not_percented;
}

static gint
divide_into_percented (HTMLTable *table, gint *col_percent, gint *max_size, gint max_width, gint left)
{
	gint added, add, c, to_fill, request, filled;

	to_fill = 0;
	for (c = 0; c < table->totalCols; c++) {
		request = (LL max_width * (PERC (c))) / 100;
		if (max_size [c] < request)
			to_fill += request - max_size [c];
	}

	/* printf ("to fill %d\n", to_fill); */
	left  = MIN (to_fill, left);
	added  = 0;
	filled = 0;
	if (left) {
		for (c = 0; c < table->totalCols; c++) {
			request = (LL max_width * (PERC (c))) / 100;
			if (max_size [c] < request) {
				add     = LL left * (request - max_size [c] + filled) / to_fill;
				if (LL left * (request - max_size [c] + filled) - LL add * to_fill >
				    LL (add + 1) * to_fill - LL left * (request - max_size [c] + filled))
					add ++;
				add          -= added;
				added        += add;
				filled       += request - max_size [c];
				max_size [c] += add;
			}
		}
	}
	/* printf ("%d added %d left %d\n", left - added, added, left); */

	return added;
}

#define PREF(i) (g_array_index (pref, gint, i))

static gboolean
calc_lowest_fill (HTMLTable *table, GArray *pref, gint *max_size, gint *col_percent, gint pixel_size,
		  gint *ret_col, gint *ret_total_pref)
{
	gint c, pw, border_extra = table->border ? 2 : 0, min_fill = COLUMN_PREF (table, table->totalCols);

	*ret_total_pref = 0;
	for (c = 0; c < table->totalCols; c++)
		if (col_percent [c + 1] == col_percent [c]) {
			pw = PREF (c + 1) - PREF (c)
				- pixel_size * (table->spacing + border_extra);
			/* printf ("col %d pw %d size %d\n", c, pw, max_size [c]); */
			if (max_size [c] < pw) {
				if (pw - max_size [c] < min_fill) {
					*ret_col = c;
					min_fill = pw - max_size [c];
				}

				(*ret_total_pref) += pw;
			}
		}

	return min_fill == COLUMN_PREF (table, table->totalCols) ? FALSE : TRUE;
}

inline static gint
divide_upto_preferred_width (HTMLTable *table, HTMLPainter *painter, GArray *pref,
			     gint *col_percent, gint *max_size, gint left)
{
	gint added, part, c, pw, pixel_size = html_painter_get_pixel_size (painter);
	gint total_fill, min_col, min_fill, min_pw, processed_pw, border_extra = table->border ? 2 : 0;

	while (left && calc_lowest_fill (table, pref, max_size, col_percent, pixel_size, &min_col, &total_fill)) {
		min_pw   = PREF (min_col + 1) - PREF (min_col)
			- pixel_size * (table->spacing + border_extra);
		min_fill = MIN (min_pw - max_size [min_col], ((gdouble) min_pw * left) / total_fill);
		if (min_fill <= 0)
			break;
		/* printf ("min_col %d min_pw %d MIN(%d,%d)\n", min_col, min_pw, min_pw - max_size [min_col],
		   (gint)(((gdouble) min_pw * left) / total_fill)); */

		if (min_fill == min_pw - max_size [min_col]) {
			/* first add to minimal one */
			max_size [min_col] += min_fill;
			left -= min_fill;
			total_fill -= min_pw;
			/* printf ("min satisfied %d, (%d=%d)left: %d\n", min_fill, max_size [min_col], min_pw, left); */
		}
		if (!left)
			break;

		processed_pw = 0;
		added = 0;

		for (c = 0; c < table->totalCols; c++) {
			if (col_percent [c + 1] == col_percent [c]) {
				pw = PREF (c + 1) - PREF (c)
					- pixel_size * (table->spacing + border_extra);
				if (max_size [c] < pw) {
					processed_pw += pw;
					part = (LL min_fill * processed_pw) / min_pw;
					if (LL min_fill * processed_pw - LL part * min_pw
					    > LL (part + 1) * min_pw - LL min_fill * processed_pw)
						part ++;
					part         -= added;
					added        += part;
					max_size [c] += part;
					left         -= part;
				        /* printf ("cell %d add: %d --> %d\n", c, part, max_size [c]); */
				}
			}
		}
	}

	return left;
}

inline static void
divide_left_by_preferred_width (HTMLTable *table, HTMLPainter *painter,
				gint *col_percent, gint *max_size, gint left)
{
	gint added, part, c, pref, pw, pixel_size = html_painter_get_pixel_size (painter);
	gint total_fill, processed_pw, border_extra = table->border ? 2 : 0;

	/* printf ("round 2 left: %d\n", left); */
	pref = 0;
	for (c = 0; c < table->totalCols; c++)
		if (col_percent [c + 1] == col_percent [c])
			pref += COLUMN_PREF (table, c + 1) - COLUMN_PREF (table, c)
				- pixel_size * (table->spacing + border_extra);
			/* printf ("col pref: %d size: %d\n", COLUMN_PREF (table, c + 1)
			   - COLUMN_PREF (table, c)
			   - pixel_size * (table->spacing + border_extra), max_size [c]); */

	added        = 0;
	processed_pw = 0;
	total_fill   = left;

	if (pref)
		for (c = 0; c < table->totalCols; c++) {
			if (col_percent [c + 1] == col_percent [c]) {
				pw  = COLUMN_PREF (table, c + 1) - COLUMN_PREF (table, c)
					- pixel_size * (table->spacing + border_extra);
				processed_pw += pw;
				part = (LL total_fill * processed_pw) / pref;
				/* printf ("pw %d part %d total %d processed %d\n",
				   pw, part, total_fill, processed_pw); */
				if (LL total_fill * processed_pw - LL part * pref
				    > LL (part + 1) * pref - LL total_fill * processed_pw)
					part ++;
				part         -= added;
				max_size [c] += part;
				added        += part;
				left         -= part;
				/* printf ("col %d (add %d) --> %d (pw=%d)\n", c, part, max_size [c], pw); */
			}
		}
		
	/* printf ("------------------------------------\n*left*: %d\n-------------------------------\n", left); */
}

inline static void
divide_into_variable_all (HTMLTable *table, HTMLPainter *painter,
			  gint *col_percent, gint *max_size, gint left)
{
	/* printf ("left %d cols: %d\n", left, table->totalCols); */
	html_object_calc_preferred_width (HTML_OBJECT (table), painter);

	left = divide_upto_preferred_width (table, painter, table->columnFixed, col_percent, max_size, left);
	left = divide_upto_preferred_width (table, painter, table->columnPref,  col_percent, max_size, left);

	if (left)
		divide_left_by_preferred_width (table, painter, col_percent, max_size, left);
}

inline static void
divide_into_percented_all (HTMLTable *table, gint *col_percent, gint *max_size, gint max_width, gint left)
{
	gdouble sub_percent, percent;
	gint c, sub_width, width;
	gboolean all_active, *active;

	active = g_new (gboolean, table->totalCols);
	for (c = 0; c < table->totalCols; c++)
		active [c] = TRUE;

	percent = col_percent [table->totalCols];
	width   = max_width;
	do {
		sub_percent = 0.0;
		sub_width   = width;
		all_active  = TRUE;
		for (c = 0; c < table->totalCols; c++) {
			if (active [c]) {
				if (max_size [c] < ((gdouble) width * PERC (c)) / percent)
					sub_percent += PERC (c);
				else {
					sub_width -= max_size [c];
					all_active = FALSE;
					active [c] = FALSE;
				}
			}
		}
		percent = sub_percent;
		width   = sub_width;
	} while (!all_active);

	/* printf ("sub_width %d\n", sub_width); */
	for (c = 0; c < table->totalCols; c++)
		if (active [c] && max_size [c] < ((gdouble) width * PERC (c)) / percent)
			max_size [c] = ((gdouble) width) * (PERC (c)) / percent;
}

#define CSPAN (MIN (cell->col + cell->cspan, table->totalCols) - cell->col - 1)

static void
set_cells_max_width (HTMLTable *table, HTMLPainter *painter, gint *max_size)
{
	HTMLTableCell *cell;
	gint r, c, size, pixel_size = html_painter_get_pixel_size (painter);
	gint border_extra = table->border ? 2 : 0;

	for (r = 0; r < table->totalRows; r++)
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell) {
				size = max_size [c] + (cell->col != c ? size : 0);
				if (cell_end_col (table, cell) - 1 == c && cell->row == r)
					html_object_set_max_width (HTML_OBJECT (cell), painter, size
								   - pixel_size * table->padding * 2
								   + pixel_size * (table->spacing + border_extra) * CSPAN);
			}
		}
}

static void
set_columns_optimal_width (HTMLTable *table, gint *max_size, gint pixel_size)
{
	gint c;

	g_array_set_size (table->columnOpt, table->totalCols + 1);
	COLUMN_OPT (table, 0) = COLUMN_MIN (table, 0);

	for (c = 0; c < table->totalCols; c++)
		COLUMN_OPT (table, c + 1) = COLUMN_OPT (table, c) + max_size [c]
			+ pixel_size * (table->spacing + (table->border ? 2 : 0));
}

static void
divide_left_width (HTMLTable *table, HTMLPainter *painter, gint *max_size, gint max_width, gint width_left)
{
	gint not_percented, c;
	gint *col_percent;

	if (!width_left)
		return;

	col_percent  = g_new (gint, table->totalCols + 1);
	for (c = 0; c <= table->totalCols; c++)
		col_percent [c] = 0;

	calc_col_percentage (table, col_percent);
	/* printf ("width_left: %d percented: %d\n", width_left, col_percent [table->totalCols]); */
	not_percented  = calc_not_percented (table, col_percent);
	if (not_percented < table->totalCols)
		width_left    -= divide_into_percented (table, col_percent, max_size, max_width, width_left);

	/* printf ("width_left: %d\n", width_left); */
	if (width_left > 0) {
		if (not_percented)
			divide_into_variable_all  (table, painter, col_percent, max_size, width_left);
		else
			divide_into_percented_all (table, col_percent, max_size, max_width, width_left);
	}

	g_free (col_percent);
}

static gint *
alloc_max_size (HTMLTable *table, gint pixel_size)
{
	gint *max_size, c, border_extra = table->border ? 2 : 0;

	max_size = g_new (gint, table->totalCols);
	for (c = 0; c < table->totalCols; c++)
		max_size [c] = COLUMN_MIN (table, c+1) - COLUMN_MIN (table, c) - pixel_size * (table->spacing + border_extra);

	return max_size;
}

static void
html_table_set_max_width (HTMLObject *o,
			  HTMLPainter *painter,
			  gint max_width)
{
	HTMLTable *table = HTML_TABLE (o);
	gint *max_size, pixel_size, glue, border_extra = table->border ? 2 : 0;

	html_object_calc_min_width (HTML_OBJECT (table), painter);

	/* printf ("max_width: %d\n", max_width); */
	pixel_size   = html_painter_get_pixel_size (painter);
	o->max_width = max_width;
	max_width    = o->flags & HTML_OBJECT_FLAG_FIXEDWIDTH
		? max_width = pixel_size * table->specified_width
		: (o->percent
		   ? MAX (((gdouble) MIN (100, o->percent) / 100 * max_width),
			  html_object_calc_min_width (o, painter))
		   : MIN (html_object_calc_preferred_width (HTML_OBJECT (table), painter), max_width));

	/* printf ("corected to: %d\n", max_width); */
	glue         = pixel_size * (table->border * 2 + (table->totalCols + 1) * table->spacing
				     + (table->totalCols * border_extra));
	max_width   -= glue;
	max_size     = alloc_max_size (table, pixel_size);

	divide_left_width (table, painter, max_size, max_width,
			   max_width + glue - COLUMN_MIN (table, table->totalCols)
			   - pixel_size * table->border);

	set_cells_max_width (table, painter, max_size);
	set_columns_optimal_width (table, max_size, pixel_size);

	/* printf ("max_width %d opt_width %d\n", o->max_width, COLUMN_OPT (table, table->totalCols) + ); */

	g_free (max_size);
}

static void
reset (HTMLObject *o)
{
	HTMLTable *table = HTML_TABLE (o);
	HTMLTableCell *cell;
	guint r, c;

	for (r = 0; r < table->totalRows; r++)
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell && cell->row == r && cell->col == c)
				html_object_reset (HTML_OBJECT (cell));
		}
}

static HTMLAnchor *
find_anchor (HTMLObject *self, const char *name, gint *x, gint *y)
{
	HTMLTable *table;
	HTMLTableCell *cell;
	HTMLAnchor *anchor;
	unsigned int r, c;

	table = HTML_TABLE (self);

	*x += self->x;
	*y += self->y - self->ascent;

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;

			if (c < table->totalCols - 1
			    && cell == table->cells[r][c+1])
				continue;
			if (r < table->totalRows - 1
			    && table->cells[r+1][c] == cell)
				continue;

			anchor = html_object_find_anchor (HTML_OBJECT (cell), 
							  name, x, y);

			if (anchor != NULL)
				return anchor;
		}
	}
	*x -= self->x;
	*y -= self->y - self->ascent;

	return 0;
}


static HTMLObject *
check_point (HTMLObject *self,
	     HTMLPainter *painter,
	     gint x, gint y,
	     guint *offset_return,
	     gboolean for_cursor)
{
	HTMLTableCell *cell;
	HTMLObject *obj;
	HTMLTable *table;
	gint r, c, start_row, end_row, start_col, end_col;

	if (x < self->x || x >= self->x + self->width
	    || y >= self->y + self->descent || y < self->y - self->ascent)
		return NULL;

	table = HTML_TABLE (self);

	x -= self->x;
	y -= self->y - self->ascent;

	get_bounds (table, x, y, 0, 0, &start_col, &end_col, &start_row, &end_row);
	for (r = start_row; r <= end_row; r++) {
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell == NULL)
				continue;

			if (c < end_col - 1 && cell == table->cells[r][c+1])
				continue;
			if (r < end_row - 1 && table->cells[r+1][c] == cell)
				continue;

			obj = html_object_check_point (HTML_OBJECT (cell), painter, x, y, offset_return, for_cursor);
			if (obj != NULL)
				return obj;
		}
	}

	if (for_cursor) {

		/* check before */
		for (c=0; c<table->totalCols; c++)
			if (table->cells [start_row][c])
				break;
		if (table->cells [start_row][c]) {
			cell = table->cells [start_row][c];
			if (x < HTML_OBJECT (cell)->x || y < HTML_OBJECT (cell)->y - HTML_OBJECT (cell)->ascent) {
				obj = html_object_check_point (HTML_OBJECT (cell), painter,
							       HTML_OBJECT (cell)->x,
							       HTML_OBJECT (cell)->y - HTML_OBJECT (cell)->ascent,
							       offset_return, for_cursor);
				if (obj)
					return obj;
			}
		}
		/* check after */
		for (c=table->totalCols - 1; c >= 0; c--)
			if (table->cells [start_row][c])
				break;
		if (table->cells [start_row][c]) {
			cell = table->cells [start_row][c];
			if (x > HTML_OBJECT (cell)->x + HTML_OBJECT (cell)->width - 1
			    || y > HTML_OBJECT (cell)->y + HTML_OBJECT (cell)->descent - 1) {
				obj = html_object_check_point (HTML_OBJECT (cell), painter,
							       HTML_OBJECT (cell)->x + HTML_OBJECT (cell)->width - 1,
							       HTML_OBJECT (cell)->y + HTML_OBJECT (cell)->descent - 1,
							       offset_return, for_cursor);
				if (obj)
					return obj;
			}
		}
	}

	return NULL;
}

static gboolean
search (HTMLObject *obj, HTMLSearch *info)
{
	HTMLTable *table = HTML_TABLE (obj);
	HTMLTableCell *cell;
	HTMLObject    *cur = NULL;
	guint r, c, i, j;
	gboolean next = FALSE;

	/* search_next? */
	if (html_search_child_on_stack (info, obj)) {
		cur  = html_search_pop (info);
		next = TRUE;
	}

	if (info->forward) {
		for (r = 0; r < table->totalRows; r++) {
			for (c = 0; c < table->totalCols; c++) {

				if ((cell = table->cells[r][c]) == 0)
					continue;
				if (c < table->totalCols - 1 &&
				    cell == table->cells[r][c + 1])
					continue;
				if (r < table->totalRows - 1 &&
				    cell == table->cells[r + 1][c])
					continue;

				if (next && cur) {
					if (HTML_OBJECT (cell) == cur) {
						cur = NULL;
					}
					continue;
				}

				html_search_push (info, HTML_OBJECT (cell));
				if (html_object_search (HTML_OBJECT (cell), info)) {
					return TRUE;
				}
				html_search_pop (info);
			}
		}
	} else {
		for (i = 0, r = table->totalRows - 1; i < table->totalRows; i++, r--) {
			for (j = 0, c = table->totalCols - 1; j < table->totalCols; j++, c--) {

				if ((cell = table->cells[r][c]) == 0)
					continue;
				if (c < table->totalCols - 1 &&
				    cell == table->cells[r][c + 1])
					continue;
				if (r < table->totalRows - 1 &&
				    cell == table->cells[r + 1][c])
					continue;

				if (next && cur) {
					if (HTML_OBJECT (cell) == cur) {
						cur = NULL;
					}
					continue;
				}

				html_search_push (info, HTML_OBJECT (cell));
				if (html_object_search (HTML_OBJECT (cell), info)) {
					return TRUE;
				}
				html_search_pop (info);
			}
		}
	}

	if (next) {
		return html_search_next_parent (info);
	}

	return FALSE;
}

static HTMLFitType
fit_line (HTMLObject *o,
	  HTMLPainter *painter,
	  gboolean start_of_line,
	  gboolean first_run,
	  gint width_left)
{
	return HTML_FIT_COMPLETE;
}

static void
append_selection_string (HTMLObject *self,
			 GString *buffer)
{
	HTMLTable *table;
	HTMLTableCell *cell;
	gboolean tab;
	gint r, c, len, rlen, tabs;

	table = HTML_TABLE (self);
	for (r = 0; r < table->totalRows; r++) {
		tab = FALSE;
		tabs = 0;
		rlen = buffer->len;
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    table->cells[r + 1][c] == cell)
				continue;
			if (tab) {
				g_string_append_c (buffer, '\t');
				tabs++;
			}
			len = buffer->len;
			html_object_append_selection_string (HTML_OBJECT (cell), buffer);
			/* remove last \n from added text */
			if (buffer->len != len && buffer->str [buffer->len-1] == '\n')
				g_string_truncate (buffer, buffer->len - 1);
			tab = TRUE;
		}
		if (rlen + tabs == buffer->len)
			g_string_truncate (buffer, rlen);
		else if (r + 1 < table->totalRows)
			g_string_append_c (buffer, '\n');
	}
}

static HTMLObject *
head (HTMLObject *self)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	for (r = 0; r < table->totalRows; r++)
		for (c = 0; c < table->totalCols; c++) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
	return NULL;
}

static HTMLObject *
tail (HTMLObject *self)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	for (r = table->totalRows - 1; r >= 0; r--)
		for (c = table->totalCols - 1; c >= 0; c--) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
	return NULL;
}

static HTMLObject *
next (HTMLObject *self, HTMLObject *child)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	r = HTML_TABLE_CELL (child)->row;
	c = HTML_TABLE_CELL (child)->col + 1;
	for (; r < table->totalRows; r++) {
		for (; c < table->totalCols; c++) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
		c = 0;
	}
	return NULL;
}

static HTMLObject *
prev (HTMLObject *self, HTMLObject *child)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	r = HTML_TABLE_CELL (child)->row;
	c = HTML_TABLE_CELL (child)->col - 1;
	for (; r >= 0; r--) {
		for (; c >=0; c--) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
		c = table->totalCols-1;
	}
	return NULL;
}

#define SB if (!html_engine_save_output_string (state,
#define SE )) return FALSE

static gboolean
save (HTMLObject *self,
      HTMLEngineSaveState *state)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);

	SB "<TABLE" SE;
	if (table->bgColor)
		SB " BGCOLOR=\"#%02x%02x%02x\"",
			table->bgColor->red >> 8,
			table->bgColor->green >> 8,
			table->bgColor->blue >> 8 SE;
	if (table->bgPixmap) {
		gchar * url = html_image_resolve_image_url (state->engine->widget, table->bgPixmap->url);
		SB " BACKGROUND=\"%s\"", url SE;
		g_free (url);
	}
	if (table->spacing != 2)
		SB " CELLSPACING=\"%d\"", table->spacing SE;
	if (table->padding != 1)
		SB " CELLPADDING=\"%d\"", table->padding SE;
	if (self->percent > 0) {
		SB " WIDTH=\"%d%%\"", self->percent SE;
	} else if (self->flags & HTML_OBJECT_FLAG_FIXEDWIDTH)
		SB " WIDTH=\"%d\"", table->specified_width SE;
	if (table->border)
		SB " BORDER=\"%d\"", table->border SE;
	SB ">\n" SE;

	for (r = 0; r < table->totalRows; r++) {
		SB "<TR>\n" SE;
		for (c = 0; c < table->totalCols; c++) {
			if (!table->cells [r][c]
			    || table->cells [r][c]->row != r
			    || table->cells [r][c]->col != c)
				continue;
			if (!html_object_save (HTML_OBJECT (table->cells [r][c]), state))
				return FALSE;
		}
		SB "</TR>\n" SE;
	}
	SB "</TABLE>\n" SE;

	return TRUE;
}

static gboolean
save_plain (HTMLObject *self,
	    HTMLEngineSaveState *state,
	    gint requested_width)
{
	HTMLTable *table;
	gint r, c;
	gboolean result = TRUE;

	table = HTML_TABLE (self);

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			if (!table->cells [r][c]
			    || table->cells [r][c]->row != r
			    || table->cells [r][c]->col != c)
				continue;
			/*  FIXME the width calculation for the column here is completely broken */
			result &= html_object_save_plain (HTML_OBJECT (table->cells [r][c]), state, requested_width / table->totalCols);
		}
	}

	return result;
}

static gboolean
check_row_split (HTMLTable *table, gint r, gint *min_y)
{
	HTMLTableCell *cell;
	gboolean changed = FALSE;
	gint c, cs;

	for (c = 0; c < table->totalCols; c++) {
		gint y1, y2;

		cell = table->cells[r][c];
		if (cell == NULL || cell->col != c)
			continue;

		y1 = HTML_OBJECT (cell)->y - HTML_OBJECT (cell)->ascent;
		y2 = HTML_OBJECT (cell)->y + HTML_OBJECT (cell)->descent;

		if (y1 <= *min_y && *min_y < y2) {
			cs = html_object_check_page_split (HTML_OBJECT (cell), *min_y - y1) + y1;
			/* printf ("min_y: %d y1: %d y2: %d --> cs=%d\n", *min_y, y1, y2, cs); */

			if (cs < *min_y) {
				*min_y = cs;
				changed = TRUE;
			}
		}
	}

	return changed;
}

static gint
check_page_split (HTMLObject *self, gint y)
{
	HTMLTable     *table;
	gint r, min_y;

	table = HTML_TABLE (self);
	r     = to_index (bin_search_index (table->rowHeights, 0, table->totalRows, y), 0, table->totalRows - 1);
	if (y < ROW_HEIGHT (table, r) && r > 0)
		r--;

	/* printf ("y: %d rh: %d rh+1: %d\n", y, ROW_HEIGHT (table, r), ROW_HEIGHT (table, r+1)); */

	/* if (r >= table->totalRows)
	   return MIN (y, ROW_HEIGHT (table, table->totalRows)); */

	min_y = MIN (y, ROW_HEIGHT (table, r+1));
	while (check_row_split (table, r, &min_y));

	/* printf ("min_y=%d\n", min_y); */

	return min_y;
}

static GdkColor *
get_bg_color (HTMLObject *o,
	      HTMLPainter *p)
{
	
	return HTML_TABLE (o)->bgColor
		? HTML_TABLE (o)->bgColor
		: html_object_get_bg_color (o->parent, p);
}


void
html_table_type_init (void)
{
	html_table_class_init (&html_table_class, HTML_TYPE_TABLE, sizeof (HTMLTable));
}

void
html_table_class_init (HTMLTableClass *klass,
		       HTMLType type,
		       guint object_size)
{
	HTMLObjectClass *object_class;

	object_class = HTML_OBJECT_CLASS (klass);

	html_object_class_init (object_class, type, object_size);

	object_class->copy = copy;
	object_class->calc_size = calc_size;
	object_class->draw = draw;
	object_class->destroy = destroy;
	object_class->calc_min_width = calc_min_width;
	object_class->calc_preferred_width = calc_preferred_width;
	object_class->set_max_width = html_table_set_max_width;
	object_class->reset = reset;
	object_class->check_point = check_point;
	object_class->find_anchor = find_anchor;
	object_class->is_container = is_container;
	object_class->forall = forall;
	object_class->search = search;
	object_class->fit_line = fit_line;
	object_class->append_selection_string = append_selection_string;
	object_class->head = head;
	object_class->tail = tail;
	object_class->next = next;
	object_class->prev = prev;
	object_class->save = save;
	object_class->save_plain = save_plain;
	object_class->check_page_split = check_page_split;
	object_class->get_bg_color = get_bg_color;

	parent_class = &html_object_class;
}

void
html_table_init (HTMLTable *table,
		 HTMLTableClass *klass,
		 gint width, gint percent,
		 gint padding, gint spacing, gint border)
{
	HTMLObject *object;
	gint r;

	object = HTML_OBJECT (table);

	html_object_init (object, HTML_OBJECT_CLASS (klass));

	object->percent = percent;

	table->specified_width = width;
	if (width == 0)
		object->flags &= ~ HTML_OBJECT_FLAG_FIXEDWIDTH;
	else
		object->flags |= HTML_OBJECT_FLAG_FIXEDWIDTH;

	table->padding  = padding;
	table->spacing  = spacing;
	table->border   = border;
	table->caption  = NULL;
	table->capAlign = HTML_VALIGN_TOP;
	table->bgColor  = NULL;
	table->bgPixmap = NULL;

	table->row = 0;
	table->col = 0;

	table->totalCols = 1; /* this should be expanded to the maximum number
				 of cols by the first row parsed */
	table->totalRows = 1;
	table->allocRows = 5; /* allocate five rows initially */

	table->cells = g_new0 (HTMLTableCell **, table->allocRows);
	for (r = 0; r < table->allocRows; r++)
		table->cells[r] = g_new0 (HTMLTableCell *, table->totalCols);;
	
	table->columnMin   = g_array_new (FALSE, FALSE, sizeof (gint));
	table->columnFixed = g_array_new (FALSE, FALSE, sizeof (gint));
	table->columnPref  = g_array_new (FALSE, FALSE, sizeof (gint));
	table->columnOpt   = g_array_new (FALSE, FALSE, sizeof (gint));
	table->rowHeights  = g_array_new (FALSE, FALSE, sizeof (gint));
}

HTMLObject *
html_table_new (gint width, gint percent,
		gint padding, gint spacing, gint border)
{
	HTMLTable *table;

	table = g_new (HTMLTable, 1);
	html_table_init (table, &html_table_class,
			 width, percent, padding, spacing, border);

	return HTML_OBJECT (table);
}



void
html_table_add_cell (HTMLTable *table, HTMLTableCell *cell)
{
	alloc_cell (table, table->row, table->col);
	prev_col_do_cspan (table, table->row);

	/* look for first free cell */
	while (table->cells [table->row][table->col] && table->col < table->totalCols)
		table->col++;

	alloc_cell (table, table->row, table->col);
	set_cell (table, table->row, table->col, cell);
	html_table_cell_set_position (cell, table->row, table->col);
	do_cspan(table, table->row, table->col, cell);
}

void
html_table_start_row (HTMLTable *table)
{
	table->col = 0;
}

void
html_table_end_row (HTMLTable *table)
{
	if (table->row >= table->totalRows)
		inc_rows (table, 1);
	table->row++;
}

void
html_table_end_table (HTMLTable *table)
{
}
