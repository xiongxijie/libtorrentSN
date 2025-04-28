/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Bastien Nocera 2019 <hadess@hadess.net>
 * Copyright (C) Simon Wenner 2011 <simon@wenner.ch>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

// #include "config.h"

#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
// #include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <errno.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

#include <string>



#ifdef __cplusplus
extern "C" {
#endif
#include "totem-plugin.h"
#ifdef __cplusplus
}
#endif



#include "BaconVideoWidget.h"
#include "TotemWrapper.h"
#include "totem-plugins-engine.h"
#include "TotemPluginFoo.h" // just for MenuPlaceHolderType

#define TOTEM_TYPE_ROTATION_PLUGIN		(totem_rotation_plugin_get_type ())
#define TOTEM_ROTATION_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_ROTATION_PLUGIN, TotemRotationPlugin))

#define GIO_ROTATION_FILE_ATTRIBUTE "metadata::totem::rotation"
#define STATE_COUNT 4

typedef struct {
	PeasExtensionBase parent;

	TotemWrapper *totem;
	
	Glib::RefPtr<Gio::Cancellable> cancellable;
	Glib::RefPtr<Gio::SimpleAction> rotate_left_action;
	Glib::RefPtr<Gio::SimpleAction> rotate_right_action;

} TotemRotationPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_ROTATION_PLUGIN, TotemRotationPlugin, totem_rotation_plugin)

static void
store_state_cb (Glib::RefPtr<Gio::AsyncResult>& result, TotemRotationPlugin* pi)
{
	// Directly get and cast the source file
	auto file = Glib::RefPtr<Gio::File>::cast_dynamic(
		result->get_source_object_base());


	try
	{
		if(!file->set_attributes_finish(result,  Glib::RefPtr<Gio::FileInfo>()))
		{
			printf ("Could not store file attribute, no error");
		}
	} 
	catch (const Glib::Error& ex)
	{
		printf ("Could not store file attribute: %s \n", ex.what().c_str());

		return;//just return, since info become nullptr on error
	}

			// printf ("(totem-rotation) store_state_cb success \n");

}

//save rotation state on the video file
static void
store_state (TotemRotationPlugin *pi)
{
	int rotation;
	char *rotation_s;
	const char *fpath;
	

	//rotation state which is a enum integer
	rotation = static_cast<int>(totem_wrapper_get_rotation (pi->totem));
	rotation_s = g_strdup_printf ("%u", rotation);
	Glib::RefPtr<Gio::FileInfo> info = Glib::make_refptr_for_instance(new Gio::FileInfo());
	//save it as a file attribute
	std::string attr_key = GIO_ROTATION_FILE_ATTRIBUTE;
	std::string attr_val = std::string (rotation_s);
	info->set_attribute_string(attr_key, attr_val);
	// std::string creates its own internal copy of rotation_s .
	g_free (rotation_s);

	fpath = totem_wrapper_get_current_full_path (pi->totem);


	std::string fpath_str = "file://" + std::string(fpath);


	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(fpath_str);
	
	file->set_attributes_async(info, 
							   sigc::bind(sigc::ptr_fun(&store_state_cb), pi),
							   pi->cancellable,
							   Gio::FileQueryInfoFlags::FILE_QUERY_INFO_NONE
	);




}

//resume rotation state you previously set
static void
restore_state_cb (Glib::RefPtr<Gio::AsyncResult>& result, TotemRotationPlugin* pi)
{
	// Directly get and cast the source file
	auto file = Glib::RefPtr<Gio::File>::cast_dynamic(
		result->get_source_object_base());
	
	Glib::RefPtr<Gio::FileInfo> info;
	const char* rotation_s = NULL;
	BvwRotation rotation;

	try
	{
		info = file->query_info_finish(result);
	} 
	catch (const Glib::Error& ex)
	{
		printf ("Could not query file attribute: %s \n", ex.what().c_str());
		return;//just return, since info become nullptr on error
	}

	Glib::ustring gstr = info->get_attribute_string(GIO_ROTATION_FILE_ATTRIBUTE);
	if(gstr.empty()==false)
	{
		rotation_s = gstr.c_str();
	}
	if (!rotation_s)
	{
		return;
	}

		// printf ("(totem-rotation) restore_state_cb success \n");

	rotation = static_cast<BvwRotation>(atoi (rotation_s));

	totem_wrapper_set_rotation(pi->totem, rotation);

}



static void
restore_state (TotemRotationPlugin *pi)
{			
	const char *fpath;

	//file must already created even it is downloading if we not seeder
	fpath = totem_wrapper_get_current_full_path (pi->totem);
	std::string fpath_str = "file://" + std::string(fpath) ;

	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(fpath_str);

	bool file_exists = file->query_exists();
	if(file_exists)
	{
		file->query_info_async(sigc::bind(sigc::ptr_fun(&restore_state_cb), pi), 
								pi->cancellable, 
								GIO_ROTATION_FILE_ATTRIBUTE, 
								Gio::FileQueryInfoFlags::FILE_QUERY_INFO_NONE);

	}
	//file has not created in libtorrent,so skip restore rotation state from video file attribute
	else 
	{
		// printf ("(totem-rotation/restore_state) file has not created, no op \n");
	}

}



