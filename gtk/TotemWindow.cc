/*
 * SPDX-License-Identifier: GPL-3-or-later
 */
#include "Utils.h"
#include "TotemWindow.h"
#include "MyHdyFlap.h" 
#include "TotemPlayerHeader.h"
#include "TotemPrefsDialog.h"
#include "TotemPlaylist.h"
#include "BaconVideoWidget.h"
#include "BitfieldScale.h"
#include "BaconTimeLabel.h"
#include <libhandy-1/handy.h>
#include "TotemUri.h" // for totem_dot_config_dir()
#include "totem-plugins-engine.h"// for func totem_plugins_engine_get_default()
#include "TotemGstHelper.h" // for totem_gst_disable_hardware_decoders() and totem_gst_ensure_newer_hardware_decoders()
#include "TotemWrapper.h" // for TotemWrapper gobject struct


//libtorrent typs
#include "libtorrent/storage_defs.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/session_handle.hpp"



//glib c types
#include <glib.h>
#include <glib-object.h> 
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstring> // For strlen()
#include <array>
#include <string>
#include <string_view>

namespace {

//Utility function to display a error dialog
void totem_error_dialog(Gtk::Window& parent, const char *title, const char *reason)
{
	auto w = std::make_shared<Gtk::MessageDialog>(
		parent,
		title,
		false,
		TR_GTK_MESSAGE_TYPE(ERROR),
		TR_GTK_BUTTONS_TYPE(CLOSE),
		true);
	w->set_transient_for(parent);
	w->set_secondary_text(reason);
	w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
	w->show();
}

void totem_error_dialog_blocking(Gtk::Window& parent, const char *title, const char*reason)
{
    Gtk::MessageDialog dialog(parent, title, false, 
		TR_GTK_MESSAGE_TYPE(ERROR), 
		TR_GTK_BUTTONS_TYPE(OK),
		true); // modal=true

	dialog.set_secondary_text(reason);
	dialog.set_transient_for(parent);

	// Blocking run
	dialog.run();

	// Dialog automatically destroys when out of scope
}


bool event_is_touch (GdkEventButton *event)
{
	GdkDevice *device;

	device = gdk_event_get_device ((GdkEvent *) event);
	return (gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);
}

using namespace std::literals;

std::array<std::string_view, 4> const normal_action_entries = {
    "play"sv,
    "preferences"sv,
    "previous-chapter"sv,
    "next-chapter"sv
};

} //anonymous namespace

class MyHdyApplicationWindow::Impl
{

public:
    Impl(MyHdyApplicationWindow& window,
		Glib::RefPtr<Gtk::Builder> const& builder,
		Glib::RefPtr<Session> const& core,
		std::uint32_t uniq_id);
    ~Impl();	

    TR_DISABLE_COPY_MOVE(Impl)


	void init_plugins_system();


	/*Maybe exposed methods, but no one use them outside class*/
	void play();
	void play_pause();
	void pause();
	bool can_seek_previous();
	bool can_seek_next();
	void seek_previous();
	void seek_next();


	Glib::RefPtr<Gio::Menu> get_plugin_menu_placeholder(MenuPlaceHolderType type);
	void empty_plugin_menu_placeholder(MenuPlaceHolderType type);
	void add_plugin_action_to_group(const Glib::RefPtr<Gio::Action>& action);
	const char* get_current_fullpath();
	BvwRotation get_bvw_rotation();
	void set_bvw_rotation(BvwRotation rotation);
	bool set_bvw_rate(float rate);

	void get_bvw_metadata(BvwMetadataType type, Glib::ValueBase& value);
	gint64 get_bvw_stream_length();




private:
	/*Internal used methods*/
	void actions_setup();
	void signals_connect();
	void check_video_widget_created();
	void fill_totem_playlist();
	void on_btdemux_destructing();

	bool piece_bitfield_refresh();

	bool time_within_seconds();
	void playlist_direction(TotemPlaylistDirection dir);
	void set_current_fileidx_and_play();
	void set_fileidx_and_play(int file_idx);
	void set_fileidx(int file_index);
	void play_pause_set_label(TotemStates state);
	
	void emit_file_opened(const char *fpath);
	void emit_file_closed();
	void emit_metadata_updated(const char *artist,
								const char *title,
								const char *album);


	/*Actions */
	void action_set_sensitive(const char* action_name, bool is_enabled);
	void normal_actions_handler(Glib::ustring const& action_name);
    void on_toggle_fullscreen(Gio::SimpleAction& action);
    void on_toggle_playlist_repeat(Gio::SimpleAction& action);
	void on_open_prefs_dlg();
	void play_action_cb();
	void previous_chapter_action_cb();
	void next_chapter_action_cb();
	void on_aspect_tatio_toggle(int cur);


	/*callbacks for signals*/
	void on_eos_event();
	void on_error_event(const char *message, bool playback_stopped);
	void update_slider_visibility(std::int64_t stream_length);
	void update_current_time(std::int64_t          current_time,
								std::int64_t        stream_length,
								double            	current_position,
								bool          		seekable);
	void on_play_starting();
	void on_got_metadata();
	void update_volume_sliders();
	void property_notify_cb_volume();
	void volume_button_value_changed_cb(double value);
	bool volume_button_scroll_event_cb(GdkEventScroll *event);
	void set_volume_relative(double off_pct);
	void volume_button_menu_shown_cb();

	void seek (double pos);
	void seek_time_rel(std::int64_t _time, bool relative, bool accurate);
	void seek_time(std::int64_t msec, bool accurate);
	bool is_bvw_seekable();
	void reset_seek_status();
	void update_seekable();
	void property_notify_cb_seekable();
	bool seek_slider_pressed_cb(GdkEventButton *event);
	bool seek_slider_released_cb(GdkEventButton *event);
	void seek_slider_changed_cb();

	bool on_bvw_motion_notify(GdkEventMotion* motion_event);
	bool window_state_event_cb(GdkEventWindowState *event);
	bool on_video_button_press_event(GdkEventButton *event);
	void window_save_size();
	bool window_is_fullscreen();
	void save_window_state();
	void setup_window_size();


	void set_controls_visibility(bool visible, bool animate);
	void unschedule_hiding_popup();
	void schedule_hiding_popup();
	bool hide_popup_timeout_cb();
	void mark_popup_busy(const char *reason);
	void unmark_popup_busy(const char *reason);
	void popup_menu_shown_cb(Gtk::MenuButton* button);

	void properties_metadata_updated();
	void update_player_header_title(const char *name);
	void update_buttons();

private:
    MyHdyApplicationWindow& parent_;

    //the transmission Session instance (will increase ref count)
    Glib::RefPtr<Session> const core_;

    //the unique id of Torrent in Session that Totem streaming for
    std::uint32_t uniq_id_;

    Glib::RefPtr<Gio::SimpleActionGroup> action_group_;

    //flap_ is HdyFlap specified in totem.ui
    MyHdyFlap *flap_ = nullptr;//Hdyflap


    
    /****************************Backend*************************************/
    BaconVideoWidget *bvw_ = nullptr; 
    /************************************************************************/
        

	BitfieldScale *bitfield_scale_ = nullptr;

	Gtk::Box *toolbox_ = nullptr;

	Gtk::MenuButton *go_button_ = nullptr;

	Gtk::VolumeButton *volume_ = nullptr;

    BaconTimeLabel  *time_label_ = nullptr;//BaconTimeLabel

    BaconTimeLabel  *time_rem_label_ = nullptr;//BaconTimeLabel

	//player_header_ is a binding template who has its own template .ui , used in totem.ui
	TotemPlayerHeader *player_header_ = nullptr;//loaded from TotemPlayerHeader which has its own .ui template


    Glib::RefPtr<Gtk::Adjustment> scale_adj_;

	Gtk::Button *play_button_ = nullptr; 

	//prefs_ is a independent window, who has its own .ui, (not a template inserted to totem.ui)
	TotemPrefsDialog *prefs_ = nullptr; //Actually HdyPreferencesWindow, use Gtk::Window derived class to receive it

	Glib::RefPtr<Gio::Settings> settings_;

	GHashTable *busy_popup_ht_ = nullptr; /* key=reason string, value=bool */

