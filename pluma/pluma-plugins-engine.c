/*
 * pluma-plugins-engine.c
 * This file is part of pluma
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Modified by the pluma Team, 2002-2005. See the AUTHORS file for a
 * list of people on the pluma Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <girepository.h>

#include "pluma-plugins-engine.h"
#include "pluma-debug.h"
#include "pluma-app.h"
#include "pluma-prefs-manager.h"
#include "pluma-dirs.h"

G_DEFINE_TYPE (PlumaPluginsEngine, pluma_plugins_engine, PEAS_TYPE_ENGINE)

struct _PlumaPluginsEnginePrivate
{
	GSettings *plugin_settings;
};

PlumaPluginsEngine *default_engine = NULL;

static void
pluma_plugins_engine_init (PlumaPluginsEngine *engine)
{
	gchar *private_path;
	GError *error = NULL;

	pluma_debug (DEBUG_PLUGINS);

	peas_engine_enable_loader (PEAS_ENGINE (engine), "python");

	engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
	                                            PLUMA_TYPE_PLUGINS_ENGINE,
	                                            PlumaPluginsEnginePrivate);

	engine->priv->plugin_settings = g_settings_new (PLUMA_SCHEMA);

	/* This should be moved to libpeas */
	if (!g_irepository_require (g_irepository_get_default (),
	                            "Peas", "1.0", 0, &error))
	{
		g_warning ("Could not load Peas repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	if (!g_irepository_require (g_irepository_get_default (),
	                            "PeasGtk", "1.0", 0, &error))
	{
		g_warning ("Could not load PeasGtk repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	private_path = g_build_filename (LIBDIR, "girepository-1.0", NULL);

	if (!g_irepository_require_private (g_irepository_get_default (),
	                                    private_path, "Pluma", "1.0", 0, &error))
	{
		g_warning ("Could not load Pluma repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	g_free (private_path);

	peas_engine_add_search_path (PEAS_ENGINE (engine),
	                             pluma_dirs_get_user_plugins_dir (),
	                             pluma_dirs_get_user_plugins_dir ());

	peas_engine_add_search_path (PEAS_ENGINE (engine),
	                             pluma_dirs_get_pluma_plugins_dir (),
	                             pluma_dirs_get_pluma_plugins_data_dir ());

	g_settings_bind (engine->priv->plugin_settings,
	                 GPM_ACTIVE_PLUGINS,
	                 engine,
	                 "loaded-plugins",
	                 G_SETTINGS_BIND_DEFAULT);
}

static void
pluma_plugins_engine_dispose (GObject *object)
{
	PlumaPluginsEngine *engine = PLUMA_PLUGINS_ENGINE (object);

	if (engine->priv->plugin_settings != NULL)
	{
		g_object_unref (engine->priv->plugin_settings);
		engine->priv->plugin_settings = NULL;
	}

	G_OBJECT_CLASS (pluma_plugins_engine_parent_class)->dispose (object);
}

static void
pluma_plugins_engine_class_init (PlumaPluginsEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = pluma_plugins_engine_dispose;

	g_type_class_add_private (klass, sizeof (PlumaPluginsEnginePrivate));
}

PlumaPluginsEngine *
pluma_plugins_engine_get_default (void)
{
	if (default_engine != NULL)
		return default_engine;

	default_engine = PLUMA_PLUGINS_ENGINE (g_object_new (PLUMA_TYPE_PLUGINS_ENGINE, NULL));
	g_object_add_weak_pointer (G_OBJECT (default_engine),
	                           (gpointer) &default_engine);

	return default_engine;
}

