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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/
#ifndef _HTMLCLUEFLOW_H_
#define _HTMLCLUEFLOW_H_

#include "htmlobject.h"

#define HTML_CLUEFLOW(x) ((HTMLClueFlow *)(x))
#define HTML_CLUEFLOW_CLASS(x) ((HTMLClueFlow *)(x))

typedef struct _HTMLClueFlow HTMLClueFlow;
typedef struct _HTMLClueFlowClass HTMLClueFlowClass;

struct _HTMLClueFlow {
	HTMLClue clue;

	gshort indent;
};

struct _HTMLClueFlowClass {
	HTMLClueClass clue_class;
};


extern HTMLClueFlowClass html_clueflow_class;


void html_clueflow_type_init (void);
void html_clueflow_class_init (HTMLClueFlowClass *klass, HTMLType type);
void html_clueflow_init (HTMLClueFlow *flow, HTMLClueFlowClass *klass,
			 gint x, gint y, gint max_width, gint percent);
HTMLObject *html_clueflow_new (gint x, gint y, gint max_width, gint percent);

#endif /* _HTMLCLUEFLOW_H_ */
