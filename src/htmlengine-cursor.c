/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.
    Copyright (C) 1999 Helix Code, Inc.

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

#include <glib.h>

#include "htmlcursor.h"

#include "htmlengine-cursor.h"



void
html_engine_cursor_move (HTMLEngine *e,
			 HTMLEngineCursorMovement movement,
			 guint count)
{
	guint i;

	g_return_if_fail (e != NULL);

	if (count == 0)
		return;

	switch (movement) {
	case HTML_ENGINE_CURSOR_RIGHT:
		for (i = 0; i < count; i++)
			html_cursor_forward (e->cursor, e);
		break;
	default:
		g_warning ("Unsupported movement %d\n", (gint) movement);
	}
}