	/*Plugins menu placeholder*/
	Glib::RefPtr<Gio::Menu> rotation_placeholder_;
	Glib::RefPtr<Gio::Menu> opendir_placeholder_;
	Glib::RefPtr<Gio::Menu> prop_placeholder_;
	Glib::RefPtr<Gio::Menu> variable_rate_placeholder_;

	TotemWrapper *wrapper_ = nullptr;
    TotemPluginsEngine* engine_ = nullptr;

	TotemPlaylist playlist_;

	TotemStates state_ = STATE_STOPPED;
	ControlsVisibility controls_visibility_ = TOTEM_CONTROLS_UNDEFINED;
	bool reveal_controls_ = false;
	bool maximised_ = false;
    bool seek_lock_ = false;/*a bool can also func as a lock*/
#if 2
	bool is_stream_length_set_ = false;
#endif
	//the file_index of currently playing stream within torrent
	std::int64_t stream_length_;
	int streaming_file_idx_ = -1;
	int window_w_ = DEFAULT_WINDOW_W;
	int window_h_ = DEFAULT_WINDOW_H;
	
    sigc::connection transition_timer_;
	sigc::connection volume_changed_tag_;
	sigc::connection piece_bitfield_timer_;










	//NOT USED, Property USED in TotemWrapper
	//************Properties***************
	// Glib::Property<bool> prop_playing_;
	// Glib::Property<std::int64_t> prop_stream_length_;
	//****************Signals*****************
	// sigc::signal<void(const char*)> signal_fileidx_opened_; 
	// sigc::signal<void()> signal_fileidx_closed_;
	// sigc::signal<void(const char *,const char *,const char *)> signal_metadata_updated_;
	
};





MyHdyApplicationWindow::MyHdyApplicationWindow(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    std::uint32_t uniq_id)
    : Gtk::Window(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core, uniq_id))
{
    // Manually cast cast_item to HdyApplicationWindow
   // // HdyApplicationWindow* hdy_window = HDY_APPLICATION_WINDOW(gobj());  // cast to HdyApplicationWindow

    set_transient_for(parent);
	
}


std::unique_ptr<MyHdyApplicationWindow> MyHdyApplicationWindow::create(Gtk::Window& parent, 
                                                                        Glib::RefPtr<Session> const& core, 
                                                                        std::uint32_t uniq_id)
{
    //init libhandy 
    HdyStyleManager *style_manager;
    hdy_init ();
    style_manager = hdy_style_manager_get_default ();
    hdy_style_manager_set_color_scheme (style_manager, HDY_COLOR_SCHEME_DEFAULT);

    //retrieve HdyApplicationWindow in totem.ui
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("totem.ui"));

    return std::unique_ptr<MyHdyApplicationWindow>(gtr_get_widget_derived<MyHdyApplicationWindow>(builder, "totem_main_window", parent, core, uniq_id));
}



MyHdyApplicationWindow::Impl::Impl(
    MyHdyApplicationWindow& window,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Session> const& core,
    std::uint32_t uniq_id)
    : parent_(window),
    core_(core),	
    uniq_id_(uniq_id),
	action_group_(Gio::SimpleActionGroup::create()),
	flap_(gtr_get_widget_derived<MyHdyFlap>(builder, "flap")),
    bvw_(gtr_get_widget_derived<BaconVideoWidget>(builder, "bvw")),
	bitfield_scale_(gtr_get_widget_derived<BitfieldScale>(builder, "bitfield_progress_bar")),
	toolbox_(gtr_get_widget<Gtk::Box>(builder, "toolbar")),
	go_button_(gtr_get_widget<Gtk::MenuButton>(builder, "go_button")),
	volume_(gtr_get_widget<Gtk::VolumeButton>(builder, "volume_button")),
	time_label_(gtr_get_widget_derived<BaconTimeLabel>(builder, "time_label", false)), 
	time_rem_label_(gtr_get_widget_derived<BaconTimeLabel>(builder, "time_rem_label", true)),
	player_header_(gtr_get_widget_derived<TotemPlayerHeader>(builder, "player_header")),
	scale_adj_(bitfield_scale_->get_adjustment()),
	play_button_(gtr_get_widget<Gtk::Button>(builder, "play_button")),
	prefs_(TotemPrefsDialog::create()),
	settings_(Gio::Settings::create(TOTEM_GSETTINGS_SCHEMA)),
	busy_popup_ht_(g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL))
{

    prefs_->set_transient_for(parent_);
    //we are preferences window's parent ,we die, it also die
    gtr_window_on_close(
        *prefs_,
        [this]()
        {    
                                std::cout << "Hide TotemPrefsDialog on handler" << std::endl;
            prefs_->hide();

            return true;//avoid destruction of prefs window
        }
    );


	
	actions_setup();
	action_set_sensitive("play", false);
	action_set_sensitive("next-chapter", false);
	action_set_sensitive("previous-chapter", false);

	signals_connect();
	setup_window_size();
	check_video_widget_created();

	bool can_set_vol = bvw_->can_set_volume();
	volume_->set_sensitive(can_set_vol);
	bvw_->grab_focus();

	fill_totem_playlist();
	play_pause_set_label(STATE_PLAYING);


	// Glib::RefPtr<Glib::Object> rot_obj = builder->get_object("rotation-placeholder");
	// if(rot_obj)
	// {
	// 	if(!rotation_placeholder_)
	// 	{
	// 		std::cout << "before rotation_placeholder_ is empty " << std::endl;
	// 	}
	// 	rotation_placeholder_ = Glib::RefPtr<Gio::Menu>::cast_dynamic(rot_obj);
	// }


	rotation_placeholder_ = Glib::RefPtr<Gio::Menu>::cast_static(builder->get_object("rotation-placeholder"));
	opendir_placeholder_ = Glib::RefPtr<Gio::Menu>::cast_static(builder->get_object("opendirectory-placeholder"));
	prop_placeholder_ = Glib::RefPtr<Gio::Menu>::cast_static(builder->get_object("properties-placeholder"));
	variable_rate_placeholder_ = Glib::RefPtr<Gio::Menu>::cast_static(builder->get_object("variable-rate-placeholder"));

	/*Plugins shoud initalized later by caller,do it after we fully Constructed, 
		so Don't put them in the constructor to avoid SIGSEGV */


	//set pipeline_ state from NULL to READY, and to PAUSED 
	bvw_->initialize_state();


	//btdemux lifetime
    GObject* btdemux_gobj =  bvw_->fetch_btdemux();
    if(btdemux_gobj != nullptr)
    {
        core_->retrieve_btdemux_gobj(btdemux_gobj);
    }
    else
    {
        std::cerr << "Error:In TotemWindow.cc : fetch_btdemux is empty " << std::endl;
		// return;
    }


	bitfield_scale_->set_num_pieces(core_->totem_get_total_num_pieces());
    //periodically update bitfield
	piece_bitfield_timer_ = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::piece_bitfield_refresh), 2);
	piece_bitfield_refresh();



	/* Delegate to do the real thing, play */
	Glib::signal_idle().connect([this](){
		std::cout << "MyHdyApplicationWindow [Start Play] " << std::endl;
		set_current_fileidx_and_play();
		return false;
	});

	std::cout << "MyHdyApplicationWindow::Impl constructor finished " << std::endl;

}




MyHdyApplicationWindow::~MyHdyApplicationWindow() /*= default;*/
{
                        std::cout << "MyHdyApplicationWindow::~MyHdyApplicationWindow()" << std::endl;
}



void MyHdyApplicationWindow::init_plugins_system()
{
	impl_->init_plugins_system();
}
void MyHdyApplicationWindow::Impl::init_plugins_system()
{
	if(wrapper_ == nullptr)
	{
		wrapper_ = totem_wrapper_from_gtkmm_instance(&parent_);
	}

	if(wrapper_!=nullptr)
	{
		std::cout << "[init_plugins_system] TotemWrapper initialized success " << std::endl;
		engine_ = totem_plugins_engine_get_default(wrapper_);
	}
	else
	{
		std::cout << "[init_plugins_system] TotemWrapper initialized failed " << std::endl;
	}

	//Gracefully feed engine to preferences dilaog-> plugins page
	if(engine_ != nullptr)
	{
		prefs_->render_plugins_page(engine_);
	}
	prefs_->load_other_pages(*bvw_);
}



