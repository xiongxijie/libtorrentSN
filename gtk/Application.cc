// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#include "Application.h"
#include "Torrent.h"
#include "Actions.h"
#include "DetailsDialog.h"
#include "Dialogs.h"
#include "FilterBar.h"
#include "GtkCompat.h"
#include "HigWorkarea.h" // GUI_PAD, GUI_PAD_BIG
#include "MainWindow.h"
#include "MakeDialog.h"
#include "MessageLogWindow.h"
#include "OptionsDialog.h"
#include "PathButton.h"
#include "Prefs.h"
#include "PrefsDialog.h"
#include "RelocateDialog.h"
#include "Session.h"
#include "StatsDialog.h"
#include "Utils.h"
#include "tr-transmission.h"
#include "tr-log.h"
#include "tr-utils.h"
#include "tr-version.h"



#include <gdkmm/display.h>
#include <giomm/appinfo.h>
#include <giomm/error.h>
#include <giomm/menu.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/value.h>
#include <glibmm/vectorutils.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/grid.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/window.h>
#include <gtkmm/dialog.h>

#if GTKMM_CHECK_VERSION(4, 0, 0)
#include <gtkmm/droptarget.h>
#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/shortcutcontroller.h>
#else
#include <gdkmm/dragcontext.h>
#include <gtkmm/selectiondata.h>
#endif

#include <fmt/core.h>

#include <algorithm>
#include <csignal>
#include <cstdlib> // exit()
#include <ctime>
#include <iterator> // std::back_inserter
#include <map>
#include <memory> //for std::unique_ptr, std::shared_ptr, std::weak_ptr
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <glib/gmessages.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif


#include "libtorrent/session.hpp"
#include "libtorrent/torrent_flags.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/session_params.hpp"


using namespace std::literals;

#if GTKMM_CHECK_VERSION(4, 0, 0)
using FileListValue = Glib::Value<GSList*>;
using FileListHandler = Glib::SListHandler<Glib::RefPtr<Gio::File>>;

using StringValue = Glib::Value<Glib::ustring>;
#endif

#define SHOW_LICENSE

namespace
{

auto const AppIconName = "transmission"sv; // TODO(C++20): Use ""s

char const* const LICENSE =
    "Copyright 2005-2024. All code is copyrighted by the respective authors.\n"
    "\n"
    "Transmission can be redistributed and/or modified under the terms of the "
    "GNU GPL, versions 2 or 3, or by any future license endorsed by Mnemosyne LLC."
    "\n"
    "In addition, linking to and/or using OpenSSL is allowed.\n"
    "\n"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
    "\n"
    "Some of Transmission's source files have more permissive licenses. "
    "Those files may, of course, be used on their own under their own terms.\n";

} //anonymous namespace





/*************
*****Impl*****
**************
**************/
class Application::Impl
{
public:
    Impl(Application& app, std::string const& config_dir, bool start_paused);
    ~Impl() = default;

    TR_DISABLE_COPY_MOVE(Impl)


    void open_files(std::vector<Glib::RefPtr<Gio::File>> const& files);


    void on_startup();
    void on_activate();


    void actions_handler(Glib::ustring const& action_name);


    lt::session const& get_session_handle() const;




    

private:
    struct counts_data
    {
        int total_count = 0;
        int queued_count = 0;
        int stopped_count = 0;
    };

private:
    void show_details_dialog_for_selected_torrents();
    void show_about_dialog();

    bool refresh_actions();
    void refresh_actions_soon();

    void on_main_window_size_allocated();
    void on_main_window_focus_in();

#if GTKMM_CHECK_VERSION(4, 0, 0)
    bool on_drag_data_received(Glib::ValueBase const& value, double x, double y);
#else
    void on_drag_data_received(
        Glib::RefPtr<Gdk::DragContext> const& drag_context,
        gint x,
        gint y,
        Gtk::SelectionData const& selection_data,
        guint info,
        guint time_);
#endif


    void placeWindowFromPrefs();
    void presentMainWindow();
    void hideMainWindow();
    void toggleMainWindow();

    bool winclose();
    void rowChangedCB(std::unordered_set<std::uint32_t> const& uniq_ids, Torrent::ChangeFlags changes);

    void app_setup();
    void main_window_setup();

    bool on_session_closed();
    void on_app_exit();

    void show_torrent_errors(Glib::ustring const& primary, std::vector<std::string>& files);
    void flush_torrent_errors();

    bool update_model_once();
    void update_model_soon();
    bool update_model_loop();

    void on_core_busy(bool busy);
    void on_core_error(Session::ErrorCode code, Glib::ustring const& msg);
    void on_add_torrent(lt::add_torrent_params atp ,char const* dottor_filename, char const* download_dir);
    void on_prefs_changed(int const key);

    [[nodiscard]] std::vector<std::uint32_t> get_selected_torrent_uniq_ids() const;
    [[nodiscard]] counts_data get_selected_torrent_counts() const;

    void copy_magnet_link_to_clipboard(Glib::RefPtr<Torrent> const& torrent) const;
    

    std::vector<lt::torrent_handle> get_selected_torrent_handle() const;
    std::vector<lt::torrent_status> get_selected_torrent_status();
    std::vector<lt::torrent_status> get_torrent_status() const;

    void start_all_torrents();
    void pause_all_torrents();
    bool stop_torrents_auto_managed();
    bool start_torrents_auto_managed();
    bool force_start_torrents();
    bool do_force_recheck();
    bool do_force_reannounce();
    bool do_scrape_last_working();
    
    enum alter_queue_pos_mode_t
    {
        alter_queue_pos_up,
        alter_queue_pos_down,
        alter_queue_pos_top,
        alter_queue_pos_bottom
    };


    bool alter_queue_position(alter_queue_pos_mode_t mode);


    void remove_selected(bool delete_files);


    enum class PeriphralDialogEnum
    {
        MakeDialog,
        RelocateDialog,
        StatsDialog,
        TorrentUrlChooserDialog 
    };

    std::string getPeriphralDialogName(PeriphralDialogEnum dialog);


private:
    Application& app_;
    //where to search configuration files such as all settings packs serialized
    //and torrent .resume files
    std::string const config_dir_;
    bool const start_paused_;
    // bool const start_iconified_;
    //minimized in notification area
    bool is_iconified_ = false;
    bool is_closing_ = false;

    Glib::RefPtr<Gtk::Builder> ui_builder_;

    unsigned int activation_count_ = 0;
    sigc::connection timer_;
    sigc::connection update_model_soon_tag_;
    sigc::connection refresh_actions_tag_;
    /*MainWindow*/
    std::unique_ptr<MainWindow> wind_;
    // std::unique_ptr<SystemTrayIcon> icon_;

    //depends-on
    Glib::RefPtr<Session> core_;
    
    /*message log dialog*/
    std::unique_ptr<MessageLogWindow> msgwin_;

    /*edit-preferences dialog*/
    std::unique_ptr<PrefsDialog> prefs_;

    /*maps from uniqid to respective details dialog*/
    std::map<std::uint32_t, std::unique_ptr<DetailsDialog>> details_;

    /*maps from info_hash_t-obj to respective OptionsDialog */
    std::map<lt::info_hash_t, std::unique_ptr<OptionsDialog>> open_tor_options_;

    std::vector<std::string> error_list_;
    std::vector<std::string> duplicates_list_;

    //stores all shared_ptr of opened Dialog used for cleanup when close program
    // std::vector<std::shared_ptr<Gtk::Dialog>> active_dialogs_;



