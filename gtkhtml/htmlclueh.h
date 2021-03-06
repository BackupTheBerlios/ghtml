/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the KDE libraries
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)

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
#ifndef _HTMLCLUEH_H_
#define _HTMLCLUEH_H_

#include "htmlclue.h"

#define HTML_CLUEH(x) ((HTMLClueH *)(x))
#define HTML_CLUEH_CLASS(X) ((HTMLClueHClass *)(x))

struct _HTMLClueH {
	HTMLClue clue;

	gshort indent;
};

struct _HTMLClueHClass {
	HTMLClueClass clue_class;
};


extern HTMLClueHClass html_clue_h_class;


void        html_clueh_type_init   (void);
void        html_clueh_class_init  (HTMLClueHClass *klass,
				    HTMLType        type,
				    guint           object_size);
void        html_clueh_init        (HTMLClueH      *clue,
				    HTMLClueHClass *klass,
				    gint            x,
				    gint            y,
				    gint            max_width);
HTMLObject *html_clueh_new         (gint            x,
				    gint            y,
				    gint            max_width);

#endif /* _HTMLCLUEH_H_ */
