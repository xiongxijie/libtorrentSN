/*
    The most backend of our Totem torrent streaming player sub-Window, attached to transmission-gtk torrent client
*/
#pragma once

//since it is used as a template in totem.ui, we need bind .ui template

#include "tr-macros.h"
#include "GtkCompat.h"

#include <glibmm.h>
#include <giomm.h>
#include <glibmm/refptr.h>
#include <glibmm/property.h>
#include <glibmm/propertyproxy.h>

#include <glibmm/extraclassinit.h>
#include <gtkmm.h>


#include <glib.h>//might still used glib type to better work with GStreamer
#include <glib-object.h>



#define BVW_MAX_RATE 3.0
#define BVW_MIN_RATE 0.5



#define BVW_ERROR bacon_video_widget_error_quark ()

GQuark bacon_video_widget_error_quark		 (void) G_GNUC_CONST;


#include "BaconVideoWidgetEnums.h"







//refactored as a c++ gtkmm class , rather than original gobj c code, so properties, signals need conform to glibmm-style

class BaconVideoWidgetExtraInit : public Glib::ExtraClassInit
{
public:
    BaconVideoWidgetExtraInit();

private:
    static void class_init(void* klass, void* user_data);
    static void instance_init(GTypeInstance* instance, void* klass);
};









class BaconVideoWidget : 
public BaconVideoWidgetExtraInit
, public Gtk::Bin
{

public:
	BaconVideoWidget();
	BaconVideoWidget(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder);
    ~BaconVideoWidget() override;
    
    TR_DISABLE_COPY_MOVE(BaconVideoWidget)
    

	

	//Glib::PropertyProxy Provides indirect access to an existing Glib::Property.
	//Exposed properties 
    Glib::PropertyProxy<bool> property_seekable();
    Glib::PropertyProxy<double> property_volume();

    Glib::PropertyProxy<std::string> property_audio_output_type();

    Glib::PropertyProxy<int> property_color_balance_contrast();
    Glib::PropertyProxy<int> property_color_balance_saturation();
    Glib::PropertyProxy<int> property_color_balance_brightness();
    Glib::PropertyProxy<int> property_color_balance_hue();


	//Exposed signals
    sigc::signal<void(const char*, bool)>& signal_error();
    sigc::signal<void()>& signal_eos();
    sigc::signal<void(std::int64_t, std::int64_t, double, bool)>& signal_tick();
	sigc::signal<void()>& signal_play_starting();
    sigc::signal<void()>& signal_got_metadata();
    sigc::signal<void()>& signal_btdemux_destructing();

	
	GObject* fetch_btdemux();

	bool check_gstreamer_init(Glib::Error& error);
	bool check_pipeline_init(Glib::Error& error);


	BvwRotation get_rotation();
	void set_rotation(BvwRotation rotation);
	
	bool set_rate(float rate);
	
	void set_aspect_ratio(BvwAspectRatio ratio);

	gint64 update_and_get_stream_length();

	gint64 get_current_time();

	void get_metadata(BvwMetadataType type, Glib::ValueBase& value);

	bool is_playing();

	void open(int fileidx);
	bool play(GError ** error);
	void pause();
	void stop();
	void close();

	bool update_and_get_seekable();
	bool seek_time(std::int64_t _time, bool accurate, GError **error);
	
	bool can_set_volume();
	void set_volume(double volume);

	bool can_direct_seek();
	
	double get_volume();

	bool seek(double position, GError **error);

	void set_show_cursor(bool show_cursor);


private:
    class Impl;
    std::unique_ptr<Impl> const impl_;

};






