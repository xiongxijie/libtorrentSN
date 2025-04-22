/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Matthieu Gautier 2015 <dev@mgautier.fr>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

// #include "config.h"

#include <stdio.h>
// #include <glib/gi18n-lib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>
#include <libportal-gtk3/portal-gtk3.h>

#include "totem-plugin.h"
#include "TotemWrapper.h"
#include "totem-plugins-engine.h"
#include "TotemPluginFoo.h" // just for MenuPlaceHolderType

#define TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN		(totem_open_directory_plugin_get_type ())
#define TOTEM_OPEN_DIRECTORY_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPlugin))

typedef struct 
{
	PeasExtensionBase parent;

	TotemWrapper   *totem;
	XdpPortal      *portal;
	GCancellable *cancellable;
	char     *full_path;
	Glib::RefPtr<Gio::SimpleAction> action;
} TotemOpenDirectoryPlugin;


TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPlugin, totem_open_directory_plugin)




static void
open_directory_cb (GObject *object,
		   GAsyncResult *result,
		   gpointer data)
{
			printf("open_directory_cb\n");
	XdpPortal *portal = XDP_PORTAL (object);
	g_autoptr(GError) error = NULL;
	gboolean res;

	res = xdp_portal_open_directory_finish (portal, result, &error);
	if (!res) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to show directory: %s", error->message);
	}
}




static void
totem_open_directory_plugin_open (const Glib::VariantBase& /*parameter*/, TotemOpenDirectoryPlugin* pi)
{
	g_assert (pi->full_path != NULL);

	printf ("(totem_open_directory_plugin_open) full_path=%s\n", pi->full_path);

	XdpParent *parent;

	GtkWindow* main_wnd = totem_wrapper_get_main_window (pi->totem);
	if(main_wnd == NULL)
	{
		return;
	}
	parent = xdp_parent_new_gtk (main_wnd);
	xdp_portal_open_directory (pi->portal, parent, pi->full_path,
				   XDP_OPEN_URI_FLAG_NONE, pi->cancellable,
				   open_directory_cb, NULL);
	xdp_parent_free (parent);
}




static void
totem_open_directory_fileidx_closed (TotemWrapper *totem,
				 TotemOpenDirectoryPlugin *pi)
{
						printf ("(totem_open_directory_fileidx_closed) \n");

	g_clear_pointer (&pi->full_path, g_free);

	pi->action->set_enabled(false);
}






static void
totem_open_directory_fileidx_opened (TotemWrapper              *totem,
				  const char               *full_path,
				  TotemOpenDirectoryPlugin *pi)
{

						// printf ("(totem_open_directory_fileidx_opened) full_path=%s\n", full_path);

	g_clear_pointer (&pi->full_path, g_free);

	if (full_path == NULL)
	{
		return;
	}

	pi->action->set_enabled(true);

	pi->full_path = g_strconcat ("file://", full_path, NULL);
}




static void
impl_activate (PeasActivatable *plugin)
{
	
					printf("OPEN-DIRECTORY: impl_activate\n ");

	TotemOpenDirectoryPlugin *pi = TOTEM_OPEN_DIRECTORY_PLUGIN (plugin);


	Glib::RefPtr<Gio::Menu> menu;
	Glib::RefPtr<Gio::MenuItem> item;

	// PeasActivatable:"object" property contains the targetted object for this #PeasActivatable instance
	pi->totem = TOTEM_WRAPPER (g_object_get_data (G_OBJECT (plugin), "object"));
	pi->portal = xdp_portal_new ();
	pi->cancellable = g_cancellable_new();

	g_signal_connect (pi->totem,
			  "fileidx-opened",
			  G_CALLBACK (totem_open_directory_fileidx_opened),
			  plugin);
	g_signal_connect (pi->totem,
			  "fileidx-closed",
			  G_CALLBACK (totem_open_directory_fileidx_closed),
			  plugin);


	pi->action = Gio::SimpleAction::create("open-dir");
	pi->action->signal_activate().connect(sigc::bind(sigc::ptr_fun(&totem_open_directory_plugin_open), pi));
	if(pi->action)
	{
		totem_wrapper_add_plugin_action(pi->totem, pi->action);
		pi->action->set_enabled(false);
	}


	/* add UI */
	menu = totem_wrapper_get_menu_section(pi->totem, MenuPlaceHolderType::OPENDIR_PLACEHOLDER);
	item = Gio::MenuItem::create("Open Containing Folder", "app.open-dir");
	menu->append_item (item);


}


static void
impl_deactivate (PeasActivatable *plugin)
{

					printf("OPEN-DIRECTORY: impl_deactivate\n ");

	TotemOpenDirectoryPlugin *pi = TOTEM_OPEN_DIRECTORY_PLUGIN (plugin);

	if (pi->cancellable != NULL) {
		g_cancellable_cancel (pi->cancellable);
		g_clear_object (&pi->cancellable);
	}

	g_signal_handlers_disconnect_by_func (pi->totem, reinterpret_cast<gpointer>(totem_open_directory_fileidx_opened), plugin);
	g_signal_handlers_disconnect_by_func (pi->totem, reinterpret_cast<gpointer>(totem_open_directory_fileidx_closed), plugin);

	totem_wrapper_empty_menu_section (pi->totem, MenuPlaceHolderType::OPENDIR_PLACEHOLDER);

	pi->totem = NULL;

	g_clear_pointer (&pi->full_path, g_free);
}