    std::unordered_map<PeriphralDialogEnum, std::shared_ptr<Gtk::Dialog>> active_dialogs_;


    // lt::session ses_;
    // lt::session_proxy ses_proxy_;

};







/*************************************************************************** */
namespace
{

template<typename T>
void gtr_window_present(T const& window)
{
    window->present(GDK_CURRENT_TIME);
}



//****  DETAILS DIALOGS MANAGEMENT
std::string get_details_dialog_key(std::vector<std::uint32_t> const& uniq_id_list)
{
    auto tmp = uniq_id_list;
    std::sort(tmp.begin(), tmp.end());

    std::ostringstream gstr;

    for (auto const id : tmp)
    {
        gstr << id << ' ';
    }

    return gstr.str();
}



} //anonymous namespace




// Function to get string representation of the enum values
std::string Application::Impl::getPeriphralDialogName(PeriphralDialogEnum dialog) 
{
    switch (dialog) {
        case PeriphralDialogEnum::MakeDialog:
            return "MakeDialog";
        case PeriphralDialogEnum::RelocateDialog:
            return "RelocateDialog";    
        case PeriphralDialogEnum::StatsDialog:
            return "StatsDialog";
        case PeriphralDialogEnum::TorrentUrlChooserDialog:
            return "TorrentUrlChooserDialog";
        default:
            return "UnknownDialog";
    }
}


//@brief torrnet detail info (peer,tracker,etc.), most posted alerts are represented there
void Application::Impl::show_details_dialog_for_selected_torrents()
{
    //should only select single torrent, cuz we disable `Show-Property` action when you multi-select 
    //so  this vector only has single torrent uniq id
    auto const uniq_ids = get_selected_torrent_uniq_ids();
    // auto const key = get_details_dialog_key(uniq_ids);

    std::uint32_t unqid = uniq_ids.front();

    // auto& ses = get_session_handle();
    //accept const var use regular var 
    lt::torrent_handle handle_temp =  core_->find_torrent(unqid);
    if(!handle_temp.is_valid())
        return;

      
        
    //avoid open two Perperty Dialog For same torrenr
    auto dialog_it = details_.find(unqid);

    if (dialog_it == details_.end())
    {
        auto dialog = DetailsDialog::create(*wind_, core_, std::move(handle_temp));
        gtr_window_on_close(*dialog, [this, unqid]() { details_.erase(unqid); });
        // DetasDialogs already a field in Application::Impl,its lifetime is managed autoly in Application::Impl, no need to manually destruct it
        dialog_it = details_.try_emplace(unqid, std::move(dialog)).first;
        dialog_it->second->show();
    }
    //we may remove/delete torrent while its DetailsDialog is active, after remove, its DetailsDialog will also close
    
    gtr_window_present(dialog_it->second);
}



//ON SELECTION CHANGED
Application::Impl::counts_data Application::Impl::get_selected_torrent_counts() const
{
    counts_data counts;

    wind_->for_each_selected_torrent(
        [&counts](auto const& Tor)
        {
            ++counts.total_count;

            auto const activity = Tor->get_activity();

            if (activity == TR_STATUS_QUEUE_FOR_DOWNLOAD || activity == TR_STATUS_QUEUE_FOR_SEED)
            {
                ++counts.queued_count;
            }

            if (activity == TR_STATUS_PAUSED)
            {
                ++counts.stopped_count;
            }
        });

    return counts;
}



bool Application::Impl::refresh_actions()
{
    if (!is_closing_)
    {
        size_t const total = core_->get_torrent_count();
        size_t const active = core_->get_active_torrent_count();
        // auto const torrent_count = core_->get_model()->get_n_items(); torrent_count is equal to total, so comment this line

        auto const sel_counts = get_selected_torrent_counts();
        bool const has_selection = sel_counts.total_count > 0;

        gtr_action_set_sensitive("select-all", total != 0);
        gtr_action_set_sensitive("deselect-all", total != 0);
        gtr_action_set_sensitive("pause-all-torrents", active != 0);
        gtr_action_set_sensitive("start-all-torrents", active != total);

        gtr_action_set_sensitive("torrent-stop", (sel_counts.stopped_count < sel_counts.total_count));
        gtr_action_set_sensitive("torrent-start", (sel_counts.stopped_count) > 0);
        gtr_action_set_sensitive("torrent-start-now", (sel_counts.stopped_count + sel_counts.queued_count) > 0);
        gtr_action_set_sensitive("torrent-verify", has_selection);
        gtr_action_set_sensitive("remove-torrent", has_selection);
        gtr_action_set_sensitive("delete-torrent", has_selection);
        gtr_action_set_sensitive("relocate-torrent", has_selection);
        gtr_action_set_sensitive("queue-move-top", has_selection);
        gtr_action_set_sensitive("queue-move-up", has_selection);
        gtr_action_set_sensitive("queue-move-down", has_selection);
        gtr_action_set_sensitive("queue-move-bottom", has_selection);
        gtr_action_set_sensitive("show-torrent-properties", sel_counts.total_count == 1);
        gtr_action_set_sensitive("open-torrent-folder", sel_counts.total_count == 1);
        gtr_action_set_sensitive("copy-magnet-link-to-clipboard", sel_counts.total_count == 1);

        // bool const can_update = wind_ != nullptr &&
        //     wind_->for_each_selected_torrent_until([](auto const& torrent)
        //                                            { return tr_torrentCanManualUpdate(&torrent->get_underlying()); });
        gtr_action_set_sensitive("torrent-reannounce", sel_counts.total_count == 1);

        gtr_action_set_sensitive("scrape-last-working", sel_counts.total_count == 1);

    }

    refresh_actions_tag_.disconnect();
    return false;
}

void Application::Impl::refresh_actions_soon()
{
    if (!is_closing_ && !refresh_actions_tag_.connected())
    {
        refresh_actions_tag_ = Glib::signal_idle().connect(sigc::mem_fun(*this, &Impl::refresh_actions));
    }
}





namespace
{

bool has_magnet_link_handler()
{
    return bool{ Gio::AppInfo::get_default_for_uri_scheme("magnet") };
}

void register_magnet_link_handler()
{
    std::string const content_type = "x-scheme-handler/magnet";

    try
    {
        auto const app = Gio::AppInfo::create_from_commandline(
            "transmission-gtk",
            "transmission-gtk",
            TR_GIO_APP_INFO_CREATE_FLAGS(SUPPORTS_URIS));
        app->set_as_default_for_type(content_type);
    }
    catch (Gio::Error const& e)
    {
        gtr_warning(fmt::format(
            _("Couldn't register Transmission as a {content_type} handler: {error} ({error_code})"),
            fmt::arg("content_type", content_type),
            fmt::arg("error", e.what()),
            fmt::arg("error_code", static_cast<int>(e.code()))));
    }
}

void ensure_magnet_handler_exists()
{
    if (!has_magnet_link_handler())
    {
        register_magnet_link_handler();
    }
}

} // anonymous namespace

