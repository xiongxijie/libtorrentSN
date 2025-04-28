/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

// #include "config.h"
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
// #include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "bacon-video-widget-properties.h"
#include "totem-plugin.h"
#ifdef __cplusplus
}
#endif
#include "TotemWrapper.h"
#include "totem-plugins-engine.h"
#include "TotemPluginFoo.h" // just for MenuPlaceHolderType

#define TOTEM_TYPE_MOVIE_PROPERTIES_PLUGIN		(totem_movie_properties_plugin_get_type ())
#define TOTEM_MOVIE_PROPERTIES_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_MOVIE_PROPERTIES_PLUGIN, TotemMoviePropertiesPlugin))

typedef struct {
	PeasExtensionBase parent;

	GtkWidget     *props;
	guint          handler_id_stream_length;
	Glib::RefPtr<Gio::SimpleAction> props_action;
} TotemMoviePropertiesPlugin;


TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_MOVIE_PROPERTIES_PLUGIN,
		      TotemMoviePropertiesPlugin,
		      totem_movie_properties_plugin)




/* used in update_properties_from_bvw() */
#define UPDATE_FROM_STRING(type, name) \
	do { \
		Glib::ValueBase value; \
		totem_wrapper_get_bvw_metadata (totem, \
						 type, value); \
		if (value.gobj() && G_VALUE_HOLDS_STRING(value.gobj())) { \
			const char* temp = static_cast<const Glib::Value<Glib::ustring>&>(value).get().c_str(); \
			g_object_set (G_OBJECT (pi->props), name, \
								temp, NULL); \
		} \
	} while (0)




#define UPDATE_FROM_INT(type, name, format, empty) \
	do { \
		Glib::ValueBase value; \
		char *temp; \
		totem_wrapper_get_bvw_metadata (totem, \
						 type, value); \
		int integer = static_cast<const Glib::Value<int>&>(value).get();  \
		if (integer!= 0) \
			temp = g_strdup_printf (format, \
					integer); \
		else \
			temp = g_strdup (empty); \
		g_object_set (G_OBJECT (pi->props), name, temp, NULL); \
		g_free (temp); \
	} while (0)




#define UPDATE_FROM_INT2(type1, type2, name, format) \
	do { \
		Glib::ValueBase value1; \
		Glib::ValueBase value2; \
		int x, y; \
		char *temp; \
		totem_wrapper_get_bvw_metadata (totem, \
						 type1, value1); \ 
		x =  static_cast<const Glib::Value<int>&>(value1).get();  \
		totem_wrapper_get_bvw_metadata (totem, \
							type2, value2); \
		y = static_cast<const Glib::Value<int>&>(value2).get();  \
		temp = g_strdup_printf (format, x, y); \
		g_object_set (G_OBJECT (pi->props), name, temp, NULL); \
		g_free (temp); \
	} while (0)