void MyHdyApplicationWindow::on_closing_sync_display() 
{
	std::cout << "void MyHdyApplicationWindow::on_closing_sync_display()" << std::endl;
	Glib::RefPtr<Gdk::Display> display = get_display();
	display->sync();
}



MyHdyApplicationWindow::Impl::~Impl()
{
                        std::cout << "MyHdyApplicationWindow::Impl::~Impl() " << std::endl;
    if(prefs_)
    {
		//Cannot skipped manully deleting it, or it will dangling
        delete prefs_;
    }
 
	if(bvw_)
	{
		window_save_size();
	}

	if (wrapper_)
    {
        totem_wrapper_cleanup(wrapper_);
    }

	if (engine_)
    {
        std::cout << "MyHdyApplicationWindow::Impl::~Impl() call totem_plugins_engine_shut_down" << std::endl;
		totem_plugins_engine_shut_down (engine_);
    }
	g_clear_object (&engine_);

	g_clear_pointer (&busy_popup_ht_, g_hash_table_destroy);

	//clear connections
	if(volume_changed_tag_.connected())
    {
		volume_changed_tag_.disconnect();
    }
	if(transition_timer_.connected())
    {
		transition_timer_.disconnect();
    }
	if(piece_bitfield_timer_.connected())
	{
		piece_bitfield_timer_.disconnect();
	}


	//save size/dimension data to state.ini file	
	save_window_state();
	
}




void MyHdyApplicationWindow::Impl::action_set_sensitive(const char* action_name, bool is_enabled)
{
	auto simple_action = Glib::RefPtr<Gio::SimpleAction>::cast_dynamic(action_group_->lookup_action(action_name));
	if(simple_action){
		simple_action->set_enabled(is_enabled);
	}
}


void MyHdyApplicationWindow::Impl::signals_connect()
{
	/*connect to mainwindow's signal*/
	parent_.signal_window_state_event().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::window_state_event_cb));


	if(bvw_)
	{
		bvw_->signal_motion_notify_event().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_bvw_motion_notify));
		bvw_->signal_button_press_event().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_video_button_press_event));
	}

	if(bitfield_scale_)
	{
		bitfield_scale_->add_events(
			Gdk::EventMask::BUTTON_PRESS_MASK |
			Gdk::EventMask::BUTTON_RELEASE_MASK);

		bitfield_scale_->signal_button_press_event().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::seek_slider_pressed_cb));
		bitfield_scale_->signal_button_release_event().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::seek_slider_released_cb));

		if(scale_adj_)
		{
			scale_adj_->signal_value_changed().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::seek_slider_changed_cb));
		}
	}


	if(bvw_ != nullptr)
	{
		/*connect to BaconVideoWidget's signal*/
		// bvw_->signal_btdemux_destructing().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_btdemux_destructing))
		bvw_->signal_eos().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_eos_event));
		bvw_->signal_error().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_error_event));
		bvw_->signal_tick().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::update_current_time));
		bvw_->signal_play_starting().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_play_starting));
		bvw_->signal_got_metadata().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::on_got_metadata));
		bvw_->property_volume().signal_changed().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::property_notify_cb_volume));
		bvw_->property_seekable().signal_changed().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::property_notify_cb_seekable));
	}


	/*go Button*/
	go_button_->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::popup_menu_shown_cb), go_button_));
	Gtk::Popover* go_btn_popover = go_button_->get_popover();
	if(go_btn_popover)
		go_btn_popover->set_size_request(175, -1);
	

	/*Volume*/
	volume_->add_events(
		Gdk::EventMask::SCROLL_MASK
	);
	volume_changed_tag_ = volume_->signal_value_changed().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::volume_button_value_changed_cb));
	volume_->signal_scroll_event().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::volume_button_scroll_event_cb));
	Gtk::Widget* volume_popup = volume_->get_popup();
	volume_popup->property_visible().signal_changed().connect(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::volume_button_menu_shown_cb));

	/*Playerheader Menubutton*/
	Gtk::MenuButton* menuBtn = player_header_->get_player_button();
	menuBtn->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &MyHdyApplicationWindow::Impl::popup_menu_shown_cb), menuBtn));

}



void MyHdyApplicationWindow::Impl::normal_actions_handler(Glib::ustring const& action_name)
{
    if (action_name == "play")//Play / Pause
	{
		play_action_cb();
	}
	else if (action_name == "preferences")//open preferences dialog
	{
		on_open_prefs_dlg();
	}
	else if (action_name == "previous-chapter")
	{
		previous_chapter_action_cb();
	}
	else if (action_name == "next-chapter")
	{
		next_chapter_action_cb();
	}
}


void MyHdyApplicationWindow::Impl::actions_setup()
{
    for (auto const& action_name_view : normal_action_entries)
    {
        auto const action_name = Glib::ustring(std::string(action_name_view));
        auto const action = Gio::SimpleAction::create(action_name);
        action->signal_activate().connect([this, a = action.get()](auto const& /*value*/)
                                          { normal_actions_handler((*a).get_name()); });
        action_group_->add_action(action);
    }

	auto const fscreen_action = Gio::SimpleAction::create_bool("fullscreen", false);//toggle fullscreen
	fscreen_action->signal_activate().connect([this, &action = *fscreen_action](auto const& /*value*/) { on_toggle_fullscreen(action); });
	action_group_->add_action(fscreen_action);


	if(playlist_.get_repeat())
	{
		std::cout << "playlist said repeat is ON" << std::endl;
	}else{
		std::cout << "playlist said repeat is OFF" << std::endl;
	}
	auto const repeat_action = Gio::SimpleAction::create_bool("repeat", playlist_.get_repeat()/*init loading*/);
	repeat_action->signal_activate().connect([this, &action = *repeat_action](auto const& /*value*/) { on_toggle_playlist_repeat(action); });
	action_group_->add_action(repeat_action);


	auto const aspect_ratio_action = Gio::SimpleAction::create_radio_integer("aspect-ratio", 0 /*initial_state*/); //aspect-ratio
	aspect_ratio_action->signal_activate().connect([this](const Glib::VariantBase& value) 
	{ 
		int radio_val = Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(value).get();
		on_aspect_tatio_toggle(radio_val); 

		//Also update UI
		auto action_temp = Glib::RefPtr<Gio::SimpleAction>::cast_dynamic(action_group_->lookup_action("aspect-ratio"));
		action_temp->set_state(value);
	});
	action_group_->add_action(aspect_ratio_action);

	//associate with app prefix, such as app.fullscreen in totem-player-toolbar.ui
	parent_.insert_action_group("app", action_group_);
}


void MyHdyApplicationWindow::Impl::on_toggle_fullscreen(Gio::SimpleAction& action)
{
    // Retrieve the current state of the action (true = fullscreen, false = normal)
    bool current_state;
    action.get_state(current_state);

    // Toggle the state and set it back to the action
    bool new_state = !current_state;
    action.set_state(Glib::Variant<bool>::create(new_state));
    std ::cout << "MyHdyApplicationWindow::Impl::on_toggle_fullscreen " << (new_state ? "fullscreen" : "unfullscreen") << std::endl;
    // Use gtkmm's fullscreen and unfullscreen methods
    if (new_state) 
    {
        parent_.fullscreen();  // Fullscreen the window
    } 
    else 
    {
        parent_.unfullscreen();  // Unfullscreen the window
    }
}


void MyHdyApplicationWindow::Impl::on_toggle_playlist_repeat(Gio::SimpleAction& action)
{
    bool current_state;
    action.get_state(current_state);

    // Toggle the state and set it back to the action
    bool new_state = !current_state;
    action.set_state(Glib::Variant<bool>::create(new_state));

    if (new_state) 
    {
        playlist_.set_repeat(true);
    } 
    else 
    {
        playlist_.set_repeat(false);
    }
}


//not toggle-like, just called when triggered 
void MyHdyApplicationWindow::Impl::on_open_prefs_dlg()
{
    std ::cout << "MyHdyApplicationWindow::Impl::on_open_prefs_dlg " << std::endl;
    prefs_->showUs();
}

void MyHdyApplicationWindow::Impl::next_chapter_action_cb()
{
	printf ("(totem_object_seek_next) Press Next Chapter \n");
	seek_next();
}