void Application::Impl::on_main_window_size_allocated()
{
    if (!core_) return; // Ensure core_ is valid

    // std::printf("Application::Impl::on_main_window_size_allocated core_ %p !\n", &(core_->get_session()));

    
#if GTKMM_CHECK_VERSION(4, 0, 0)
    bool const is_maximized = wind_->is_maximized();
#else
    auto const gdk_window = wind_->get_window();
    bool const is_maximized = gdk_window != nullptr && (gdk_window->get_state() & Gdk::WINDOW_STATE_MAXIMIZED) != 0;
#endif

    core_->set_pref(lt::settings_pack::TR_KEY_main_window_is_maximized, is_maximized);

    if (!is_maximized)
    {
#if !GTKMM_CHECK_VERSION(4, 0, 0)
        int x = 0;
        int y = 0;
        wind_->get_position(x, y);
        core_->set_pref(lt::settings_pack::TR_KEY_main_window_x, x);
        core_->set_pref(lt::settings_pack::TR_KEY_main_window_y, y);
#endif

        int w = 0;
        int h = 0;
#if GTKMM_CHECK_VERSION(4, 0, 0)
        wind_->get_default_size(w, h);
#else
        wind_->get_size(w, h);
#endif
        core_->set_pref(lt::settings_pack::TR_KEY_main_window_width, w);
        core_->set_pref(lt::settings_pack::TR_KEY_main_window_height, h);
    }
}





/***
****  signal handling
***/
namespace
{

#ifdef G_OS_UNIX

gboolean signal_handler(gpointer user_data)
{
    gtr_message(_("Got termination signal, trying to shut down cleanly. Do it again if it gets stuck."));
    gtr_actions_handler("quit", user_data);
    return G_SOURCE_REMOVE;
}

#endif

} //anonumois namespace




void Application::on_startup()
{
    Gtk::Application::on_startup();
    impl_->on_startup();
}

void Application::Impl::on_startup()
{
    IF_GTKMM4(Gtk::IconTheme::get_for_display(Gdk::Display::get_default()), Gtk::IconTheme::get_default())
        ->add_resource_path(gtr_get_full_resource_path("icons"s));
    Gtk::Window::set_default_icon_name(std::string(AppIconName));

    /* Add style provider to the window. */
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_resource(gtr_get_full_resource_path("transmission-ui.css"));
    Gtk::StyleContext::IF_GTKMM4(add_provider_for_display, add_provider_for_screen)(
        IF_GTKMM4(Gdk::Display::get_default(), Gdk::Screen::get_default()),
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    FilterBar();
    PathButton();


#ifdef G_OS_UNIX
    g_unix_signal_add(SIGINT, &signal_handler, this);
    g_unix_signal_add(SIGTERM, &signal_handler, this);
#endif

        
    /*
        initialize the libtorrent session, 
            -- if had session.sett, load it to our session obj
            -- if dont have, still load default settings,use same function
    */
    /* logging */
    tr_logAddInfo(_("Loading session settings "));
    // lt::session_params loaded_params = tr_sessionLoadSettings();
    // lt::session ses(loaded_params);

    
            // std::printf("In Application::on_startup ses %p !\n", &ses_);

    //init core_
    core_ = Session::create(std::move(tr_sessionLoadSettings()));
 

            std::printf("In Application::on_startup core_ %p !\n", &(core_->get_session()));

    /* ensure the directories are created */
    if (auto const str = core_->get_sett().get_str(lt::settings_pack::TR_KEY_download_dir); !str.empty())
    {
        (void)g_mkdir_with_parents(str.c_str(), 0777);
    }


//**********************************************************************/


    /* init the ui manager */
    ui_builder_ = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("transmission-ui.xml"s));
    auto const actions = gtr_actions_init(ui_builder_, this, core_->get_sett());

    auto const main_menu = gtr_action_get_object<Gio::Menu>("main-window-menu");
    app_.set_menubar(main_menu);

    /* create main window now to be a parent to any error dialogs */
    wind_ = MainWindow::create(app_, actions, core_);
    wind_->set_show_menubar(true);
#if GTKMM_CHECK_VERSION(4, 0, 0)
    wind_->property_maximized().signal_changed().connect(sigc::mem_fun(*this, &Impl::on_main_window_size_allocated));
    wind_->property_default_width().signal_changed().connect(sigc::mem_fun(*this, &Impl::on_main_window_size_allocated));
    wind_->property_default_height().signal_changed().connect(sigc::mem_fun(*this, &Impl::on_main_window_size_allocated));
#else
    wind_->signal_size_allocate().connect(sigc::hide<0>(sigc::mem_fun(*this, &Impl::on_main_window_size_allocated)));
#endif

#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto const shortcut_controller = Gtk::ShortcutController::create(gtr_shortcuts_get_from_menu(main_menu));
    shortcut_controller->set_scope(Gtk::ShortcutScope::GLOBAL);
    wind_->add_controller(shortcut_controller);
#endif

    app_.hold();
    app_setup();
    
    /* if there's no magnet link handler registered, register us */
    ensure_magnet_handler_exists();
}

void Application::on_activate()
{
    Gtk::Application::on_activate();

    impl_->on_activate();
}

void Application::Impl::on_activate()
{
    activation_count_++;

    /* GApplication emits an 'activate' signal when bootstrapping the primary.
     * Ordinarily we handle that by presenting the main window, but if the user
     * started Transmission minimized, ignore that initial signal... */
    if (/*start_iconified_ && */activation_count_ == 1)
    {
        return;
    }

    gtr_action_activate("present-main-window");
}



//wrapper
lt::session const& Application::Impl::get_session_handle() const
{
    return core_->get_session();
}




//wrapper 
/**
 * @brief get selected torrent(s) .aka row(s) 
 */
std::vector<lt::torrent_status> Application::Impl::get_torrent_status() const
{
    std::vector<lt::torrent_status> ret;

    std::vector<std::uint32_t> unq_ids;
    wind_->for_each_selected_torrent([&unq_ids](auto const& Torr) 
    { 
        unq_ids.push_back(Torr->get_uniq_id());        
    });

    // auto& ses = get_session_handle();

    for(auto& i : unq_ids)
    {
        auto st = core_->find_torrent_status(i);
        if(!st.handle.is_valid())
            continue;
        ret.push_back(st);
    }

    return ret;
}





[[nodiscard]] std::vector<std::uint32_t> Application::Impl::get_selected_torrent_uniq_ids() const
{
    std::vector<std::uint32_t> uniq_ids;

    wind_->for_each_selected_torrent([&uniq_ids](auto const& torrent) { uniq_ids.push_back(torrent->get_uniq_id()); });

    return uniq_ids;
}



std::vector<lt::torrent_status> Application::Impl::get_selected_torrent_status()
{
    std::vector<lt::torrent_status> status;

    wind_->for_each_selected_torrent([&status](auto const& torrent) { status.push_back(torrent->get_status_ref()); });
    return status;
}



std::vector<lt::torrent_handle> Application::Impl::get_selected_torrent_handle() const
{
    std::vector<lt::torrent_handle> handles;

    wind_->for_each_selected_torrent([&handles](auto const& torrent) { handles.push_back(torrent->get_handle_ref()); });
    return handles;
}



/*
    for open gtk-client from command-line and pass in torrent file as argv
    this will open add torrent option dialog
*/
void Application::Impl::open_files(std::vector<Glib::RefPtr<Gio::File>> const& files)
{
    // bool do_start = {};
    bool do_prompt = {};
 
    if(!core_)
    {
        // do_start = !start_paused_;
        do_prompt = true;
    }
    else
    {
        // do_start = (core_->get_sett().get_bool(settings_pack::TR_KEY_start_added_torrents) ) && !start_paused_;
        do_prompt = core_->get_sett().get_bool(settings_pack::TR_KEY_show_options_window);
    }
 

    core_->add_files(files, do_prompt);
}

void Application::on_open(std::vector<Glib::RefPtr<Gio::File>> const& files, Glib::ustring const& hint)
{
    Gtk::Application::on_open(files, hint);

    impl_->open_files(files);
}



