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
#ifndef _HTMLOBJECT_H_
#define _HTMLOBJECT_H_

#include <glib.h>
#include "htmlpainter.h"
#include "htmlfont.h"

typedef struct _HTMLObject HTMLObject;

#define HTML_OBJECT(x) ((HTMLObject *)(x))


typedef enum { HTMLNoFit, HTMLPartialFit, HTMLCompleteFit } HTMLFitType;

enum ObjectFlags { Separator = 0x01, NewLine = 0x02, Selected = 0x04,
		   AllSelected = 0x08, FixedWidth = 0x10, Aligned = 0x20,
		   Printed = 0x40, Hidden = 0x80};

typedef enum { CNone, CLeft, CRight, CAll } ClearType;
typedef enum { Top, Bottom, VCenter, VNone } VAlignType;
typedef enum { Left, HCenter, Right, None } HAlignType;

typedef	enum { Object, Clue, ClueV, ClueH, ClueFlow, Text, HSpace, TextMaster, TextSlave, VSpace, Rule, Bullet, TableType, TableCell, ClueAligned,
	       Image } objectType;

struct _HTMLObject {

	objectType ObjectType;

	gint x, y;
	gint ascent, descent;
	HTMLFont *font;
	gshort width, max_width, percent;
	guchar flags;
	HTMLObject *nextObj;
	gint objCount;

	/* The absolute position of this object on the page */
	gint absX;
	gint absY;

	void (*draw) (HTMLObject *o, HTMLPainter *p, gint x, gint y, gint width, gint height,
		      gint tx, gint ty);

	HTMLFitType (*fit_line) (HTMLObject *o, gboolean startOfLine, 
				 gboolean firstRun, gint widthLeft);

	void (*calc_size) (HTMLObject *o, HTMLObject *parent);

	void (*set_max_ascent) (HTMLObject *o, gint a);
	
	void (*set_max_descent) (HTMLObject *o, gint d);
	
	void (*destroy) (HTMLObject *o);
	
	void (*set_max_width) (HTMLObject *o, gint max_width);
	
	void (*reset) (HTMLObject *o);
	
	gint (*calc_min_width) (HTMLObject *o);
	
	gint (*calc_preferred_width) (HTMLObject *o);

	void (*calc_absolute_pos) (HTMLObject *o, gint x, gint y);
};

void        html_object_init    (HTMLObject *o, objectType ObjectType);
HTMLObject *html_object_new     (void);
void        html_object_destroy (HTMLObject *o);
#endif /* _HTMLOBJECT_H_ */