void MyHdyApplicationWindow::Impl::previous_chapter_action_cb()
{
	printf ("(totem_object_seek_next) Press Prev Chapter \n");
	seek_previous();
}

void MyHdyApplicationWindow::Impl::play_action_cb()
{
			printf ("(totem-menu) Play/Pause Button Pressed \n");
	play_pause();
}


void MyHdyApplicationWindow::Impl::on_aspect_tatio_toggle(int cur)
{
	bvw_->set_aspect_ratio(static_cast<BvwAspectRatio>(cur));
}


bool MyHdyApplicationWindow::Impl::piece_bitfield_refresh()
{
    using namespace libtorrent;

	lt::bitfield const& bfd = core_->totem_fetch_piece_bitfield();
	bitfield_scale_->update_bitfield(bfd);

	return true;//keep going
}



//fill in playlist
void MyHdyApplicationWindow::Impl::fill_totem_playlist()
{
    using namespace libtorrent;

    lt::torrent_handle const& handle = core_->find_torrent(uniq_id_);

    if(handle.is_valid())
    {
        std::shared_ptr<const torrent_info> ti = handle.torrent_file();
        file_storage fs = ti->files();
        int num_files = fs.num_files();
        int i;
        for(i=0; i<num_files; i++)
        {
            std::string tor_save_path =  handle.save_path();
            
            //fs.file_path() just the path related to torrent
            std::string video_file_name_str = fs.file_path(i);
            tor_save_path += '/';
            std::string video_full_path_str = tor_save_path + video_file_name_str;
std::cout << "fill_totem_playlist " << video_file_name_str << std::endl;
            //add one row in playlist
            playlist_.add_one_row(i, video_file_name_str.c_str(), video_full_path_str.c_str());
        }

		int len = playlist_.get_last() + 1;
		printf ("(fill_totem_playlist) probe: len of playlist is %d\n", len);

		playlist_.set_at_start();

    }
}







void MyHdyApplicationWindow::Impl::set_current_fileidx_and_play()
{
	int file_index_cur = -1;

	file_index_cur = playlist_.get_current_fileidx();

					printf("(totem_object_set_current_fileidx_and_play) get current fileidx %d from totem-playlist \n", file_index_cur);

	if(file_index_cur != -1)
	{
		set_fileidx_and_play(file_index_cur);
	}
}


void MyHdyApplicationWindow::Impl::set_fileidx_and_play(int file_index)
{
							printf("totem_object_set_fileidx_and_play, file_index=%d \n", file_index);
	set_fileidx(file_index);
	play();
}


void MyHdyApplicationWindow::Impl::set_fileidx(int file_index)
{

									printf("(totem_object_set_fileidx), file_idx=%d \n", file_index);

	//when switch to another item in playlist, we should close the current one,
	//FIXME:btdemux may keep pushing even if we Press Next Chapter, call below code, seems it knows nothing about our request
	if(streaming_file_idx_ != -1) 
	{
									printf("(totem_object_set_fileidx, file_idx=%d) Closing the current stream first \n", file_index);
		bvw_->close();
		emit_file_closed();
		play_pause_set_label(STATE_STOPPED);
	}

	if(file_index == -1)
	{
		play_pause_set_label(STATE_PAUSED);
	
		/* Play/Pause */
		action_set_sensitive("play", false);

		/* Volume */
		bitfield_scale_->set_sensitive(bvw_->can_set_volume());
	
		/* Control popup */
		action_set_sensitive("next-chapter", false);
		action_set_sensitive("previous-chapter", false);

		/* Set the label */
		update_player_header_title(NULL);
	}
	//normal case
	else
	{
		bool can_vol_seek;	
		#if 2
		if (is_stream_length_set_ == true) 
		{
								printf ("(totem_object_set_fileidx) clear is_stream_length_set_\n");
								is_stream_length_set_ = false;
		}
		#endif 

							printf("(totem_object_set_fileidx), file_idx=%d gonna call bacon_video_widget_open\n", file_index);
						
		bvw_->open(file_index); 
		mark_popup_busy("opening file");//----------Mark as busy, unmark it when play_starting_cb

		/**************SET FILEIDX***********/
		streaming_file_idx_ = file_index;
		/************************************/

		// Enable Play/Pause Button
		action_set_sensitive("play", true);

		/* Volume */
		can_vol_seek = bvw_->can_set_volume();
		volume_->set_sensitive(can_vol_seek);
	
		/*Play Pause Button icon*/
		play_pause_set_label (STATE_PAUSED);

		const char *fpath = NULL;
		fpath = playlist_.get_current_fullpath();
		//update the totem-movie-properties if it opened
		emit_file_opened(fpath);

		/*set TotemPlayerHeader title*/
		const char* title_cur = playlist_.get_current_filename();
		update_player_header_title(title_cur);
	}

	// update Previous and Next Button sensitivity per playlist 
	update_buttons();
}





void MyHdyApplicationWindow::Impl::play()
{			
	g_autoptr(GError) err = NULL;
	int retval;

	if (streaming_file_idx_ == -1)
	{
		return;
	}

					printf("(totem_object_play) \n");

	if (bvw_->is_playing() != false)
	{
		return;
	}

	retval = bvw_->play(&err);

	//update Play/Pause icon
	play_pause_set_label(retval ? STATE_PLAYING : STATE_STOPPED);

	if (retval != false) 
	{
		unmark_popup_busy("paused");
		return;
	}

	totem_error_dialog(parent_, "Videos could not play.", err->message);
	bvw_->stop();
	play_pause_set_label(STATE_STOPPED);
}




void MyHdyApplicationWindow::Impl::play_pause()
{
				printf("(totem_object_play_pause) \n");

	//only after we received the videos_info from btdemux, which will be used to update playlist, can we call this function
	if (streaming_file_idx_ == -1) 
	{
		play_pause_set_label(STATE_STOPPED);
		//bail out early
		return;
	}

	if (bvw_->is_playing() == false) 
	{
		bvw_->play(NULL);
		play_pause_set_label(STATE_PLAYING);
	} 
	else 
	{
		printf("(totem_object_play_pause) next line to call bacon_video_widget_pause\n");
		bvw_->pause();
		play_pause_set_label(STATE_PAUSED);
	}
}





void MyHdyApplicationWindow::Impl::pause()
{
	if (bvw_->is_playing() != false) {

					printf("(totem_object_pause) next line to call bacon_video_widget_pause\n");
					
		bvw_->pause();
		mark_popup_busy("paused");
		play_pause_set_label(STATE_PAUSED);
	}
}



void MyHdyApplicationWindow::Impl::properties_metadata_updated()
{

	Glib::ustring title;
	Glib::ustring album;
	Glib::ustring artist;
	
	/*use curly brackets scope to prevents the "already initialized GValue" error.*/
	{	
		Glib::ValueBase value;
		bvw_->get_metadata(BVW_INFO_TITLE, value);
		title = static_cast<const Glib::Value<Glib::ustring>&>(value).get();
	} // temporary scope, value destructs goes out of scope



	{
		Glib::ValueBase value;
		bvw_->get_metadata(BVW_INFO_ARTIST, value);
		album = static_cast<const Glib::Value<Glib::ustring>&>(value).get();
	} // temporary scope, value destructs goes out of scope



	{
		Glib::ValueBase value;
		bvw_->get_metadata(BVW_INFO_ALBUM, value);
		album = static_cast<const Glib::Value<Glib::ustring>&>(value).get();
	} // temporary scope, value destructs goes out of scope


							printf ("(totem_get_nice_name_for_stream) emit_metadata_updated \n");

	// tell totem-movie-properties to get metadata tags from BaconVideoWidget
	emit_metadata_updated(title.c_str(),
	                       album.c_str(),
						   artist.c_str());

}




//update title on the TotemPlayerHeader
void MyHdyApplicationWindow::Impl::update_player_header_title(const char *name)
{
	if (name == NULL) 
	{
		time_label_->reset();
		time_rem_label_->reset();

		totem_prop_stream_length_notify(wrapper_);
	}
std::cout << "set_title of player_header_" << std::endl;
	//set totem's player header title
	player_header_->set_title(name);
}







bool MyHdyApplicationWindow::Impl::can_seek_previous()
{
	return 
		(playlist_.have_previous_item() || playlist_.get_repeat());
}