namespace
{

std::string get_application_id(std::string const& config_dir)
{
    struct ::stat sb{};
    (void) ::stat(config_dir.c_str(), &sb);
    return fmt::format("com.transmissionbt.transmission_{}_{}", sb.st_dev, sb.st_ino);
}

} //anonymous namespace


Application::Application(std::string const& config_dir, bool start_paused)
    : Gtk::Application(get_application_id(config_dir), TR_GIO_APPLICATION_FLAGS(HANDLES_OPEN))
    , impl_(std::make_unique<Impl>(*this, config_dir, start_paused))
{
    
}


Application::~Application() = default;
// {
//     std::printf("Application::~Application(\n");
// }

// Application::Impl::~Impl()
// {
//     std::printf("In Application::Impl Dtor!\n");
//     if(ses_.is_valid())
//     {
//         std::printf("In Application::Impl Dtor, ses is valid!\n");
//     }
//     else
//     {
//         std::printf("In Application::Impl Dtor, ses is NOT valid!\n");
//     }
// }



Application::Impl::Impl(Application& app, std::string const& config_dir, bool start_paused)
    : app_(app)
    , config_dir_(config_dir)
    , start_paused_(start_paused)
    // , ses_(tr_sessionLoadSettings())
{
    
}


void Application::Impl::on_core_busy(bool busy)
{
    wind_->set_busy(busy);
}

void Application::Impl::app_setup()
{
    // if (start_iconified_)
    // {
    //     core_->set_pref(settings_pack::TR_KEY_show_notification_area_icon, true);
    // }

    gtr_actions_set_core(core_);

    /* set up core handlers */
    core_->signal_busy().connect(sigc::mem_fun(*this, &Impl::on_core_busy));
    core_->signal_add_error().connect(sigc::mem_fun(*this, &Impl::on_core_error));
    core_->signal_add_prompt().connect(sigc::mem_fun(*this, &Impl::on_add_torrent));
    core_->signal_prefs_changed().connect(sigc::mem_fun(*this, &Impl::on_prefs_changed));
    // core_->signal_close_application().connect(sigc::mem_fun(*this, &Impl::on_close_application));


    //load previous torrents tasks from local settings serialized file
    
                std::cout << "Load Previous Torrents from Resume files " << std::endl;

    core_->load(start_paused_);
    core_->torrents_added();


    /* set up main window */
    main_window_setup();



    /* start model update timer */
    timer_ = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &Impl::update_model_loop),
        MAIN_WINDOW_REFRESH_INTERVAL_SECONDS);
    update_model_once();


    /* either show the window or iconify it */
    // if (!start_iconified_)
    // {
        wind_->show();
        gtr_action_set_toggled("toggle-main-window", true);
    // }
    // else
    // {
    //     gtr_window_set_skip_taskbar_hint(*wind_, icon_ != nullptr);
    //     gtr_action_set_toggled("toggle-main-window", false);
    // }

    if (!core_->get_sett().get_bool(settings_pack::TR_KEY_user_has_given_informed_consent))
    {
        auto w = std::make_shared<Gtk::MessageDialog>(
            *wind_,
            _("Transmission is a file sharing program. When you run a torrent, its data will be "
              "made available to others by means of upload. Any content you share is your sole responsibility."),
            false,
            TR_GTK_MESSAGE_TYPE(OTHER),
            TR_GTK_BUTTONS_TYPE(NONE),
            true);
        w->add_button(_("_Cancel"), TR_GTK_RESPONSE_TYPE(REJECT));
        w->add_button(_("I _Agree"), TR_GTK_RESPONSE_TYPE(ACCEPT));
        w->set_default_response(TR_GTK_RESPONSE_TYPE(ACCEPT));
        w->signal_response().connect(
            [w, this](int response) mutable
            {
                if (response == TR_GTK_RESPONSE_TYPE(ACCEPT))
                {
                    // only show it once
                    core_->set_pref(lt::settings_pack::TR_KEY_user_has_given_informed_consent, true);
                    w.reset();
                }
                else
                {
                    exit(0);
                }
            });
        w->show();
    }
}

void Application::Impl::placeWindowFromPrefs()
{
#if GTKMM_CHECK_VERSION(4, 0, 0)
    wind_->set_default_size((int)gtr_pref_int_get(TR_KEY_main_window_width), (int)gtr_pref_int_get(TR_KEY_main_window_height));
#else

    int main_win_wid = {};
    int main_win_hei = {};
    int main_win_x = {};
    int main_win_y = {};

    if(!core_)
    {
        main_win_wid = 300;
        main_win_hei = 500;
        main_win_x = 50;
        main_win_y = 50;
    }
    else
    {
        auto sett = core_->get_sett();
        main_win_wid =  (int)sett.get_int(settings_pack::TR_KEY_main_window_width);
        main_win_hei =  (int)sett.get_int(settings_pack::TR_KEY_main_window_height);
        main_win_x =    (int)sett.get_int(settings_pack::TR_KEY_main_window_x);
        main_win_y =    (int)sett.get_int(settings_pack::TR_KEY_main_window_y);
    }
    wind_->resize(main_win_wid, main_win_hei);
    wind_->move(main_win_x, main_win_y);
#endif
}

void Application::Impl::presentMainWindow()
{
    gtr_action_set_toggled("toggle-main-window", true);

    if (is_iconified_)
    {
        is_iconified_ = false;

        gtr_window_set_skip_taskbar_hint(*wind_, false);
    }

    if (!wind_->get_visible())
    {
        placeWindowFromPrefs();
        gtr_widget_set_visible(*wind_, true);
    }

    gtr_window_present(wind_);
    gtr_window_raise(*wind_);
}

void Application::Impl::hideMainWindow()
{
    gtr_action_set_toggled("toggle-main-window", false);

    gtr_window_set_skip_taskbar_hint(*wind_, true);
    gtr_widget_set_visible(*wind_, false);
    is_iconified_ = true;
}

void Application::Impl::toggleMainWindow()
{
    if (is_iconified_)
    {
        presentMainWindow();
    }
    else
    {
        hideMainWindow();
    }
}

bool Application::Impl::winclose()
{

        std::cout << "In Application::Impl::winclose() for MainWindow" << std::endl;
    // if (icon_ != nullptr)
    // {
    //     gtr_action_activate("toggle-main-window");
    // }
    // else
    // {

        on_app_exit();

    // }

    return true; /* don't propagate event further */
}

void Application::Impl::rowChangedCB(std::unordered_set<std::uint32_t> const& uniq_ids, Torrent::ChangeFlags changes)
{
    if (changes.test(Torrent::ChangeFlag::TORRENT_STATE) &&
        wind_->for_each_selected_torrent_until([&uniq_ids](auto const& torrent)
                                               { return uniq_ids.find(torrent->get_uniq_id()) != uniq_ids.end(); }))
    {
        refresh_actions_soon();
    }
}

#if GTKMM_CHECK_VERSION(4, 0, 0)

bool Application::Impl::on_drag_data_received(Glib::ValueBase const& value, double /*x*/, double /*y*/)
{
    if (G_VALUE_HOLDS(value.gobj(), GDK_TYPE_FILE_LIST))
    {
        FileListValue files_value;
        files_value.init(value.gobj());
        open_files(FileListHandler::slist_to_vector(files_value.get(), Glib::OwnershipType::OWNERSHIP_NONE));
        return true;
    }

    if (G_VALUE_HOLDS(value.gobj(), StringValue::value_type()))
    {
        StringValue string_value;
        string_value.init(value.gobj());
        if (auto const text = gtr_str_strip(string_value.get()); !text.empty())
        {
            return core_->add_from_url(text);
        }
    }

    return false;
}

