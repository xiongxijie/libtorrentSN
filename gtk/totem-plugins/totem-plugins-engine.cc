/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Plugin engine for Totem, heavily based on the code from Rhythmbox,
 * which is based heavily on the code from totem.
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *               2006 James Livingston  <jrl@ids.org.au>
 *               2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

// #ifdef HAVE_CONFIG_H
// #include <config.h>
// #endif

#include <string.h>

// #include <glib/gi18n.h>
#include <gio/gio.h> 

#include <glib.h>
#include <girepository.h>
#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-set.h>
#include <stdio.h>
#include "totem-dirs.h"
#include "totem-plugins-engine.h"


struct _TotemPluginsEngine {
	PeasEngine parent;
	PeasExtensionSet *activatable_extensions;
	TotemWrapper *totem;
	GSettings *settings;
	guint garbage_collect_id;
};

G_DEFINE_TYPE (TotemPluginsEngine, totem_plugins_engine, PEAS_TYPE_ENGINE)

static void totem_plugins_engine_dispose (GObject *object);

static gboolean
garbage_collect_cb (gpointer data)
{
	TotemPluginsEngine *engine = (TotemPluginsEngine *) data;
	peas_engine_garbage_collect (PEAS_ENGINE (engine));
	return TRUE;
}

static void
totem_plugins_engine_class_init (TotemPluginsEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = totem_plugins_engine_dispose;
}

static void
on_activatable_extension_added (PeasExtensionSet *set,
				PeasPluginInfo   *info,
				PeasExtension    *exten,
				TotemPluginsEngine *engine)
{
	peas_activatable_activate (PEAS_ACTIVATABLE (exten));
}

static void
on_activatable_extension_removed (PeasExtensionSet *set,
				  PeasPluginInfo   *info,
				  PeasExtension    *exten,
				  TotemPluginsEngine *engine)
{
	peas_activatable_deactivate (PEAS_ACTIVATABLE (exten));
}

TotemPluginsEngine *
totem_plugins_engine_get_default (TotemWrapper *totem)
{
	
												if (FALSE)
												{
													if(totem!=NULL)
													{
														printf ("OK(TotemWrapper not empty)\n");
													}
													else
													{
														printf ("BAD(TotemWrapper is empty)\n");
													}
													return NULL;
												}
												
	/*static*/ TotemPluginsEngine *engine = NULL;
	char **paths;
	guint i;
	const GList *plugin_infos, *l;
												
												printf("IN totem_plugins_engine_get_default engine is %p\n",static_cast<gpointer>(engine));
	//if already exists, return it with ref
	// if (G_LIKELY (engine != NULL))
	// {
	// 	return g_object_ref (engine);
	// }

	g_return_val_if_fail (totem != NULL, NULL);
		
	// g_irepository_require: a function in the GObject Introspection (GI) library
	// ensure that a specific version of a library is loaded into the GObject Introspection repository, 
	// enabling developers to interact with the library and utilize its symbols at runtime.

	//Libpeas needs gir-introspection,so dont comment out below two lines
	g_irepository_require (g_irepository_get_default (), "Peas", "1.0", static_cast<GIRepositoryLoadFlags>(0), NULL);
	g_irepository_require (g_irepository_get_default (), "PeasGtk", "1.0", static_cast<GIRepositoryLoadFlags>(0), NULL);


	paths = totem_get_plugin_paths ();

	engine = TOTEM_PLUGINS_ENGINE (g_object_new (TOTEM_TYPE_PLUGINS_ENGINE, NULL));
	// printf ("engine is %p \n", engine);
	for (i = 0; paths[i] != NULL; i++) 
	{
				printf("IN totem_plugins_engine_get_default ,path[]=%s\n", paths[i]);
		/* Totem uses the libdir even for noarch data */
		/*
		A so-called "search path" actually consists of both a
		module directory (where the shared libraries or language modules
		lie) and a data directory (where the plugin data is).
		a plugin data file (such as rotation.plugin.desktop.in) contans the plugin metadata (Name,Author,Description,Copyright...)
		.desktop.in file exntension is to install i18n multi-language,which we dont need										
		*/
		peas_engine_add_search_path (PEAS_ENGINE (engine),
					     paths[i], paths[i]);
	}
    // Free the array of strings
	g_strfreev (paths);

	peas_engine_enable_loader (PEAS_ENGINE (engine), "python3");

	//No need , since engine is no more a static local variable
	// g_object_add_weak_pointer (G_OBJECT (engine),
						// reinterpret_cast<void**>(&engine));

	// engine->totem = g_object_ref (totem);
				
		

	// PeasActivatable:"object" property is belong to PeasActivatable
	engine->activatable_extensions = peas_extension_set_new (PEAS_ENGINE (engine),
								       PEAS_TYPE_ACTIVATABLE,
								       "object", totem,
								       NULL);

	g_signal_connect (engine->activatable_extensions, "extension-added",
			  G_CALLBACK (on_activatable_extension_added), engine);
	g_signal_connect (engine->activatable_extensions, "extension-removed",
			  G_CALLBACK (on_activatable_extension_removed), engine);

	//Creates a binding between a GSettings key and a property of an object , "loaded-plugins" property defined in libpeas (engine derives from PeasEngine)
	g_settings_bind (engine->settings, "active-plugins",
			 engine, "loaded-plugins",
			 static_cast<GSettingsBindFlags>(GSettingsBindFlags::G_SETTINGS_BIND_DEFAULT | GSettingsBindFlags::G_SETTINGS_BIND_NO_SENSITIVITY));

	/* Load builtin plugins */
	plugin_infos = peas_engine_get_plugin_list (PEAS_ENGINE (engine));


	// g_object_freeze_notify and g_object_thaw_notify are functions in the GObject system that 
	// help control property change notifications during bulk updates to an object's properties
	// They are useful when you want to  make multiple changes to an 
	// object without triggering individual notifications for each change.
	g_object_freeze_notify (G_OBJECT (engine));
	for (l = plugin_infos; l != NULL; l = l->next) {

		
		
		PeasPluginInfo *plugin_info = PEAS_PLUGIN_INFO (l->data);
		
		const char* plugin_name = peas_plugin_info_get_name (plugin_info);
		
		printf("IN totem_plugins_engine_get_default , load one plugin %s\n", plugin_name);

		//A builtin plugin is a plugin which cannot be enabled or disabled by the user
		if (peas_plugin_info_is_builtin (plugin_info)) {
			//Emits the [signal@PeasEngine::load-plugin] signal
			peas_engine_load_plugin (PEAS_ENGINE (engine), plugin_info);
		}
	}
	g_object_thaw_notify (G_OBJECT (engine));
printf ("(before return engine is %p) \n", static_cast<gpointer>(engine));
	return engine;
}