void MyHdyApplicationWindow::Impl::seek_previous()
{
				printf("(totem_object_seek_previous) Press Previous Chapter \n");

	playlist_direction(TOTEM_PLAYLIST_DIRECTION_PREVIOUS);
}




bool MyHdyApplicationWindow::Impl::can_seek_next()
{
	return 
		(playlist_.have_next_item() || playlist_.get_repeat());
}
void MyHdyApplicationWindow::Impl::seek_next()
{
				
	playlist_direction(TOTEM_PLAYLIST_DIRECTION_NEXT);
}


//avoid switch too frequently
bool MyHdyApplicationWindow::Impl::time_within_seconds()
{
	gint64 _time;

	_time = bvw_->get_current_time();

	return (_time < REWIND_OR_PREVIOUS);
}

void MyHdyApplicationWindow::Impl::playlist_direction(TotemPlaylistDirection dir)
{
	if (playlist_.has_direction(dir) == false &&
	    playlist_.get_repeat() == false)
	{
		//if playlist repeat-mode is OFF, and this is last movie in playlist, just do nothing
				printf ("(playlist_direction) repeat-mode is OFF, do nothing \n");
		return;
	}


	if (dir == TOTEM_PLAYLIST_DIRECTION_NEXT ||
	    bvw_->update_and_get_seekable() == false ||
	    time_within_seconds() != false) 
	{

				printf ("(playlist_direction) switch to next item in playlist \n");

		// move playlist "cursor" to next for previous item 	
		playlist_.set_direction(dir);
		// perform switch to next movie in playlist
		set_current_fileidx_and_play();
	} 
	else
	{
		// don't perform switch to another fileidx in playlist, just seek to the very beginning of the current movie
		seek(0);
	}
}



void MyHdyApplicationWindow::Impl::seek(double pos)
{
	g_autoptr(GError) err = NULL;
	int retval;
	
	if(streaming_file_idx_ == -1)
	{
		return;
	}
	
	if(bvw_->update_and_get_seekable() == false)
	{
		printf ("(totem_object_seek) Not seekable, skip this seek\n");
		return;
	}

								printf("(totem_object_seek) seek pos:%lf \n", pos);

	retval = bvw_->seek(pos, &err);

	if (retval == false)
	{
					printf("(totem_object_seek)Videos could not play  \n");
		reset_seek_status();
		totem_error_dialog(parent_, "Videos could not play .", err->message);
	}
}


void MyHdyApplicationWindow::Impl::seek_time(std::int64_t msec, bool accurate)
{
	seek_time_rel(msec, false, accurate);
}


void MyHdyApplicationWindow::Impl::seek_time_rel(std::int64_t _time, bool relative, bool accurate)
{
	g_autoptr(GError) err = NULL;
	std::int64_t sec;

	if (streaming_file_idx_ == -1)
	{
		return;
	}
	if (bvw_->update_and_get_seekable() == false)
	{
		return;
	}

	if (relative != false) 
	{
		std::int64_t oldmsec;
		oldmsec = bvw_->get_current_time();
		sec = MAX (0, oldmsec + _time);
	} 
	else 
	{
		sec = _time;
	}

	bvw_->seek_time(sec, accurate, &err);

	if (err != NULL)
	{
		g_autofree char *msg = NULL;
		g_autofree char *disp = NULL;

		msg = g_strdup_printf("Videos could not play.");

		bvw_->stop();
		play_pause_set_label(STATE_STOPPED);
		totem_error_dialog(parent_, msg, err->message);
	}
}


bool MyHdyApplicationWindow::Impl::is_bvw_seekable()
{
	if (bvw_ == NULL)
		return false;

	return bvw_->update_and_get_seekable() != false;
}



void MyHdyApplicationWindow::Impl::reset_seek_status()
{
	/* Release the lock and reset everything so that we
	 * avoid being "stuck" seeking on errors */
	if (seek_lock_ != false) 
	{
					// printf ("(totem/reset_seek_status) Unset seek_lock_\n");
					std::cout << "(totem/reset_seek_status) Unset seek_lock_" << std::endl;
		seek_lock_ = false;
		unmark_popup_busy("seek started");
		bvw_->seek(0, NULL);
		bvw_->stop();
		play_pause_set_label(STATE_STOPPED);
	}else{
		// printf ("(totem/reset_seek_status) Lock already unlocked\n");
		std::cout << "(totem/reset_seek_status) seek_lock_ already unset" << std::endl;
	}
}






//when click Play/Pause, update icon
void MyHdyApplicationWindow::Impl::play_pause_set_label(TotemStates state)
{

				printf("(totem-object/play_pause_set_label) \n");

	Gtk::Image *image = nullptr;
	const char *id, *tip;

	if (state == state_)
	{
		return;
	}

	switch (state)
	{
		case STATE_PLAYING://Double Bar ||
			id = "media-playback-pause-symbolic";
			tip = "Pause";
			time_label_->set_show_msecs(false);
			break;
		case STATE_PAUSED://Triangle |>
			id = "media-playback-start-symbolic";
			tip = "Play";
			break;
		case STATE_STOPPED://Triangle |>
			time_label_->reset();
			time_rem_label_->reset();
			id = "media-playback-start-symbolic";
			tip = "Play";
			break;
		default:
			g_assert_not_reached();
			return;
	}


	play_button_->set_tooltip_text(tip);
	image = dynamic_cast<Gtk::Image*>(play_button_->get_image());
	// image = play_button_->get_image();
	if(image)
		image->set_from_icon_name(id, Gtk::ICON_SIZE_MENU);
	
	state_ = state;

}




//this is to update disable or enable of Previous and Next Button 
void MyHdyApplicationWindow::Impl::update_buttons()
{
	action_set_sensitive("previous-chapter",
				       can_seek_previous());
	action_set_sensitive("next-chapter",
				       can_seek_next());
}








/*******************************************SIGNALS******************************************************/
//when this func called, the video file maybe not exist for initial time of start torrenting
// NO USE FOR NOW
void MyHdyApplicationWindow::Impl::on_btdemux_destructing()
{
	if(core_)
	{
		core_->btdemux_should_close();
	}
}

void MyHdyApplicationWindow::Impl::emit_file_opened(const char *fpath)
{
	//Let TotemWrapper proxy us 
	totem_fileidx_opened_signal_emit(wrapper_, fpath);
}



void MyHdyApplicationWindow::Impl::emit_file_closed()
{
	//Let TotemWrapper proxy us 
	totem_fileidx_closed_signal_emit(wrapper_);
}



void MyHdyApplicationWindow::Impl::emit_metadata_updated(const char *artist,
							const char *title,
							const char *album)
{
	//Let TotemWrapper proxy us 
	totem_metadata_updated_signal_emit(wrapper_,artist, title, album);
}



void MyHdyApplicationWindow::Impl::on_eos_event()
{
	int fidx = -1;
	reset_seek_status();
	//***For current stream is Last item in playlist and repeat mode is OFF
	if (
		//if it's last item of playlist, `totem_playlist_have_next_item` return false
		playlist_.have_next_item() == false &&
	    //playlist repeat mode is OFF
	    playlist_.get_repeat() == false &&
		//There is more than one item in playlist OR current stream is NOT seekable
	    (playlist_.get_last() != 0 ||
		is_bvw_seekable() == false)
	)
	{

								printf("(totem-object/on_eos_event) FIRST case\n");

		//go back to start of playlist end pause
		playlist_.set_at_start();
		update_buttons();
		bvw_->stop();
		play_pause_set_label(STATE_STOPPED);

		//such as when playing last item in playlist, when reach eos, it will start back at the first item in playlist and pause
		fidx = playlist_.get_current_fileidx();
								printf("(totem-object/on_eos_event) get current fileidx:%d and set it \n", fidx);
		if(fidx != -1)		
		{
			set_fileidx(fidx);
		}
								printf("(totem-object/on_eos_event) gonna  call bacon_video_widget_pause \n");
		bvw_->pause();
	} 
	//***For current stream is Non-Last item in playlist, or Reapt-mode is ON, 
	// no pause and play next no matter the current stream is in last or not
	else 
	{
								printf("(totem-object/on_eos_event) SECOND case\n");
		//***For playlist has only single item Case
		if (playlist_.get_last() == 0 &&
		    is_bvw_seekable()) 
		{
			if (playlist_.get_repeat() != false) 
			{
						printf ("(totem-object/on_eos_event) single item in playlist, Repeat-mode is ON \n");
				//play from start
				seek_time(0, false);
				play();
			} 
			else 
			{
						printf ("(totem-object/on_eos_event) single item in playlist, Repeat-mode is OFF \n");
				//pause at start
				pause();
				seek_time(0, false);
			}
		} 
		//***For playlist has multiple items Case
		else 
		{
						printf ("(totem-object/on_eos_event) since playlist has multiple items, so seek next item\n");
			seek_next();
		}
	}
}



