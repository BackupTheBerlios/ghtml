/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

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
#include "htmlclueflow.h"
#include "htmlcursor.h"
#include "htmlengine.h"
#include "htmlengine-edit.h"
#include "htmlengine-edit-cut-and-paste.h"
#include "htmlengine-edit-table.h"
#include "htmltable.h"
#include "htmltablecell.h"
#include "htmltablepriv.h"
#include "htmlundo.h"

static void delete_table_column (HTMLEngine *e, HTMLUndoDirection dir);
static void insert_table_column (HTMLEngine *e, gboolean after, HTMLUndoDirection dir);
static void delete_table_row    (HTMLEngine *e, HTMLUndoDirection dir);
static void insert_table_row    (HTMLEngine *e, gboolean after, HTMLUndoDirection dir);

static HTMLTableCell *
new_cell (HTMLEngine *e, HTMLTable *table)
{
	HTMLObject    *cell;
	HTMLObject    *text;
	HTMLObject    *flow;
	
	cell  = html_table_cell_new (0, 1, 1, table->padding);
	flow  = html_clueflow_new (HTML_CLUEFLOW_STYLE_NORMAL, 0);
	text  = html_engine_new_text_empty (e);

	html_clue_append (HTML_CLUE (flow), text);
	html_clue_append (HTML_CLUE (cell), flow);

	if (table->bgColor)
		html_object_set_bg_color (cell, table->bgColor);
	if (table->bgPixmap)
		html_table_cell_set_bg_pixmap (HTML_TABLE_CELL (cell), table->bgPixmap);

	return HTML_TABLE_CELL (cell);
}

/*
 * Table insertion
 */

/**
 * html_engine_insert_table_1_1:
 * @e: An html engine
 *
 * Inserts new table with one cell containing an empty flow with an empty text. Inserted table has 1 row and 1 column.
 **/

void
html_engine_insert_table_1_1 (HTMLEngine *e)
{
	HTMLObject    *flow;
	HTMLObject    *table;

	table = html_table_new (0, 100, 1, 2, 1);

	html_table_add_cell (HTML_TABLE (table), new_cell (e, HTML_TABLE (table)));

	flow  = html_clueflow_new (HTML_CLUEFLOW_STYLE_NORMAL, 0);
	html_clue_append (HTML_CLUE (flow), table);

	html_engine_append_object (e, flow, 2, 2);
	html_cursor_backward (e->cursor, e);
}

/*
 *  Insert Column
 */

static void
insert_column_undo_action (HTMLEngine *e, HTMLUndoData *data, HTMLUndoDirection dir)
{
	delete_table_column (e, html_undo_direction_reverse (dir));
}

static void
insert_column_setup_undo (HTMLEngine *e, HTMLUndoDirection dir)
{
	html_undo_add_action (e->undo,
			      html_undo_action_new ("Insert table column", insert_column_undo_action,
						    NULL, html_cursor_get_position (e->cursor)),
			      dir);
}