#else

void Application::Impl::on_drag_data_received(
    Glib::RefPtr<Gdk::DragContext> const& drag_context,
    gint /*x*/,
    gint /*y*/,
    Gtk::SelectionData const& selection_data,
    guint /*info*/,
    guint time_)
{
    if (auto const uris = selection_data.get_uris(); !uris.empty())
    {
        auto files = std::vector<Glib::RefPtr<Gio::File>>();
        files.reserve(uris.size());
        std::transform(uris.begin(), uris.end(), std::back_inserter(files), &Gio::File::create_for_uri);

        open_files(files);
    }
    else
    {
        auto const text = gtr_str_strip(selection_data.get_text());

        if (!text.empty())
        {
            core_->add_from_url(text);
        }
    }

    drag_context->drag_finish(true, false, time_);
}

#endif

void Application::Impl::main_window_setup()
{
    wind_->signal_selection_changed().connect(sigc::mem_fun(*this, &Impl::refresh_actions_soon));
    refresh_actions_soon();
    core_->signal_torrents_changed().connect(sigc::mem_fun(*this, &Impl::rowChangedCB));
    //register for MainWindow
    gtr_window_on_close(*wind_, sigc::mem_fun(*this, &Impl::winclose));
    refresh_actions();

    /* register to handle URIs that get dragged onto our main window */
#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto drop_controller = Gtk::DropTarget::create(G_TYPE_INVALID, Gdk::DragAction::COPY);
    drop_controller->set_gtypes({ StringValue::value_type(), GDK_TYPE_FILE_LIST });
    drop_controller->signal_drop().connect(sigc::mem_fun(*this, &Impl::on_drag_data_received), false);
    wind_->add_controller(drop_controller);
#else
    wind_->drag_dest_set(Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
    wind_->drag_dest_add_uri_targets();
    wind_->drag_dest_add_text_targets(); /* links dragged from browsers are text */
    wind_->signal_drag_data_received().connect(sigc::mem_fun(*this, &Impl::on_drag_data_received));
#endif
}







/*****************
****CLOSE GUI*****
******************/
bool Application::Impl::on_session_closed()
{
    std::printf("Application::Impl::on_session_closed ...\n");

    details_.clear();

    open_tor_options_.clear();
    
    prefs_.reset(); 

    if (msgwin_ != nullptr)
    {
        msgwin_.reset();
    }

    wind_.reset();


    //clear various Dialogs
    for (auto& [key, ptr] : active_dialogs_) 
    {
        std::string const& keyName = getPeriphralDialogName(key);
        std::cout << "on_session_closed: Destruct " << keyName << std::endl;

        //manage referencec count
        ptr.reset();
    }
    active_dialogs_.clear();


    /*Destruct Glib::RefPtr<Session>*/
            std::printf("\n...unreferencing Session object \n");
    core_->unreference();
    // core_.reset();
    // icon_.reset();

    error_list_.clear();

    duplicates_list_.clear();

    // app_.release();  //use app_.quit instead
    app_.quit();
    return false;
}


void Application::Impl::on_app_exit()
{
    std::printf("Application::Impl::on_app_exit start\n");

    if (is_closing_)
    {
        return;
    }

    is_closing_ = true;

    refresh_actions_tag_.disconnect();
    update_model_soon_tag_.disconnect();
    timer_.disconnect();

#if !GTKMM_CHECK_VERSION(4, 0, 0)
    wind_->remove();
#endif

    wind_->set_show_menubar(false);

    auto* p = Gtk::make_managed<Gtk::Grid>();
    p->set_column_spacing(GUI_PAD_BIG);
    p->set_halign(TR_GTK_ALIGN(CENTER));
    p->set_valign(TR_GTK_ALIGN(CENTER));
#if GTKMM_CHECK_VERSION(4, 0, 0)
    wind_->set_child(*p);
#else
    wind_->add(*p);
#endif

    auto* icon = Gtk::make_managed<Gtk::Image>();
    icon->property_icon_name() = "network-workgroup";
    icon->property_icon_size() = IF_GTKMM4(Gtk::IconSize::LARGE, Gtk::ICON_SIZE_DIALOG);
    p->attach(*icon, 0, 0, 1, 2);

    auto* top_label = Gtk::make_managed<Gtk::Label>();
    top_label->set_markup(fmt::format("<b>{:s}</b>", _("Closing Connections…")));
    top_label->set_halign(TR_GTK_ALIGN(START));
    top_label->set_valign(TR_GTK_ALIGN(CENTER));
    p->attach(*top_label, 1, 0, 1, 1);

    auto* bottom_label = Gtk::make_managed<Gtk::Label>(_("Sending upload/download totals to tracker…"));
    bottom_label->set_halign(TR_GTK_ALIGN(START));
    bottom_label->set_valign(TR_GTK_ALIGN(CENTER));
    p->attach(*bottom_label, 1, 1, 1, 1);


    //Abnormal close, destructor not called
    //NOT USED FORCE CLOSE, CUZ session CLOSE Is TIED TO Session Destruct , NOT SURE
    auto* button = Gtk::make_managed<Gtk::Button>(_("_Quit Now"), true);
    button->set_margin_top(GUI_PAD);
    button->set_halign(TR_GTK_ALIGN(START));
    button->set_valign(TR_GTK_ALIGN(END));
    button->signal_clicked().connect([]() { ::exit(0); });
    p->attach(*button, 1, 2, 1, 1);



#if !GTKMM_CHECK_VERSION(4, 0, 0)
    p->show_all();
#endif
    // button->grab_focus();

    /* clear the UI */
    core_->clear();
    
    /* ensure the window is in its previous position & size.
     * this seems to be necessary because changing the main window's
     * child seems to unset the size */
    placeWindowFromPrefs();



    // /*retrieve session obj which we give ownership to Session before from Session */
    // on_session_closed();


    /* shut down libtorrent */
    /* since tr_sessionClose () is a blocking function,
     * delegate its call to another thread here... when it's done,
     * punt the GUI teardown back to the GTK+ thread */
    // core_.release();



    std::thread(
        [this]()
        {
            
                        std::cout << "enter new thread core_->close();" << std::endl;
            core_->close();
                        std::cout << "Destruct the GUI..." << std::endl;

            Glib::signal_idle().connect(sigc::mem_fun(*this, &Impl::on_session_closed));

        })
        .detach();
}




/******************
 ****ERR Dialog****
 ******************/
void Application::Impl::show_torrent_errors(Glib::ustring const& primary, std::vector<std::string>& files)
{
    std::ostringstream s;
    auto const leader = files.size() > 1 ? gtr_get_unicode_string(GtrUnicode::Bullet) : "";

    for (auto const& f : files)
    {
        s << leader << ' ' << f << '\n';
    }

    auto w = std::make_shared<Gtk::MessageDialog>(
        *wind_,
        primary,
        false,
        TR_GTK_MESSAGE_TYPE(ERROR),
        TR_GTK_BUTTONS_TYPE(CLOSE),
        true);
    w->set_secondary_text(s.str());
    w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
    w->show();

    files.clear();
}

void Application::Impl::flush_torrent_errors()
{
    if (!error_list_.empty())
    {
        show_torrent_errors(
            ngettext("Couldn't add corrupt torrent", "Couldn't add corrupt torrents", error_list_.size()),
            error_list_);
    }

    if (!duplicates_list_.empty())
    {
        show_torrent_errors(
            ngettext("Couldn't add duplicate torrent", "Couldn't add duplicate torrents", duplicates_list_.size()),
            duplicates_list_);
    }
}

void Application::Impl::on_core_error(Session::ErrorCode code, Glib::ustring const& msg)
{
    switch (code)
    {
        //add corrupted for current torrent 
        case Session::ERR_ADD_TORRENT_ERR:
                        std::cout << "case Session::ERR_ADD_TORRENT_ERR: " << msg.raw() << std::endl;
            error_list_.push_back(Glib::path_get_basename(msg.raw()));
            break;

        case Session::ERR_ADD_TORRENT_DUP:
            duplicates_list_.push_back(msg);
            break;

        //flush torrent add errors only when the last finishes, not that flush when every torernt added 
        case Session::ERR_NO_MORE_TORRENTS:
            flush_torrent_errors();
            break;

        default:
            g_assert_not_reached();
            break;
    }
}





void Application::Impl::on_main_window_focus_in()
{
    if (wind_ != nullptr)
    {
        gtr_window_set_urgency_hint(*wind_, false);
    }
}

// this method shows up Options Dialog when add torrent or after making torrent
void Application::Impl::on_add_torrent(lt::add_torrent_params atp ,char const* dottor_filename, char const* download_dir)
{
    lt::info_hash_t ih_copy_tmp = atp.info_hashes;


    auto options_it = open_tor_options_.find(ih_copy_tmp);

    if(options_it == open_tor_options_.end())
    {
        /*OptionsDialog, support open multiple at the same time*/
        auto dialog = /*std::shared_ptr<OptionsDialog>(*/
            OptionsDialog::create(*wind_, core_, std::move(atp), std::string(dottor_filename), std::string(download_dir))/*)*/;


        gtr_window_on_close(*dialog, [this, ih_copy_tmp]() { 
                            std::cout << "Destruct OptionsDialog on handler" << std::endl;
            open_tor_options_.erase(ih_copy_tmp); 
        });

        // OptionsDialog already a field in Application::Impl,its lifetime is managed autoly in Application::Impl, no need to manually destruct it
        options_it = open_tor_options_.try_emplace(ih_copy_tmp, std::move(dialog)).first;
   

        #if GTKMM_CHECK_VERSION(4, 0, 0)
            auto focus_controller = Gtk::EventControllerFocus::create();
            focus_controller->signal_enter().connect(sigc::mem_fun(*this, &Impl::on_main_window_focus_in));
            w->add_controller(focus_controller);
        #else
            options_it->second->signal_focus_in_event().connect_notify(sigc::hide<0>(sigc::mem_fun(*this, &Impl::on_main_window_focus_in)));
        #endif

            if (wind_ != nullptr)
            {
                gtr_window_set_urgency_hint(*wind_, true);
            }

            options_it->second->show();

    }




}


// void Application::Impl::on_close_application()
// {

// }


// listen when gtk client related settings changed
void Application::Impl::on_prefs_changed(int const key)
{
    auto& ses = core_-> get_session();
    auto sett = core_-> get_sett();

    switch (key)
    {
        case settings_pack::TR_KEY_message_level:
            tr_logSetLevel(static_cast<tr_log_level>(sett.get_int(key)));
            break;

        // case settings_pack::TR_KEY_show_options_window:
        //     tr_logSetLevel(static_cast<tr_log_level>(sett.get_int(key)));
        //     break;

        // case settings_pack::TR_KEY_download_dir:
        //         std::cout <<" on prefs changed Application:: TR_KEY_Download_d9r " << std::endl;
        //     break;

        default:
            break;
    }
}



/*******Update******/
bool Application::Impl::update_model_once()
{
    /* update the torrent data in the model */
    core_->update();

    /* refresh the main window's statusbar and toolbar buttons */
    if (wind_ != nullptr)
    {
        wind_->refresh();
    }

    /* update the actions */
    refresh_actions();

    /* update the status tray icon */
    // if (icon_ != nullptr)
    // {
    //     icon_->refresh();
    // }

    update_model_soon_tag_.disconnect();
    return false;
}

void Application::Impl::update_model_soon()
{
    if (!update_model_soon_tag_.connected())
    {
        update_model_soon_tag_ = Glib::signal_idle().connect(sigc::mem_fun(*this, &Impl::update_model_once));
    }
}

/************update torernt**************/
bool Application::Impl::update_model_loop()
{
    if (!is_closing_)
    {
        update_model_once();
    }

    return !is_closing_;
}





void Application::Impl::show_about_dialog()
{
    auto const uri = Glib::ustring("https://transmissionbt.com/");
    auto const authors = std::vector<Glib::ustring>({
        "Charles Kerr (Backend; GTK+)",
        "Mitchell Livingston (Backend; macOS)",
        "Mike Gelfand",
    });

    auto d = std::make_shared<Gtk::AboutDialog>();
    d->set_authors(authors);
    d->set_comments(_("A fast and easy BitTorrent client"));
    d->set_copyright(_("Copyright © The Transmission Project"));
    d->set_logo_icon_name(std::string(AppIconName));
    d->set_name(Glib::get_application_name());
    /* Translators: translate "translator-credits" as your name
       to have it appear in the credits in the "About"
       dialog */
    d->set_translator_credits(_("translator-credits"));
    d->set_version(LONG_VERSION_STRING);
    d->set_website(uri);
    d->set_website_label(uri);
#ifdef SHOW_LICENSE
    d->set_license(LICENSE);
    d->set_wrap_license(true);
#endif
    // the dialog is tied to the parent window (wind_ in this case). When the parent window is closed, the modal dialog is also closed.
    d->set_transient_for(*wind_);
    //modal dialog
    d->set_modal(true);


    gtr_window_on_close(*d, [d,this]() mutable { 
        //if it closed in handler, remove from active_dialogs_
        d.reset(); 

    });

#if !GTKMM_CHECK_VERSION(4, 0, 0)
    d->signal_response().connect_notify([&dref = *d](int /*response*/) { dref.close(); });
#endif
    d->show();
}


namespace
{


//strat or stop
void toggle_auto_managed(lt::torrent_status const& ts)
{
    auto& h = ts.handle;
    if(h.is_valid() == false)
    {
        return;
    }
    //resume ||
    //whether it is (paused AND auto_managed) or  (paused And !auto_managed), set auto_managed 
    if ((ts.flags & (lt::torrent_flags::auto_managed| lt::torrent_flags::paused)) 
                == lt::torrent_flags::paused)
    {
        //open auto_managed
        h.set_flags(lt::torrent_flags::auto_managed);
    }
    //pause !>
    else
    {
        //pause a torrent need to disable auto_managed firstly, to avoid resume automatically
        h.unset_flags(lt::torrent_flags::auto_managed);
        h.pause(torrent_handle::graceful_pause);
    }
}


//only force-strat, no force-stop
void force_start_cancel_automanaged(lt::torrent_status const& ts)
{
    auto& h = ts.handle;
    if(h.is_valid() == false)
    {
        return;
    }

    //toggle auto_managed
    h.set_flags(~(ts.flags & lt::torrent_flags::auto_managed),
        lt::torrent_flags::auto_managed);
    //if torrent not in pause mode,no need to call resume()
    if ((ts.flags & lt::torrent_flags::auto_managed)
        && (ts.flags & lt::torrent_flags::paused))
    {
                                
        h.resume();
    }
}


}//anonymous namespace




// remove or trash selected torrent
void Application::Impl::remove_selected(bool delete_files)
{
    if (std::vector<lt::torrent_status> st = get_selected_torrent_status(); !st.empty())
    {
        gtr_confirm_remove(*wind_, core_, st, delete_files);
    }
}




// no need to check which row is selected or not
void Application::Impl::start_all_torrents()
{
    auto status = get_torrent_status();    
    std::sort(std::begin(status), std::end(status), [](auto& lhs, auto& rhs){
        return lhs.queue_position < rhs.queue_position;
    });

    for(auto const& s : status)
    {
        if(s.handle.is_valid())
        {
            toggle_auto_managed(s);
        }
    }
}




// no need to check which torrent is selected
void Application::Impl::pause_all_torrents()
{
    std::vector<lt::torrent_status> status = get_torrent_status();    
    std::sort(std::begin(status), std::end(status), [](auto& lhs, auto& rhs){
        return lhs.queue_position < rhs.queue_position;
    });

    for(lt::torrent_status const&  s : status)
    {
        if(s.handle.is_valid())
        {
            toggle_auto_managed(s);
        }
    }
}





bool Application::Impl::force_start_torrents()
{
    bool changed = {};
    std::vector<lt::torrent_status> status = get_selected_torrent_status();    
    std::sort(std::begin(status), std::end(status), [](auto& lhs, auto& rhs){
        return lhs.queue_position < rhs.queue_position;
    });

    for(lt::torrent_status const& s : status)
    {
        if(s.handle.is_valid())
        {   
            if(changed == false) changed = true;
            force_start_cancel_automanaged(s);
        }
    } 

    return changed;
}


bool Application::Impl::start_torrents_auto_managed()
{
    bool changed = {};
    std::vector<lt::torrent_status> status = get_selected_torrent_status();    
    std::sort(std::begin(status), std::end(status), [](auto& lhs, auto& rhs){
        return lhs.queue_position < rhs.queue_position;
    });

    for(lt::torrent_status const& s : status)
    {
        if(s.handle.is_valid())
        {
            if(changed == false) changed = true;
            toggle_auto_managed(s);
        }
    } 
    return changed;
}


bool Application::Impl::stop_torrents_auto_managed()
{
    bool changed = {};
     std::vector<lt::torrent_status> status = get_selected_torrent_status();    
    std::sort(std::begin(status), std::end(status), [](auto& lhs, auto& rhs)
    {
        return lhs.queue_position < rhs.queue_position;
    });

    for(lt::torrent_status& s : status)
    {
        if(s.handle.is_valid())
        {
            if(changed == false) changed = true;
            toggle_auto_managed(s);
        }
    } 
    return changed;
}




void Application::Impl::copy_magnet_link_to_clipboard(Glib::RefPtr<Torrent> const& torrent) const
{
    // auto const magnet = tr_torrentGetMagnetLink(&torrent->get_underlying());
    std::string const& magnet = torrent->fetchMagnetLink();
    if(magnet.empty())
    {
        ;
    }

    auto const display = wind_->get_display();

    /* this is The Right Thing for copy/paste... */
    IF_GTKMM4(display->get_clipboard(), Gtk::Clipboard::get_for_display(display, GDK_SELECTION_CLIPBOARD))->set_text(magnet);

    /* ...but people using plain ol' X need this instead */
    IF_GTKMM4(display->get_primary_clipboard(), Gtk::Clipboard::get_for_display(display, GDK_SELECTION_PRIMARY))
        ->set_text(magnet);
}



bool Application::Impl::do_force_recheck()
{
    bool changed = {};
    std::vector<lt::torrent_handle> handles = get_selected_torrent_handle();
    for(lt::torrent_handle& h : handles)
    {
        if(h.is_valid())
        {
            if(changed ==false) changed = true;
            h.force_recheck();
        }
    }
    return changed;
}


bool Application::Impl::do_force_reannounce()
{
    bool changed = {};
    std::vector<lt::torrent_handle> handles = get_selected_torrent_handle();
    for(lt::torrent_handle& h : handles)
    {
        if(h.is_valid())
        {
            if(changed ==false) changed = true;
            h.force_reannounce();
        }
    }
    return changed;
}


bool Application::Impl::do_scrape_last_working()
{
    bool changed = {};
    std::vector<lt::torrent_handle> handles = get_selected_torrent_handle();
    for(lt::torrent_handle& h : handles)
    {
        if(h.is_valid())
        {
            if(changed ==false) changed = true;
            //scrape the last working tracker
            h.scrape_tracker();
        }
    }
    return changed;

}


bool Application::Impl::alter_queue_position(alter_queue_pos_mode_t mode)
{
    bool changed = {};
    std::vector<lt::torrent_handle> handles = get_selected_torrent_handle();
    for(lt::torrent_handle& h : handles)
    {
        if(h.is_valid())
        {
            if(changed ==false) changed = true;
            switch(mode)
            {
                case alter_queue_pos_up:
                    h.queue_position_up();
                    break;
                case alter_queue_pos_down:
                    h.queue_position_down();
                    break;
                case alter_queue_pos_top:
                    h.queue_position_top();
                    break;  
                case alter_queue_pos_bottom:
                    h.queue_position_bottom();
                    break;
                default:
                    break;
            }
        }
    }
    return changed;
}


void gtr_actions_handler(Glib::ustring const& action_name, gpointer user_data)
{
    static_cast<Application::Impl*>(user_data)->actions_handler(action_name);
}


void Application::Impl::actions_handler(Glib::ustring const& action_name)
{
    bool changed = false;

    /*add torrent from magnet link*/
    if (action_name == "open-torrent-from-url")
    {
        auto dialog = std::shared_ptr<TorrentUrlChooserDialog>(TorrentUrlChooserDialog::create(*wind_, core_));

        //its owneris `active_dialogs_` field, so it kept alive even though go out of this else-if scope
        active_dialogs_[PeriphralDialogEnum::TorrentUrlChooserDialog] = dialog;

        // Capture as weak_ptr, dont need to worry the reference count that it may not destructed successfully
        std::weak_ptr<TorrentUrlChooserDialog> weak_dialog = dialog; 

  
        // when you open multiple this Dialog, the original (shared_ptr) reference count 
        // will drop to zero and its destrutor be called, 
        // so we cannot (no need to) open multiple identical Dialogs  
        gtr_window_on_close(*dialog, [weak_dialog,this]() mutable { 
            // Check if dialog is still alive
            if (auto locked_dialog = weak_dialog.lock()) 
            { 
                active_dialogs_.erase(PeriphralDialogEnum::TorrentUrlChooserDialog);
                std::cout << "Destruct TorrentUrlChooserDialog on handler" <<std::endl;
                // Decrease the reference count
                locked_dialog.reset(); 
            }
        });

        dialog->show();
    }
    /*add torrent from torrent-file*/
    else if (action_name == "open-torrent")
    {
        auto w = std::shared_ptr<TorrentFileChooserDialog>(TorrentFileChooserDialog::create(*wind_, core_));

        w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
        w->show();
    }

    /*pause all torrent in our session*/
    else if (action_name == "pause-all-torrents")
    {
        pause_all_torrents();
    }
    /*start all torrents in out session*/
    else if (action_name == "start-all-torrents")
    {
        start_all_torrents();
    }

    /*Copy Magnet-Link to ClipBoard*/
    else if (action_name == "copy-magnet-link-to-clipboard")
    {
        wind_->for_each_selected_torrent_until(
            sigc::bind_return(sigc::mem_fun(*this, &Impl::copy_magnet_link_to_clipboard), true));
    }


    /*start selected torrent [auto-managed]*/
    else if(action_name == "torrent-start")
    {
        changed = start_torrents_auto_managed();
    }
    /*force-start selected torrent [regardless of auto-manage mechanism]*/
    else if(action_name == "torrent-start-now")
    {
        changed = force_start_torrents();
    }
    /*stop selected torrent [aotumanaged]*/
    else if(action_name == "torrent-stop")
    {
        changed = stop_torrents_auto_managed();
    }
    /*reannouce to trackers*/
    else if(action_name == "torrent-reannounce")
    {
        changed = do_force_reannounce();
    
    }

    /*scrape to the last working tracker, NOT all trackers*/
    else if(action_name == "scrape-last-working")
    {
        changed = do_scrape_last_working();
    }

    /*Verifu local data*/
    else if(action_name == "torrent-verify")
    {
        changed = do_force_recheck();

    }

    else if(action_name == "queue-move-up")
    {
        changed = alter_queue_position(alter_queue_pos_mode_t::alter_queue_pos_up);
    }
    else if(action_name == "queue-move-down")
    {
        changed = alter_queue_position(alter_queue_pos_mode_t::alter_queue_pos_down);
    }
    else if(action_name == "queue-move-top")
    {
        changed = alter_queue_position(alter_queue_pos_mode_t::alter_queue_pos_top);
    }
    else if(action_name == "queue-move-bottom")
    {
        changed = alter_queue_position(alter_queue_pos_mode_t::alter_queue_pos_bottom);
    }

    /*open save-path folder*/
    else if (action_name == "open-torrent-folder")
    {
        wind_->for_each_selected_torrent([this](auto const& torrent) { core_->open_folder(torrent->get_uniq_id()); });
    }


    /*DetailsDialog*/
    else if (action_name == "show-torrent-properties")
    {
        show_details_dialog_for_selected_torrents();
    }


    /*PrefsDialog*/
    else if (action_name == "edit-preferences")
    {
        if (prefs_ == nullptr)
        {
            //prefsDialog already a field in Application::Impl,its lifetime is managed autoly in Application::Impl, no need to manually destruct it
            prefs_ = PrefsDialog::create(*wind_, core_);

            gtr_window_on_close(*prefs_, [this]() 
            { 
                                std::cout << "Destruct PrefsDialog on handler " << std::endl;
                prefs_.reset(); 

            });
        }

        gtr_window_present(prefs_);
    }

    /*MessageLogWindow*/
    else if (action_name == "toggle-message-log")
    {
        if (msgwin_ == nullptr)
        {
            msgwin_ = MessageLogWindow::create(*wind_, core_);
            //messag window already a field in Application::Impl,its lifetime is managed autoly in Application::Impl, no need to manually destruct it
            gtr_window_on_close(
                *msgwin_,
                [this]()
                {
                    gtr_action_set_toggled("toggle-message-log", false);
                    msgwin_.reset();
                });

            gtr_action_set_toggled("toggle-message-log", true);
            msgwin_->show();
        }
        else
        {
            msgwin_->close();
        }
    }


    /*RelocateDialog*/
    else if (action_name == "relocate-torrent")
    {
        /*move storage for multiple torrents*/
        auto const ids = get_selected_torrent_uniq_ids();

        if (!ids.empty())
        {
            auto dialog = std::shared_ptr<RelocateDialog>(RelocateDialog::create(*wind_, core_, ids));

            // its owneris `active_dialogs_` field, so it kept alive even though go out of this else-if scope
            active_dialogs_[PeriphralDialogEnum::RelocateDialog] = dialog;

            // Capture as weak_ptr, dont need to worry the reference count that it may not destructed successfully
            std::weak_ptr<RelocateDialog> weak_dialog = dialog; 

  
            // when you open multiple this Dialog, the original (shared_ptr) reference count 
            // will drop to zero and its destrutor be called, 
            // so we cannot (no need to) open multiple identical Dialogs  
            gtr_window_on_close(*dialog, [weak_dialog,this]() mutable { 
                // Check if dialog is still alive
                if (auto locked_dialog = weak_dialog.lock()) 
                { 
                    active_dialogs_.erase(PeriphralDialogEnum::RelocateDialog);
                    std::cout << "Destruct RelocateDialog on handler" <<std::endl;
                    // Decrease the reference count
                    locked_dialog.reset(); 
                }
            });

            dialog->show();
                            // printf("w = %p \n", w.get());
                       
        }
    }


    /*MakeDialog*/
    else if (action_name == "new-torrent")
    {
        auto dialog = std::shared_ptr<MakeDialog>(MakeDialog::create(*wind_, core_));
        
        //its owneris `active_dialogs_` field, so it kept alive even though go out of this else-if scope
        active_dialogs_[PeriphralDialogEnum::MakeDialog] = dialog;

        // Capture as weak_ptr, dont need to worry the reference count that it may not destructed successfully
        std::weak_ptr<MakeDialog> weak_dialog = dialog; 

  
        // when you open multiple this Dialog, the original (shared_ptr) reference count 
        // will drop to zero and its destrutor be called, 
        // so we cannot (no need to) open multiple identical Dialogs  
        gtr_window_on_close(*dialog, [weak_dialog,this]() mutable { 
            // Check if dialog is still alive
            if (auto locked_dialog = weak_dialog.lock()) 
            { 
                active_dialogs_.erase(PeriphralDialogEnum::MakeDialog);
                std::cout << "Destruct MakeDialog on handler" <<std::endl;
                // Decrease the reference count
                locked_dialog.reset(); 
            }
        });


        dialog->show();
    }


    /*StatsDialog*/
    else if (action_name == "show-stats")
    {
        auto dialog = std::shared_ptr<StatsDialog>(StatsDialog::create(*wind_, core_));
        
        //its owneris `active_dialogs_` field, so it kept alive even though go out of this else-if scope
        active_dialogs_[PeriphralDialogEnum::StatsDialog] = dialog;

        // Capture as weak_ptr, dont need to worry the reference count that it may not destructed successfully
        std::weak_ptr<StatsDialog> weak_dialog = dialog; 

        // when you open multiple this Dialog, the original (shared_ptr) reference count 
        // will drop to zero and its destrutor be called, 
        // so we cannot (no need to) open multiple identical Dialogs  
        gtr_window_on_close(*dialog, [weak_dialog,this]() mutable { 
            // Check if dialog is still alive
            if (auto locked_dialog = weak_dialog.lock()) 
            { 
                active_dialogs_.erase(PeriphralDialogEnum::StatsDialog);
                std::cout << "Destruct StatsDialog on handler" << std::endl;
                // Decrease the reference count
                locked_dialog.reset(); 
            }
        });

        dialog->show();
    }

    /*remove from our sesion, no deleting local file*/
    else if (action_name == "remove-torrent")
    {
        remove_selected(false);
    }
    /*remove torrent from session and delete local file*/
    else if (action_name == "delete-torrent")
    {
        remove_selected(true);
    }
    else if (action_name == "quit")
    {
                        std::cout << "action `quit` triggered " << std::endl;
        on_app_exit();
    }
    /*sele all*/
    else if (action_name == "select-all")
    {
        wind_->select_all();
    }
    /*desele all*/
    else if (action_name == "deselect-all")
    {
        wind_->unselect_all();
    }

    /*show about dialog */
    else if (action_name == "show-about-dialog")
    {
        show_about_dialog();
    }
    /*open website*/
    else if (action_name == "help")
    {
        gtr_open_uri(gtr_get_help_uri());
    }

    else if (action_name == "toggle-main-window")
    {
        toggleMainWindow();
    }
    else if (action_name == "present-main-window")
    {
        presentMainWindow();
    }
    

    else
    {
        gtr_error(fmt::format("Unhandled action: {}", action_name));
    }

    if (changed)
    {
        update_model_soon();
    }
}