//update from bvw's tags data oc current video being played
static void
update_properties_from_bvw (TotemMoviePropertiesPlugin *pi)
{
	gboolean has_video, has_audio;

	//use Glib::ValueBase value; to hold underlying Glib::Value<T>, everytime it call totem_wrapper_get_bvw_metadata(), it is holding new GLib::Value<T>, so the previous one will be destructed automatically
	//which is type-erase on reassignment (Glib::Value holds type info while Glib::ValueBase not)
	
	
	TotemWrapper *totem;
	totem = TOTEM_WRAPPER(g_object_get_data (G_OBJECT (pi), "object"));


	//even if some tag has fetched,we still update it , such as BVW_INFO_CONTAINER will update everytime even if it is constant

	/* General */
	UPDATE_FROM_STRING (BVW_INFO_TITLE, "media-title");
	UPDATE_FROM_STRING (BVW_INFO_ARTIST, "artist");
	UPDATE_FROM_STRING (BVW_INFO_ALBUM, "album");
	UPDATE_FROM_STRING (BVW_INFO_YEAR, "year");
	UPDATE_FROM_STRING (BVW_INFO_COMMENT, "comment");
	UPDATE_FROM_STRING (BVW_INFO_CONTAINER, "container");


	{
		Glib::ValueBase value; 
		totem_wrapper_get_bvw_metadata (totem, BVW_INFO_DURATION, value);
		int duration = static_cast<const Glib::Value<int>&>(value).get();
		bacon_video_widget_properties_set_duration(BACON_VIDEO_WIDGET_PROPERTIES(pi->props),
								duration * 1000);
	}


	/* Types */
	{
		Glib::ValueBase value; 
		totem_wrapper_get_bvw_metadata (totem, BVW_INFO_HAS_VIDEO, value);
		has_video = static_cast<const Glib::Value<bool>&>(value).get();
	}


	{
		Glib::ValueBase value; 
		totem_wrapper_get_bvw_metadata (totem, BVW_INFO_HAS_AUDIO, value);
		has_audio = static_cast<const Glib::Value<bool>&>(value).get();
	}
	
	bacon_video_widget_properties_set_has_type(BACON_VIDEO_WIDGET_PROPERTIES(pi->props), has_video, has_audio);

	/* Video */
	if (has_video != FALSE)
	{
		UPDATE_FROM_INT2 (BVW_INFO_DIMENSION_X, BVW_INFO_DIMENSION_Y,
				  "dimensions", "%d × %d");
		UPDATE_FROM_STRING (BVW_INFO_VIDEO_CODEC, "video-codec");
		UPDATE_FROM_INT (BVW_INFO_VIDEO_BITRATE, "video_bitrate",
				 "%d kbps", "N/A");
		
		{
			Glib::ValueBase value; 
			totem_wrapper_get_bvw_metadata (totem, BVW_INFO_FPS, value);
			float framerate = static_cast<const Glib::Value<float>&>(value).get();
			bacon_video_widget_properties_set_framerate (BACON_VIDEO_WIDGET_PROPERTIES(pi->props), framerate);
		}
	}


	/* Audio */
	if (has_audio != FALSE)
	{
		UPDATE_FROM_INT (BVW_INFO_AUDIO_BITRATE, "audio-bitrate",
				"%d kbps", "N/A");
		UPDATE_FROM_STRING (BVW_INFO_AUDIO_CODEC, "audio-codec");
		UPDATE_FROM_INT (BVW_INFO_AUDIO_SAMPLE_RATE, "samplerate",
				"%d Hz",  "N/A");
		UPDATE_FROM_STRING (BVW_INFO_AUDIO_CHANNELS, "channels");
	}

#undef UPDATE_FROM_STRING
#undef UPDATE_FROM_INT
#undef UPDATE_FROM_INT2
}






static void
stream_length_notify_cb (TotemWrapper *totem,
			 GParamSpec *arg1,
			 TotemMoviePropertiesPlugin *plugin)
{
	gint64 stream_length;

	stream_length = totem_wrapper_get_stream_length(totem);

	bacon_video_widget_properties_set_duration
		(BACON_VIDEO_WIDGET_PROPERTIES (plugin->props),
		 stream_length);
}





static void
totem_movie_properties_plugin_fileidx_opened (TotemWrapper *totem,
											  const char *fpath, /*No Use Here*/
											  TotemMoviePropertiesPlugin *plugin)
{

				printf("In totem_movie_properties_plugin_fileidx_opened \n");


	update_properties_from_bvw(plugin);
	
	// show the `totem_movie_properties` Dialog
	gtk_widget_set_sensitive (plugin->props, TRUE);

}








static void
totem_movie_properties_plugin_file_closed (TotemWrapper *totem,
					   TotemMoviePropertiesPlugin *plugin)
{

				printf("In totem_movie_properties_plugin_fileidx_closed \n");

	/* Reset the properties and wait for the signal*/
	bacon_video_widget_properties_reset (BACON_VIDEO_WIDGET_PROPERTIES (plugin->props));

	//hide the `totem_movie_properties` Dialog
	gtk_widget_set_sensitive (plugin->props, FALSE);

}