static void
insert_table_column (HTMLEngine *e, gboolean after, HTMLUndoDirection dir)
{
	HTMLTable *t;
	HTMLTableCell *cell;
	gint r, c, nc, col, row, delta = 0, first_row = -1;

	t = HTML_TABLE (html_object_nth_parent (e->cursor->object, 3));
	if (!t)
		return;

	html_engine_freeze (e);

	cell = HTML_TABLE_CELL (html_object_nth_parent (e->cursor->object, 2)); 
	col  = cell->col + (after ? 1 : 0);
	row  = cell->row;
	nc   = t->totalCols + 1;
	html_table_alloc_cell (t, 0, t->totalCols);

	for (r = 0; r < t->totalRows; r ++) {
		for (c = nc - 1; c > col; c --) {
			HTMLTableCell *cell = t->cells [r][c - 1];

			if (cell && cell->col >= col) {
				if (cell->row == r && cell->col == c - 1)
					html_table_cell_set_position (cell, r, c);
				t->cells [r][c] = cell;
				t->cells [r][c - 1] = NULL;
			}
		}
		if (!t->cells [r][col]) {
			html_table_set_cell (t, r, col, new_cell (e, t));
			html_table_cell_set_position (t->cells [r][col], r, col);
			if (r <= row)
				delta ++;
			if (first_row == -1)
				first_row = r;
		}
	}
	e->cursor->position += delta - (after ? 1 : 0);
	/* now we have right position, let move to the first cell of this new column */
	if (first_row != -1) {
		HTMLTableCell *cell;
		gboolean end;
		do {
			cell = HTML_TABLE_CELL (html_object_nth_parent (e->cursor->object, 2));
			if (cell->col == col && cell->row == first_row)
				break;
			if (after && first_row >= row)
				end = html_cursor_forward (e->cursor, e);
			else
				end = html_cursor_backward (e->cursor, e);
		} while (end);
	} else
		g_warning ("no new cells added\n");

	html_object_change_set (HTML_OBJECT (t), HTML_CHANGE_ALL);
	insert_column_setup_undo (e, dir);
	html_engine_thaw (e);
}

/**
 * html_engine_insert_table_column:
 * @e: An HTML engine.
 * @after: If TRUE then inserts new column after current one, defined by current cursor position.
 *         If FALSE then inserts before current one.
 *
 * Inserts new column into table after/before current column.
 **/

void
html_engine_insert_table_column (HTMLEngine *e, gboolean after)
{
	insert_table_column (e, after, HTML_UNDO_UNDO);
}

/*
 * Delete column
 */

static void
delete_column_undo_action_after (HTMLEngine *e, HTMLUndoData *data, HTMLUndoDirection dir)
{
	insert_table_column (e, TRUE, html_undo_direction_reverse (dir));
}

static void
delete_column_undo_action_before (HTMLEngine *e, HTMLUndoData *data, HTMLUndoDirection dir)
{
	insert_table_column (e, FALSE, html_undo_direction_reverse (dir));
}

static void
delete_column_setup_undo (HTMLEngine *e, gboolean after, HTMLUndoDirection dir)
{
	html_undo_add_action (e->undo,
			      html_undo_action_new ("Delete table column", after
						    ? delete_column_undo_action_after : delete_column_undo_action_before,
						    NULL, html_cursor_get_position (e->cursor)),
			      dir);
}

static void
delete_table_column (HTMLEngine *e, HTMLUndoDirection dir)
{
	HTMLTable *t;
	HTMLTableCell *cell;
	HTMLTableCell **column;
	HTMLObject *co;
	gint r, c, col, delta = 0;

	t = HTML_TABLE (html_object_nth_parent (e->cursor->object, 3));

	/* this command is valid only in table and when this table has > 1 column */
	if (!t || !HTML_IS_TABLE (HTML_OBJECT (t)) || t->totalCols < 2)
		return;

	html_engine_freeze (e);

	cell   = HTML_TABLE_CELL (html_object_nth_parent (e->cursor->object, 2));
	col    = cell->col;
	column = g_new0 (HTMLTableCell *, t->totalRows);

	/* move cursor after/before this column (always keep it in table!) */
	do {
		if (col != t->totalCols - 1)
			html_cursor_forward (e->cursor, e);
		else
			html_cursor_backward (e->cursor, e);
		co = html_object_nth_parent (e->cursor->object, 2);
	} while (co && co->parent == HTML_OBJECT (t)
		 && HTML_OBJECT_TYPE (co) == HTML_TYPE_TABLECELL && HTML_TABLE_CELL (co)->col == col);

	for (r = 0; r < t->totalRows; r ++) {
		cell = t->cells [r][col];

		/* remove & keep old one */
		if (cell && cell->col == col) {
			HTML_OBJECT (cell)->parent = NULL;
			column [r] = cell;
			t->cells [r][col] = NULL;
			delta += html_object_get_recursive_length (HTML_OBJECT (cell));
			delta ++;
		}

		for (c = col + 1; c < t->totalCols; c ++) {
			cell = t->cells [r][c];
			if (cell && cell->col != col) {
				if (cell->row == r && cell->col == c)
					html_table_cell_set_position (cell, r, c - 1);
				t->cells [r][c - 1] = cell;
				t->cells [r][c]     = NULL;
			}
		}
	}

	if (col != t->totalCols - 1)
		e->cursor->position -= delta;
	t->totalCols --;

	html_object_change_set (HTML_OBJECT (t), HTML_CHANGE_ALL);
	delete_column_setup_undo (e, col != t->totalCols - 1, dir);
	html_engine_thaw (e);
}