void MyHdyApplicationWindow::Impl::on_error_event(const char *message, bool playback_stopped)
{

									printf("(totem-object/on_error_event) \n");

	if (playback_stopped)
	{
		play_pause_set_label(STATE_STOPPED);
	}

	totem_error_dialog (parent_, "An error occurred", message);
}




void MyHdyApplicationWindow::Impl::update_slider_visibility(std::int64_t stream_length)
{
	//if stream_length not change, do nothing
	if (stream_length_ == stream_length)
	{
		return;
	}
	//if old and new stream_length are both > 0,keep as it is 
	if (stream_length_ > 0 && stream_length > 0)
	{
		return;
	}

	if (stream_length != 0)
		bitfield_scale_->set_range(0., 65535.);
	else
		bitfield_scale_->set_range(0., 0.);

}


void MyHdyApplicationWindow::Impl::update_current_time(std::int64_t current_time,
													   std::int64_t stream_length,
													   double current_position,
													   bool seekable)
{

								// printf("(totem-object/update_current_time) \n");

	update_slider_visibility(stream_length);

	if (seek_lock_ == false) 
	{

		// std::cout << "update_current_time, seek_lock_ is unset, ok to set scale_adj_ value " << std::endl;

		//update position of progress bar
		scale_adj_->set_value(current_position * 65535);

		/* and current/remaining time label*/
		// fileidx is set (this maybe redundant cuz when this func get called, fileidx is guaranteed to already be set)
		// and stream_length is unknown, not set yet
		if (stream_length == 0 && streaming_file_idx_ != -1) 
		{

			// std::cout << "update_current_time, stream_length not known, set Unknow on timelable " << std::endl;


			// current time label shown as "--:--"
			time_label_->set_time (current_time, -1);
		
			// remaining time label shown as "--:--"
			time_rem_label_->set_time (current_time, -1);

		} 
		//stream_length is received and is valid (a positive number)
		else 
		{
			// std::cout << "update_current_time, OK to set timelabel " << std::endl;


			// current time label shown eg: "0:03"
			time_label_->set_time (current_time, stream_length);

			// remaining time label shown eg: "-19:30"
			time_rem_label_->set_time (current_time, stream_length);
		}
	}

	//monitoring the change on stream_length
	if (stream_length_ != stream_length) 
	{

					#if 2
					if (!is_stream_length_set_ && stream_length >0 && stream_length_==0) {
						is_stream_length_set_ = true;
						printf ("(update_current_time) From now on, We get the stream_length, which is %ld \n", stream_length);
					}
					#endif

		//notify totem-movie-properties.c
		totem_prop_stream_length_notify(wrapper_);
		stream_length_ = stream_length;
	}

}




void MyHdyApplicationWindow::Impl::on_play_starting ()
{
				printf("play_starting_cb \n");
	//this means the toolbox can hide automatically
	unmark_popup_busy("opening file");
}




/*what means got-metadata
it means decodebin srcpad have added (so moov atom surely been parsed before this), start to play
*/
void MyHdyApplicationWindow::Impl::on_got_metadata()
{
						printf ("(on_got_metadata) \n");

	properties_metadata_updated();
	const char* title_cur = playlist_.get_current_filename();
	update_player_header_title(title_cur);
	update_buttons();
}






void MyHdyApplicationWindow::Impl::update_seekable()
{
							// printf("(totem-object/update_seekable) \n");
	bool seekable;

	seekable = bvw_->update_and_get_seekable();
			
	/* Check if the stream is seekable */
	bitfield_scale_->set_sensitive(seekable);
}


void MyHdyApplicationWindow::Impl::property_notify_cb_seekable()
{
			// printf("(totem-object) property_notify_cb_seekable \n");
	update_seekable();
}





/*
Three kind of seek:
Case 1.Drag exactly over slider to seek (you button press the slider, hold it with motion, any value change of adjustment will not handled until release button)

Case 2.Button single-press seek (no holding the slider, just button press desired point of progress bar)

Case 3.Hold Button press with motion but cursor not over slider(button press the scale area, and drag whether inside or outside the scale, 
motion will also change the slider potiiton)
*/
//seek_slider_changed_cb may called multiple times before seek_slider_pressed_cb, ignore them
bool MyHdyApplicationWindow::Impl::seek_slider_pressed_cb(GdkEventButton *event)
{

	std::cout << "(seek_slider_pressed_cb) Set seek_lock_" << std::endl;

	/* HACK: we want the behaviour you get with the left button, so we
	 * mangle the event.  clicking with other buttons moves the slider in
	 * step increments, clicking with the left button moves the slider to
	 * the location of the click.
	 */
	event->button = GDK_BUTTON_PRIMARY;

	auto settings = bitfield_scale_->get_settings();
    settings->property_gtk_primary_button_warps_slider() = true;

	//acquire seek_lock
	seek_lock_ = true;

	mark_popup_busy("seek started");

	return false;
}

bool MyHdyApplicationWindow::Impl::seek_slider_released_cb(GdkEventButton *event)
{
	Glib::RefPtr<Gtk::Adjustment> adj;
	double val;

	std::cout << "(seek_slider_released_cb) Unset seek_lock_" << std::endl;

	/* HACK: see seek_slider_pressed_cb */
	event->button = GDK_BUTTON_PRIMARY;

	/* set to false here to avoid triggering a final seek when
	 * syncing the adjustments while being in direct seek mode */
	seek_lock_ = false;
	unmark_popup_busy("seek started");

	/* sync both adjustments */
	adj = bitfield_scale_->get_adjustment();
	val = adj->get_value();
	
	/*
	HACK:when button-single-press seek, seek_slider_released_cb who unset seek_lock_ called 
	right after seek_slider_pressed_cb who set seek_lock_
	so seek_slider_changed_cb() will be return early, got no chance to call seek
	*/
	//For Case 2
	//update time label later in update_current_time()
	if(bvw_->can_direct_seek())
	{

			// printf("(totem-object/seek_slider_released_cb) seek\n");
			std::cout << "(seek_slider_released_cb) button-single-press seek" << std::endl;

		seek(val / 65535.0);
	}

	return false;
}

void MyHdyApplicationWindow::Impl::seek_slider_changed_cb()
{
	// std::cout << "(seek_slider_changed_cb) enter both" << std::endl;

	double pos;
	std::int64_t _time;

	/*ignore until button-press-event triggered*/
	if(seek_lock_ == false)
	{
		// std::cout << "(seek_slider_changed_cb) seek_lock is unset, return now " << std::endl;
		return;
	}

	/******For Case 1 and 3*******/
	//get the pos in percent you point the seek bar to
	pos = scale_adj_->get_value() / 65535;

	//update time label
	_time = bvw_->update_and_get_stream_length();//in milliseconds
	time_label_->set_time(pos*_time, _time);
	time_rem_label_->set_time(pos*_time, _time);


	if (bvw_->can_direct_seek()) 
	{
									std::cout << "(seek_slider_changed_cb seek_lock) drag-seek pos is " << pos << std::endl;
									// printf eg. "seek_slider_changed_cb, pos is 0.759259"
		seek(pos);
	}
}






void MyHdyApplicationWindow::Impl::update_volume_sliders()
{
	
	double volume;
	volume = bvw_->get_volume();

							std::cout << "update_volume_sliders: " << volume << std::endl;
	
	volume_changed_tag_.block();/*****************************/
	//volume_changed_tag_ is blocked, so this set_volume will not trigger signal_value_changed, so volume_button_value_changed_cb not called 
	volume_->set_value(volume);
	volume_changed_tag_.unblock();/***************************/
}