static void
update_state (TotemRotationPlugin *pi,
	      const char          *fpath)
{
	if (fpath == NULL) 
	{
		pi->rotate_left_action->set_enabled(false);
		pi->rotate_right_action->set_enabled(false);
	} 
	else 
	{
		pi->rotate_left_action->set_enabled(true);
		pi->rotate_right_action->set_enabled(true);

				//printf ("(totem-rotation) update_state\n");

		restore_state (pi);
	}
}

static void
cb_rotate_left (const Glib::VariantBase& /*parameter*/, TotemRotationPlugin* pi)
{
        int state;

        state = (static_cast<int>(totem_wrapper_get_rotation (pi->totem)) - 1) % STATE_COUNT;
		totem_wrapper_set_rotation(pi->totem, static_cast<BvwRotation>(state));
        store_state (pi);
}

static void
cb_rotate_right (const Glib::VariantBase& /*parameter*/, TotemRotationPlugin* pi)
{
		int state;

        state = ( static_cast<int>(totem_wrapper_get_rotation(pi->totem)) + 1) % STATE_COUNT;
        totem_wrapper_set_rotation (pi->totem, static_cast<BvwRotation>(state));
        store_state (pi);
}

static void
totem_rotation_file_closed (TotemWrapper *totem,
			    TotemRotationPlugin *pi)
{
	update_state (pi, NULL);
}

static void
totem_rotation_file_opened (TotemWrapper *totem,
			    const char *fpath,
			    TotemRotationPlugin *pi)
{
	update_state (pi, fpath);
}

static void
impl_activate (PeasActivatable *plugin)
{

			printf ("totem-rotation plugin activated \n");
			
	TotemRotationPlugin *pi = TOTEM_ROTATION_PLUGIN (plugin);
	Glib::RefPtr<Gio::Menu> menu;
	Glib::RefPtr<Gio::MenuItem> item;


	// PeasActivatable:"object" property contains the targetted object for this #PeasActivatable instance
	pi->totem = TOTEM_WRAPPER (g_object_get_data (G_OBJECT (plugin), "object"));
	g_object_ref(pi->totem);
	pi->cancellable = Gio::Cancellable::create();


	g_signal_connect (pi->totem,
			  "fileidx-opened",
			  G_CALLBACK (totem_rotation_file_opened),
			  plugin);
	g_signal_connect (pi->totem,
			  "fileidx-closed",
			  G_CALLBACK (totem_rotation_file_closed),
			  plugin);

	/* add UI */
	menu = totem_wrapper_get_menu_section(pi->totem, MenuPlaceHolderType::ROTATION_PLACEHOLDER);
	
	if(menu)
	{
		printf("totem rotation, get menu placeholder success \n");
	}else{
		printf("totem rotation, get menu placeholder failed \n");
		
	}

	pi->rotate_left_action = Gio::SimpleAction::create("rotate-left");
	pi->rotate_left_action->signal_activate().connect(sigc::bind(sigc::ptr_fun(&cb_rotate_left), pi));
	if(pi->rotate_left_action)
		totem_wrapper_add_plugin_action(pi->totem, pi->rotate_left_action);


	pi->rotate_right_action = Gio::SimpleAction::create("rotate-right");
	pi->rotate_right_action->signal_activate().connect(sigc::bind(sigc::ptr_fun(&cb_rotate_right), pi));
	if(pi->rotate_right_action)
		totem_wrapper_add_plugin_action(pi->totem, pi->rotate_right_action);


	item = Gio::MenuItem::create("Rotate ↷", "app.rotate-right");
	menu->append_item(item);

	item = Gio::MenuItem::create("Rotate ↶", "app.rotate-left");
	menu->append_item(item);

}




static void
impl_deactivate (PeasActivatable *plugin)
{
	printf ("totem-rotation plugin deactivated \n");

	TotemRotationPlugin *pi = TOTEM_ROTATION_PLUGIN (plugin);

	if (pi->cancellable) 
	{
		pi->cancellable->cancel();
	}

	g_signal_handlers_disconnect_by_func (pi->totem, reinterpret_cast<gpointer>(totem_rotation_file_opened), plugin);
	g_signal_handlers_disconnect_by_func (pi->totem, reinterpret_cast<gpointer>(totem_rotation_file_closed), plugin);

	totem_wrapper_empty_menu_section (pi->totem, MenuPlaceHolderType::ROTATION_PLACEHOLDER);

	g_object_unref(pi->totem);
	pi->totem = nullptr;
}