static void
totem_movie_properties_plugin_metadata_updated (TotemWrapper *totem,
						const char *artist, 
						const char *title, 
						const char *album,
						TotemMoviePropertiesPlugin *plugin)
{

				// printf("In totem_movie_properties_plugin_metadata_updated \n");
	
	update_properties_from_bvw(plugin);

}







static void
properties_action_cb (const Glib::VariantBase& /*parameter*/, TotemMoviePropertiesPlugin *pi)
{
	// TotemWrapper *totem;

	// totem = g_object_get_data (G_OBJECT (pi), "object");

	gtk_widget_show (pi->props);
}






static void
impl_activate (PeasActivatable *plugin)
{

					printf("In totem-movie-properties.c impl_activeate \n");

	TotemMoviePropertiesPlugin *pi;
	TotemWrapper *totem;
	GtkWindow *parent;

	Glib::RefPtr<Gio::Menu> menu;
	Glib::RefPtr<Gio::MenuItem> item;


	pi = TOTEM_MOVIE_PROPERTIES_PLUGIN (plugin);
	// PeasActivatable:"object" property contains the targetted object for this #PeasActivatable instance
	totem = TOTEM_WRAPPER (g_object_get_data (G_OBJECT (plugin), "object"));

	parent = totem_wrapper_get_main_window (totem);
	pi->props = bacon_video_widget_properties_new (parent);
	gtk_widget_set_sensitive (pi->props, FALSE);

	g_signal_connect (pi->props, "delete-event",
			  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

			  
	/* Properties action */
	pi->props_action = Gio::SimpleAction::create("properties");
	pi->props_action->signal_activate().connect(sigc::bind(sigc::ptr_fun(&properties_action_cb), pi));
	if(pi->props_action)
		totem_wrapper_add_plugin_action(totem, pi->props_action);


	/* Install the menu */
	menu = totem_wrapper_get_menu_section (totem, MenuPlaceHolderType::PROPETIES_PLACEHOLDER);

	if(menu)
	{
		printf("totem movie-properties, get menu placeholder success \n");
	}else{
		printf("totem movie-properties, get menu placeholder failed \n");
		
	}


	item = Gio::MenuItem::create("Properties", "app.properties");
	menu->append_item(item);





	g_signal_connect (G_OBJECT (totem),
			  "fileidx-opened",
			  G_CALLBACK (totem_movie_properties_plugin_fileidx_opened),
			  plugin);
	

	g_signal_connect (G_OBJECT (totem),
			  "fileidx-closed",
			  G_CALLBACK (totem_movie_properties_plugin_file_closed),
			  plugin);
	

	g_signal_connect (G_OBJECT (totem),
			  "metadata-updated",
			  G_CALLBACK (totem_movie_properties_plugin_metadata_updated),
			  plugin);



	// notify callback to be executed when the property specified changed 
	pi->handler_id_stream_length = g_signal_connect (G_OBJECT (totem),
							       "notify::stream-length",
							       G_CALLBACK (stream_length_notify_cb),
							       plugin);
								   
			   
}




static void
impl_deactivate (PeasActivatable *plugin)
{
	// 				printf("In totem-movie-properties.c impl_deactivate \n");

	TotemMoviePropertiesPlugin *pi;
	TotemWrapper *totem;

	pi = TOTEM_MOVIE_PROPERTIES_PLUGIN (plugin);
	totem = TOTEM_WRAPPER (g_object_get_data (G_OBJECT (plugin), "object"));

	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_stream_length);


	g_signal_handlers_disconnect_by_func (G_OBJECT (totem),
								reinterpret_cast<gpointer>(totem_movie_properties_plugin_metadata_updated),
					      plugin);

	g_signal_handlers_disconnect_by_func (G_OBJECT (totem),
								reinterpret_cast<gpointer>(totem_movie_properties_plugin_fileidx_opened),
					      plugin);

	g_signal_handlers_disconnect_by_func (G_OBJECT (totem),
								reinterpret_cast<gpointer>(totem_movie_properties_plugin_file_closed),
					      plugin);

	pi->handler_id_stream_length = 0;


	totem_wrapper_empty_menu_section (totem, MenuPlaceHolderType::PROPETIES_PLACEHOLDER);

}