/**
 * html_engine_delete_table_column:
 * @e: An HTML engine.
 *
 * Deletes current table column.
 **/

void
html_engine_delete_table_column (HTMLEngine *e)
{
	delete_table_column (e, HTML_UNDO_UNDO);
}

/*
 *  Insert Row
 */

static void
insert_row_undo_action (HTMLEngine *e, HTMLUndoData *data, HTMLUndoDirection dir)
{
	delete_table_row (e, html_undo_direction_reverse (dir));
}

static void
insert_row_setup_undo (HTMLEngine *e, HTMLUndoDirection dir)
{
	html_undo_add_action (e->undo,
			      html_undo_action_new ("Insert table row", insert_row_undo_action,
						    NULL, html_cursor_get_position (e->cursor)),
			      dir);
}

static void
insert_table_row (HTMLEngine *e, gboolean after, HTMLUndoDirection dir)
{
	HTMLTable *t;
	gint r, c, ntr, row, delta = 0, first_col;

	t = HTML_TABLE (html_object_nth_parent (e->cursor->object, 3));
	if (!t)
		return;

	html_engine_freeze (e);

	row = HTML_TABLE_CELL (html_object_nth_parent (e->cursor->object, 2))->row + (after ? 1 : 0);
	ntr  = t->totalRows + 1;
	html_table_alloc_cell (t, t->totalRows, 0);

	for (c = 0; c < t->totalCols; c ++) {
		for (r = ntr - 1; r > row; r --) {
			HTMLTableCell *cell = t->cells [r - 1][c];

			if (cell && cell->row >= row) {
				if (cell->row == r - 1 && cell->col == c)
					html_table_cell_set_position (cell, r, c);
				t->cells [r][c]     = cell;
				t->cells [r - 1][c] = NULL;
			}
		}
		if (!t->cells [row][c]) {
			html_table_set_cell (t, row, c, new_cell (e, t));
			html_table_cell_set_position (t->cells [row][c], row, c);
			delta ++;
			if (delta == 1)
				first_col = c;
		}
	}
	if (!after)
		e->cursor->position += delta;
	/* now we have right position, let move to the first cell of this new row */
	if (delta) {
		HTMLTableCell *cell;
		gboolean end;
		do {
			cell = HTML_TABLE_CELL (html_object_nth_parent (e->cursor->object, 2));
			if (cell->col == first_col && cell->row == row)
				break;
			if (after)
				end = html_cursor_forward (e->cursor, e);
			else
				end = html_cursor_backward (e->cursor, e);
		} while (end);
	} else
		g_warning ("no new cells added\n");

	html_object_change_set (HTML_OBJECT (t), HTML_CHANGE_ALL);
	insert_row_setup_undo (e, dir);
	html_engine_thaw (e);
}

/**
 * html_engine_insert_table_row:
 * @e: An HTML engine.
 * @after: If TRUE then inserts new row after current one, defined by current cursor position.
 *         If FALSE then inserts before current one.
 *
 * Inserts new row into table after/before current row.
 **/

void
html_engine_insert_table_row (HTMLEngine *e, gboolean after)
{
	printf ("html_engine_insert_table_row\n");
	insert_table_row (e, after, HTML_UNDO_UNDO);
}

/*
 * Delete row
 */