void MyHdyApplicationWindow::Impl::property_notify_cb_volume()
{
						std::cout << "property_notify_cb_volume" << std::endl;
	update_volume_sliders();
}

void MyHdyApplicationWindow::Impl::volume_button_value_changed_cb(double value)
{

						// printf("volume_button_value_changed_cb: %lf\n", value);
						std::cout << "volume_button_value_changed_cb: " << value << std::endl;
	bvw_->set_volume(value);//this will trigger signal_value_changed
}

bool MyHdyApplicationWindow::Impl::volume_button_scroll_event_cb(GdkEventScroll *event)
{

						// printf("Mouse Wheel volume_button_scroll_event_cb: \n");
						std::cout << "volume_button_scroll_event_cb: " <<  std::endl;

	bool increase;

	if (event->direction == GDK_SCROLL_SMOOTH) {
		gdouble delta_y;

		gdk_event_get_scroll_deltas ((GdkEvent *) event, NULL, &delta_y);
		if (delta_y == 0.0)
			return GDK_EVENT_PROPAGATE;

		increase = delta_y < 0.0;
	} else if (event->direction == GDK_SCROLL_UP) {
		increase = true;
	} else if (event->direction == GDK_SCROLL_DOWN) {
		increase = SEEK_BACKWARD_OFFSET * 1000;
	} else {
		return GDK_EVENT_PROPAGATE;
	}

	set_volume_relative(increase ? VOLUME_UP_OFFSET : VOLUME_DOWN_OFFSET);
	return GDK_EVENT_STOP;
}

void MyHdyApplicationWindow::Impl::set_volume_relative(double off_pct)
{
	volume_changed_tag_.block();

						// printf("totem_object_set_volume_relative: %lf\n", off_pct);
						std::cout << "(totem_object_set_volume_relative): " << off_pct << std::endl;
	double vol;
	if (bvw_->can_set_volume() == false)
		return;
	vol = bvw_->get_volume();
	if(vol >= 100)
	{
		return;
	}
	bvw_->set_volume(vol + off_pct);

	volume_changed_tag_.unblock();
}


void MyHdyApplicationWindow::Impl::volume_button_menu_shown_cb()
{
	Gtk::Widget* volume_popup = volume_->get_popup();

	if(volume_popup->property_visible().get_value() == true)
		mark_popup_busy("volume menu visible");
	else
		unmark_popup_busy("volume menu visible");
}







void MyHdyApplicationWindow::Impl::unschedule_hiding_popup()
{
	if (transition_timer_.connected())
		transition_timer_.disconnect();
}

//when no mouse motion for a few seconds , the button toolbar will hide, when mouse reach within the player area, it will pop up again
bool MyHdyApplicationWindow::Impl::hide_popup_timeout_cb()
{
	// std::cout << "hide_popup_timeout_cb " << std::endl;
	set_controls_visibility(false, true);
	unschedule_hiding_popup();
	return false;
}

void MyHdyApplicationWindow::Impl::schedule_hiding_popup()
{
	unschedule_hiding_popup();
	transition_timer_ = Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, 
				&MyHdyApplicationWindow::Impl::hide_popup_timeout_cb), POPUP_HIDING_TIMEOUT);
}

/****MenuButton popup menu manage******/
//these two methods and the GHashTable is for: when popup-menu(toolbox go-menu and player header menu) is shown,or during bacon open, or during seek
//It Tell Totem's Window: Hey,  Don't hide the buttom toolbox automatically
void MyHdyApplicationWindow::Impl::mark_popup_busy(const char *reason)
{

				printf("mark_popup_busy %s\n",reason);
			
	g_hash_table_insert (busy_popup_ht_,
		g_strdup (reason)/*key*/,
		GINT_TO_POINTER (1)/*key*/);
	
	// g_debug ("Adding popup busy for reason %s", reason);

	set_controls_visibility(true, false);
	unschedule_hiding_popup();
}

void MyHdyApplicationWindow::Impl::unmark_popup_busy(const char *reason)
{
				printf("-unmark_popup_busy %s\n", reason);

	g_hash_table_remove (busy_popup_ht_, reason);

	// g_debug ("Removing popup busy for reason %s", reason);

	if (g_hash_table_size (busy_popup_ht_) == 0 &&
		toolbox_->get_opacity() != 0.0) 
	{
		// g_debug ("Will hide popup soon");
		schedule_hiding_popup();
	}
}

void MyHdyApplicationWindow::Impl::set_controls_visibility(bool visible, bool animate)
{	
	toolbox_->set_visible(visible);

	if (controls_visibility_ == TOTEM_CONTROLS_FULLSCREEN)
	{
		flap_->set_reveal_flap(visible);
	}
	bvw_->set_show_cursor(visible);
	if (visible && animate)
	{
		schedule_hiding_popup();
	}
	reveal_controls_ = visible;
}

/*when mouse hovering on player area, it will pop up the buttom toolbar of player */
bool MyHdyApplicationWindow::Impl::on_bvw_motion_notify(GdkEventMotion* motion_event)
{
	if (!reveal_controls_)
		set_controls_visibility(true, true);

	return GDK_EVENT_PROPAGATE;
}



//FullScreen Switch Handling
bool MyHdyApplicationWindow::Impl::window_state_event_cb (GdkEventWindowState *event)
{
	GAction *action;
	bool is_fullscreen;

	maximised_ = !!(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);

	if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
	{
		return false;
	}

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
		if (controls_visibility_ != TOTEM_CONTROLS_UNDEFINED)
			window_save_size();

		controls_visibility_ = TOTEM_CONTROLS_FULLSCREEN;
	} else {
		controls_visibility_ = TOTEM_CONTROLS_VISIBLE;
		window_save_size();
	}

	is_fullscreen = (controls_visibility_ == TOTEM_CONTROLS_FULLSCREEN);
	flap_->set_fold_policy(is_fullscreen);
			
	
	player_header_->set_fullscreen_mode(is_fullscreen);


	//Also update action in action_group_
	auto action_temp = Glib::RefPtr<Gio::SimpleAction>::cast_dynamic(action_group_->lookup_action("fullscreen"));
    action_temp->set_state(Glib::Variant<bool>::create(is_fullscreen));


	if (transition_timer_.connected())
	{
		set_controls_visibility(true, false);
	}

	return false;
}


bool MyHdyApplicationWindow::Impl::window_is_fullscreen ()
{
	return (controls_visibility_ == TOTEM_CONTROLS_FULLSCREEN);
}


//double-click on video frame to set fullscreen
bool MyHdyApplicationWindow::Impl::on_video_button_press_event (GdkEventButton *event)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 1) 
	{
		bvw_->grab_focus();
		return true;
	} 
	else if (event->type == GDK_2BUTTON_PRESS &&
		   event->button == 1 &&
		   event_is_touch (event) == false)
	{

		if (window_is_fullscreen() != false)
		{
			parent_.unfullscreen();
		}
		else
		{
			parent_.fullscreen();
		}

		return true;
	} 
	else if (event->type == GDK_BUTTON_PRESS && event->button == 2) 
	{
		play_pause();
		return true;
	}

	return false;
}

void MyHdyApplicationWindow::Impl::popup_menu_shown_cb(Gtk::MenuButton* button)
{
	bool is_active = button->get_active();
	if (is_active)
		mark_popup_busy("toolbar/go menu visible");
	else
		unmark_popup_busy("toolbar/go menu visible");
}







void MyHdyApplicationWindow::Impl::check_video_widget_created()
{
					printf("(totem-object/check_video_widget_created) \n");
				

	Glib::Error err;
	bool should_exit = false;

	if (settings_->get_boolean("force-software-decoders"))
	{
		totem_gst_disable_hardware_decoders();
	}
	else
	{
		totem_gst_ensure_newer_hardware_decoders();
	}


	if(!bvw_->check_gstreamer_init(err))
	{
		totem_error_dialog_blocking(parent_, "Bacon GStreamer library initalization failed.",
			err.what().c_str());
		
		should_exit = true;
	}
	if(!bvw_->check_pipeline_init(err)) 
	{
		totem_error_dialog_blocking(parent_, "Bacon GStreamer pipeline construct failed.",
						err.what().c_str());
		
		if(!should_exit)
			should_exit = true;

		//manually close the Totem
		// Create a dummy GdkEventAny
		//   GdkEventAny event;
		//   event.type = GDK_DELETE;
		//   event.window = gtk_widget_get_window(GTK_WIDGET(parent_.gobj()));
		//   event.send_event = true;

	}
	
	if (should_exit)	
		parent_.close();

	//Need it ?
	// bvw_->realize(); no is protected
	// gtk_widget_realize(GTK_WIDGET(bvw_->gobj()));
}






