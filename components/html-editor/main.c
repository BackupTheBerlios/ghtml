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

    Author: Ettore Perazzoli <ettore@helixcode.com>
*/

/* FIXME VERSION */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>

#include "editor-control-factory.h"


#ifdef USING_OAF

#include <liboaf/liboaf.h>

static void
init_corba (int *argc, char **argv)
{
	gnome_init_with_popt_table ("html-editor-factory", "0.0",
				    *argc, argv, oaf_popt_options, 0, NULL);

	oaf_init (*argc, argv);
}

#else

#include <libgnorba/gnorba.h>

static void
init_corba (int *argc, char **argv)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table ("html-editor-factory", "0.0",
					  argc, argv,
					  NULL, 0, NULL,
					  GNORBA_INIT_SERVER_FUNC,
					  &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_error (_("Could not initialize GNORBA"));

	CORBA_exception_free (&ev);
}

#endif

static void
init_bonobo (int *argc, char **argv)
{
	init_corba (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}

int
main (int argc, char **argv)
{
	init_bonobo (&argc, argv);

	editor_control_factory_init ();

	bonobo_main ();

	return 0;
}