static void
delete_row_undo_action_after (HTMLEngine *e, HTMLUndoData *data, HTMLUndoDirection dir)
{
	insert_table_row (e, TRUE, html_undo_direction_reverse (dir));
}

static void
delete_row_undo_action_before (HTMLEngine *e, HTMLUndoData *data, HTMLUndoDirection dir)
{
	insert_table_row (e, FALSE, html_undo_direction_reverse (dir));
}

static void
delete_row_setup_undo (HTMLEngine *e, gboolean after, HTMLUndoDirection dir)
{
	html_undo_add_action (e->undo,
			      html_undo_action_new ("Delete table row", after
						    ? delete_row_undo_action_after : delete_row_undo_action_before,
						    NULL, html_cursor_get_position (e->cursor)),
			      dir);
}

static void
delete_table_row (HTMLEngine *e, HTMLUndoDirection dir)
{
	HTMLTable *t;
	HTMLTableCell *cell;
	HTMLTableCell **row;
	HTMLObject *co;
	gint r, c, col, delta = 0;

	t = HTML_TABLE (html_object_nth_parent (e->cursor->object, 3));

	/* this command is valid only in table and when this table has > 1 row */
	if (!t || !HTML_IS_TABLE (HTML_OBJECT (t)) || t->totalCols < 2)
		return;

	html_engine_freeze (e);

	cell   = HTML_TABLE_CELL (html_object_nth_parent (e->cursor->object, 2));
	col    = cell->col;
	row = g_new0 (HTMLTableCell *, t->totalRows);

	/* move cursor after/before this row (always keep it in table!) */
	do {
		if (col != t->totalCols - 1)
			html_cursor_forward (e->cursor, e);
		else
			html_cursor_backward (e->cursor, e);
		co = html_object_nth_parent (e->cursor->object, 2);
	} while (co && co->parent == HTML_OBJECT (t)
		 && HTML_OBJECT_TYPE (co) == HTML_TYPE_TABLECELL && HTML_TABLE_CELL (co)->col == col);

	for (r = 0; r < t->totalRows; r ++) {
		cell = t->cells [r][col];

		/* remove & keep old one */
		if (cell && cell->col == col) {
			HTML_OBJECT (cell)->parent = NULL;
			row [r] = cell;
			t->cells [r][col] = NULL;
			delta += html_object_get_recursive_length (HTML_OBJECT (cell));
			delta ++;
		}

		for (c = col + 1; c < t->totalCols; c ++) {
			cell = t->cells [r][c];
			if (cell && cell->col != col) {
				if (cell->row == r && cell->col == c)
					html_table_cell_set_position (cell, r, c - 1);
				t->cells [r][c - 1] = cell;
				t->cells [r][c]     = NULL;
			}
		}
	}

	if (col != t->totalCols - 1)
		e->cursor->position -= delta;
	t->totalCols --;

	html_object_change_set (HTML_OBJECT (t), HTML_CHANGE_ALL);
	delete_row_setup_undo (e, col != t->totalCols - 1, dir);
	html_engine_thaw (e);
}

/**
 * html_engine_delete_table_row:
 * @e: An HTML engine.
 *
 * Deletes current table row.
 **/

void
html_engine_delete_table_row (HTMLEngine *e)
{
	delete_table_row (e, HTML_UNDO_UNDO);
}

/*
 * Border width
 */

void
html_engine_table_set_border_width (HTMLEngine *e, gint border_width, gboolean relative)
{
	HTMLTable *t;

	t = HTML_TABLE (html_object_nth_parent (e->cursor->object, 3));

	/* this command is valid only in table and when this table has > 1 column */
	if (!t || !HTML_IS_TABLE (HTML_OBJECT (t)))
		return;

	html_engine_freeze (e);
	if (relative)
		t->border += border_width;
	else
		t->border = border_width;
	html_object_change_set (HTML_OBJECT (t), HTML_CHANGE_ALL);
	html_engine_thaw (e);
}