void MyHdyApplicationWindow::Impl::window_save_size()
{
	if (bvw_ == nullptr)
		return;

	if (window_is_fullscreen() != false)
		return;

	/* Save the size of the video widget */
	parent_.get_size(window_w_/*pass by ref*/, window_h_/*pass by ref*/);
}

//save totem player  window width/height, is maximised.. serialized in state.ini file
void MyHdyApplicationWindow::Impl::save_window_state ()
{
	GKeyFile *keyfile;
	g_autofree char *contents = NULL;
	g_autofree char *filename = NULL;


	if (window_w_ == 0 || window_h_ == 0)
		return;

	keyfile = g_key_file_new ();
	g_key_file_set_integer (keyfile, "State",
				"window_w", window_w_);
	g_key_file_set_integer (keyfile, "State",
			"window_h", window_h_);
	g_key_file_set_boolean (keyfile, "State",
			"maximised", maximised_);

	contents = g_key_file_to_data (keyfile, NULL, NULL);
	g_key_file_free (keyfile);
	filename = g_build_filename(totem_dot_config_dir(), "state.ini", NULL);
	g_file_set_contents (filename, contents, -1, NULL);
}

//load size from state.ini file
void MyHdyApplicationWindow::Impl::setup_window_size()
{
								printf("(totem_setup_window) \n");

	GKeyFile *keyfile;
	int w, h;
	g_autofree char *filename = NULL;

	/*save serialized file*/
	filename = g_build_filename(totem_dot_config_dir(), "state.ini", NULL);
	// filename is /home/pal/config/totem/state.ini 	
	keyfile = g_key_file_new ();
	if (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL) == false) 
	{
		w = DEFAULT_WINDOW_W;
		h = DEFAULT_WINDOW_H;
		maximised_ = true;
	} 
	else 
	{
		GError *err = NULL;
		w = g_key_file_get_integer (keyfile, "State", "window_w", &err);
		if (err != NULL) 
		{
			w = 0;
			g_clear_error (&err);
		}
		h = g_key_file_get_integer (keyfile, "State", "window_h", &err);
		if (err != NULL) 
		{
			h = 0;
			g_clear_error (&err);
		}
		maximised_ = g_key_file_get_boolean (keyfile, "State",
				"maximised", NULL);
	}
	if (w > 0 && h > 0 && maximised_ == false)
	{
		parent_.set_default_size(w, h);
		window_w_ = w;
		window_h_ = h;
	} 
	else if (maximised_ != false) 
	{
		parent_.maximize();
	}


	return;
}







/*************** PUBLIC EXPOSED METHODS (impement ITotemWindow interface, used by those plugins who needs access TotemWindow) *******************/
void MyHdyApplicationWindow::someMethod()
{
    std::cout << "someMethod called" << std::endl;
}


BvwRotation MyHdyApplicationWindow::get_bvw_rotation()
{
	return impl_->get_bvw_rotation();
}
BvwRotation MyHdyApplicationWindow::Impl::get_bvw_rotation()
{
	return bvw_->get_rotation();
}


void MyHdyApplicationWindow::set_bvw_rotation(BvwRotation rotation)
{
	impl_->set_bvw_rotation(rotation);
}
void MyHdyApplicationWindow::Impl::set_bvw_rotation(BvwRotation rotation)
{
	bvw_->set_rotation(rotation);
}


bool MyHdyApplicationWindow::set_bvw_rate(float rate)
{
	return impl_->set_bvw_rate(rate);
}
bool MyHdyApplicationWindow::Impl::set_bvw_rate(float rate)
{
	return bvw_->set_rate(rate);
}


Glib::RefPtr<Gio::Menu> MyHdyApplicationWindow::get_plugin_menu_placeholder(MenuPlaceHolderType type) 
{
	return impl_->get_plugin_menu_placeholder(type);
}
Glib::RefPtr<Gio::Menu> MyHdyApplicationWindow::Impl::get_plugin_menu_placeholder(MenuPlaceHolderType type) 
{
	Glib::RefPtr<Gio::Menu> ret;

	switch(type)
	{
		case MenuPlaceHolderType::OPENDIR_PLACEHOLDER:
				ret = opendir_placeholder_;
			break;
		case MenuPlaceHolderType::ROTATION_PLACEHOLDER:
				ret = rotation_placeholder_;
			break;	
		case MenuPlaceHolderType::PROPETIES_PLACEHOLDER:
				ret = prop_placeholder_;
			break;
		case MenuPlaceHolderType::VARIABLE_RATE_PLACEHOLDER:
				ret = variable_rate_placeholder_;
			break;
		default:
			break;
	}

	return ret;
}


void MyHdyApplicationWindow::empty_plugin_menu_placeholder (MenuPlaceHolderType type) 
{
	impl_->empty_plugin_menu_placeholder(type);
}
void MyHdyApplicationWindow::Impl::empty_plugin_menu_placeholder (MenuPlaceHolderType type) 
{
	Glib::RefPtr<Gio::Menu> tar = Glib::RefPtr<Gio::Menu>();
	switch(type)
	{
		case MenuPlaceHolderType::OPENDIR_PLACEHOLDER:
			tar = opendir_placeholder_;
			break;
		case MenuPlaceHolderType::ROTATION_PLACEHOLDER:
			tar = rotation_placeholder_;
			break;	
		case MenuPlaceHolderType::PROPETIES_PLACEHOLDER:
			tar = prop_placeholder_;
			break;
		case MenuPlaceHolderType::VARIABLE_RATE_PLACEHOLDER:
			tar = variable_rate_placeholder_;
			break;
		default:
			break;
	}

	if(tar)
	{
		try
		{
			while(tar->get_n_items() > 0)
			{
				Glib::VariantBase action_variant = tar->get_item_attribute(0, Gio::MenuAttribute::MENU_ATTRIBUTE_ACTION,  Glib::VariantType("s"));
				std::string action_name = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(action_variant).get();
				if(action_name.compare(0, 4, "app.") == 0)
				{
					Glib::VariantBase action_target = tar->get_item_attribute(0, Gio::MenuAttribute::MENU_ATTRIBUTE_TARGET, {});

					/* Don't remove actions that have a specific target */
					if (action_target == nullptr)
					{
						action_group_->remove_action(action_name.substr(strlen("app.")));
					}
				}
				tar->remove(0);
			}
		}
		catch(const Glib::Error& ex)
		{
			std::cout << "Exceptions when empty plugin menu section: " << ex.what() << std::endl;
		}
	}
}


void MyHdyApplicationWindow::add_plugin_action_to_group(const Glib::RefPtr<Gio::Action>& action)
{
	impl_->add_plugin_action_to_group(action);
}
void MyHdyApplicationWindow::Impl::add_plugin_action_to_group(const Glib::RefPtr<Gio::Action>& action)
{
	action_group_->add_action(action);
}


const char* MyHdyApplicationWindow::get_current_fullpath()
{
	return impl_->get_current_fullpath();
}
const char* MyHdyApplicationWindow::Impl::get_current_fullpath()
{
	const char* fpath;
	fpath = playlist_.get_current_fullpath();
	return fpath;
}


GtkWindow* MyHdyApplicationWindow::get_main_window_gobj() 
{
    return this->gobj();  
}


gint64 MyHdyApplicationWindow::get_bvw_stream_length()
{
	return impl_->get_bvw_stream_length();

}
gint64 MyHdyApplicationWindow::Impl::get_bvw_stream_length()
{
	return bvw_->update_and_get_stream_length();
}


void MyHdyApplicationWindow::get_bvw_metadata(BvwMetadataType type, Glib::ValueBase& value)
{
	impl_->get_bvw_metadata(type, value);
}

void MyHdyApplicationWindow::Impl::get_bvw_metadata(BvwMetadataType type, Glib::ValueBase& value)
{
	bvw_->get_metadata(type, value);
}