/* Necessary to break the reference cycle between activatable_extensions and the engine itself. Also useful to allow the plugins to be shut down
 * earlier than the rest of Totem, so that (for example) they can display modal save dialogues and the like. */
void
totem_plugins_engine_shut_down (TotemPluginsEngine *self)
{
	g_return_if_fail (TOTEM_IS_PLUGINS_ENGINE (self));
	g_return_if_fail (self->activatable_extensions != NULL);

	/* Disconnect from the signal handlers in case unreffing activatable_extensions doesn't finalise the PeasExtensionSet. */
	g_signal_handlers_disconnect_by_func (self->activatable_extensions, reinterpret_cast<gpointer>(on_activatable_extension_added), self);
	g_signal_handlers_disconnect_by_func (self->activatable_extensions, reinterpret_cast<gpointer>(on_activatable_extension_removed), self);

	/* We then explicitly deactivate all the extensions. Normally, this would be done extension-by-extension as they're unreffed when the
	 * PeasExtensionSet is finalised, but we've just removed the signal handler which would do that (extension-removed). */
	peas_extension_set_call (self->activatable_extensions, "deactivate");

	g_clear_object (&self->activatable_extensions);
}

static void
totem_plugins_engine_init (TotemPluginsEngine *engine)
{
	/*
	g_settings_new() requires a GSettings schema ID to create a GSettings object.
	The schema ID refers to a specific settings schema defined in your .gschema.xml file (here, it is org.gnome.totem, it's dot separated)
	This schema ID allows GSettings to know which configuration keys, types, and default values it should use.

	When your application calls g_settings_new() with a schema ID (for example, "org.gnome.totem"), 
	GSettings uses the system-wide gschemas.compiled to look up your schema.

	GSettings provides a system-wide configuration mechanism, 
	Any GNOME application can use a schema to store or retrieve values, provided that 
	the schema is compiled and available in the gschemas.compiled file

	--To put it clearly

	1.You write your settings schema in a file such as org.gnome.totem.gschema.xml. This XML file defines the keys, their types, default values, 
	and other metadata for your application's settings.
	2.During the build or installation phase, this XML file is compiled using the glib-compile-schemas tool. The tool collects all the schema files 
	in the specified directory and compiles them into a single binary file, typically named gschemas.compiled at /usr/share/glib-2.0/schemas/ for example.
	hen your application runs  "g_settings_new(ID)",it tells the GSettings system to load the schema with the ID
	Once found, it creates an object that provides a read/write interface to the settings defined by that schema.

	*/

	engine->settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);

	engine->garbage_collect_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, 20, garbage_collect_cb, engine, NULL);
	g_source_set_name_by_id (engine->garbage_collect_id, "[totem] garbage_collect_cb");
}




static void
totem_plugins_engine_dispose (GObject *object)
{

	printf ("totem_plugins_engine_dispose \n");
	
	TotemPluginsEngine *engine = TOTEM_PLUGINS_ENGINE (object);

	if (engine->activatable_extensions != NULL)
		totem_plugins_engine_shut_down (engine);

	g_clear_handle_id (&engine->garbage_collect_id, g_source_remove);
	peas_engine_garbage_collect (PEAS_ENGINE (engine));

	// g_clear_object (&engine->totem);
	g_clear_object (&engine->settings);

	G_OBJECT_CLASS (totem_plugins_engine_parent_class)->dispose (object);
}
