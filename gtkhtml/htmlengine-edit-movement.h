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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef _HTMLENGINE_EDIT_MOVEMENT_H
#define _HTMLENGINE_EDIT_MOVEMENT_H

#include "htmltypes.h"

guint     html_engine_move_cursor                   (HTMLEngine               *e,
						     HTMLEngineCursorMovement  movement,
						     guint                     count);
void      html_engine_jump_to_object                (HTMLEngine               *e,
						     HTMLObject               *object,
						     guint                     offset);
void      html_engine_jump_at                       (HTMLEngine               *e,
						     gint                      x,
						     gint                      y);
void      html_engine_beginning_of_document         (HTMLEngine               *engine);
gboolean  html_engine_beginning_of_line             (HTMLEngine               *engine);
gboolean  html_engine_beginning_of_paragraph        (HTMLEngine               *engine);
void      html_engine_end_of_document               (HTMLEngine               *engine);
gboolean  html_engine_end_of_line                   (HTMLEngine               *engine);
gboolean  html_engine_end_of_paragraph              (HTMLEngine               *engine);
gint      html_engine_scroll_down                   (HTMLEngine               *engine,
						     gint                      amount);
gint      html_engine_scroll_up                     (HTMLEngine               *engine,
						     gint                      amount);
gboolean  html_engine_forward_word                  (HTMLEngine               *engine);
gboolean  html_engine_backward_word                 (HTMLEngine               *engine);
void      html_engine_edit_cursor_position_save     (HTMLEngine               *e);
void      html_engine_edit_cursor_position_restore  (HTMLEngine               *e);
void      html_engine_update_selection_if_necessary (HTMLEngine               *e);

#endif /* _HTMLENGINE_EDIT_MOVEMENT_H */
