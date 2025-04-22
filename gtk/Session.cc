// This file Copyright © Transmission authors and contributors.
// This file is licensed under the MIT (SPDX: MIT) license,
// A copy of this license can be found in licenses/ .

#include "Session.h"
#include "Actions.h"
#include "ListModelAdapter.h"
#include "Prefs.h"
#include "PrefsDialog.h"
#include "SortListModel.hh"
#include "Torrent.h"
#include "TorrentSorter.h"
#include "Utils.h"

#include "tr-transmission.h"
#include "tr-log.h"
#include "tr-utils.h" 
#include "tr-web-utils.h" // tr_urlIsValid()
#include "tr-favicon-cache.h"



#include <giomm/asyncresult.h>
#include <giomm/dbusconnection.h>
#include <giomm/fileinfo.h>
#include <giomm/filemonitor.h>
#include <giomm/liststore.h>
#include <glibmm/error.h>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/stringutils.h>
#include <glibmm/variant.h>
#include <glibmm/refptr.h>

#if GTKMM_CHECK_VERSION(4, 0, 0)
#include <gtkmm/sortlistmodel.h>
#else
#include <gtkmm/treemodelsort.h>
#endif


#include <glib.h>
#include <glib-object.h> 

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cinttypes> // PRId64
#include <cstring>   // strstr
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <mutex>
#include <condition_variable> 
#include <iomanip>


#include "libtorrent/error_code.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/settings_pack.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/session_stats.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/info_hash.hpp"
#include "libtorrent/magnet_uri.hpp"
#include "libtorrent/torrent_flags.hpp"
#include "libtorrent/session_handle.hpp"
#include "libtorrent/load_torrent.hpp"



//totem-gstbt
#include "gst_bt_demux.hpp" // for btdemux exposed function


using namespace lt;
using namespace std::literals;



class Session::Impl
{
public:
    Impl(Session &core, lt::session_params&& sp);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

    bool is_session_valid();

    void close();


    Glib::RefPtr<Gio::ListStore<Torrent>> get_raw_model() const;
    
    Glib::RefPtr<SortListModel<Torrent>> get_sorted_model();
    
    lt::session& get_session();

    lt::settings_pack get_sett() const;

    void set_sett_cache(int const key, bool newVal);
    void set_sett_cache(int const key, int newVal);
    void set_sett_cache(int const key, std::string const& newVal);

    void apply_settings_pack();

    std::pair<Glib::RefPtr<Torrent>, guint> find_torrent_by_uniq_id(std::uint32_t uniq_id) const;

    size_t get_active_torrent_count() const;

    int get_upload_rate_limit() const;

    int get_download_rate_limit() const;
    
    void add_stats_when_load(std::int64_t up, std::int64_t dn);

    void remove_torrent(lt::torrent_handle const& han, bool delete_files);

    void load(bool force_paused);


    void all_time_accum_clear();

    void all_time_accum_stats(
        std::uint64_t& all_up,
        std::uint64_t& all_dn,
        std::uint64_t& active_sec,
        std::uint64_t& open_times,
        std::uint64_t& current_sec
    );

    void only_current_stats(std::int64_t& accum_up, std::int64_t& accum_dn);



    static lt::torrent_handle empty_handle_;
    static Glib::RefPtr<Torrent> empty_Tor_RefPtr_;

    /*-----gstbt-related-----*/
    void retrieve_btdemux_gobj(GObject* obj);
    lt::bitfield const& totem_fetch_piece_bitfield();
    int totem_get_total_num_pieces();
    void totem_should_open(std::uint32_t id);
    void btdemux_should_close();
    bool is_totem_active();

    lt::torrent_status const& find_torrent_status(std::uint32_t uniq_id);

    lt::torrent_handle const find_torrent(std::uint32_t uniq_id);

    Glib::RefPtr<Torrent> const& find_Torrent(std::uint32_t uniq_id);


    void update();
    void torrents_added();

    void fetchTorHandleCB(tr_addtor_cb_t cb);

    std::string onTorHandleFetched(lt::torrent_handle const& h,  Glib::RefPtr<Torrent> const& Tor, bool& add_or_load);

    void add_files(std::vector<Glib::RefPtr<Gio::File>> const &files, bool do_prompt);
  

    void add_impl(lt::add_torrent_params && atp ,char const* dottor_filename, char const* download_dir, bool do_prompt);
    void add_torrent_ui(Glib::RefPtr<Torrent> const& torrent);
    bool add_from_url(Glib::ustring const &url);


    void commit_prefs_change(int const key);

    auto &signal_add_error()
    {
        return signal_add_error_;
    }

    auto &signal_add_prompt()
    {
        return signal_add_prompt_;
    }



    auto &signal_busy()
    {
        return signal_busy_;
    }

    auto &signal_prefs_changed()
    {
        return signal_prefs_changed_;
    }

 

    auto &signal_torrents_changed()
    {
        return signal_torrents_changed_;
    }

    [[nodiscard]] constexpr auto &favicon_cache()
    {
        return favicon_cache_;
    }

    int get_session_global_up_speed() const;

    int get_session_global_down_speed() const;


    void clear_move_file_err();

    void FetchTorRenameResult(rename_done_func callback, void* callback_user_data);

    lt::error_code FetchMoveFileErr();

    void inc_num_outstanding_resume_data();


private:
    // Glib::RefPtr<Session> get_core_ptr() const;

    bool is_busy() const;
    void add_to_busy(int addMe);
    void inc_busy();
    void dec_busy();

    bool add_file(Glib::RefPtr<Gio::File> const &file, bool do_prompt, lt::info_hash_t& avoid_duplicate);


    // void add_file_async_callback(
    //     Glib::RefPtr<Gio::File> const &file,
    //     Glib::RefPtr<Gio::AsyncResult> &result,
    //     tr_ctor *ctor,
    //     bool do_prompt);


    void add_torrent_no_prompt(lt::add_torrent_params atp, std::string const& dottor_path, std::string const& download_dir);

    void maybe_inhibit_hibernation();
    void set_hibernation_allowed(bool allowed);


    void watchdir_update();
    void watchdir_scan();
    void watchdir_monitor_file(Glib::RefPtr<Gio::File> const &file);
    bool watchdir_idle();
    void on_file_changed_in_watchdir(
        Glib::RefPtr<Gio::File> const &file,
        Glib::RefPtr<Gio::File> const &other_type,
        IF_GLIBMM2_68(Gio::FileMonitor::Event, Gio::FileMonitorEvent) event_type
    );

    void on_pref_changed(int const key);




    bool post_alerts_on_session();
    // void handle_alerts();


    void OnRenameDone(lt::error_code ec);
    
    void dispatch_torrent_status_alert(std::vector<lt::torrent_status> && st);

    void dispatch_peers_alert(lt::torrent_handle& hdl, std::vector<lt::peer_info> && peers);

    void dispatch_file_progress_alert(lt::torrent_handle& hdl,  std::vector<std::int64_t> && fps);

    void dispatch_trackers_alert(lt::torrent_handle& hdl , std::vector<lt::announce_entry> && trackers);

    // void process_alerts();

    
private:
    Session &core_;


    //contais settings_pack
    lt::session_params ses_params_cache_;

    //depends-on
    // lt::session& session_;
    lt::session session_;






    sigc::signal<void(ErrorCode, Glib::ustring const &)> signal_add_error_;
    sigc::signal<void(lt::add_torrent_params, char const*, char const*)> signal_add_prompt_;
    sigc::signal<void(bool)> signal_busy_;
    sigc::signal<void(int const)> signal_prefs_changed_;
    sigc::signal<void(std::unordered_set<std::uint32_t> const&, Torrent::ChangeFlags)> signal_torrents_changed_;
    sigc::connection post_torrent_update_timer_;
    sigc::connection handle_alerts_timer_;
    


    Glib::RefPtr<Gio::FileMonitor> watchdir_monitor_;
    sigc::connection watchdir_monitor_tag_;
    Glib::RefPtr<Gio::File> monitor_dir_;
    std::vector<Glib::RefPtr<Gio::File>> watchdir_monitor_files_;
    sigc::connection watch_monitor_idle_tag_;
    bool adding_from_watch_dir_ = false;


    bool inhibit_allowed_ = false;
    bool have_inhibit_cookie_ = false;
    bool dbus_error_ = false;
    guint inhibit_cookie_ = 0;
    gint busy_count_ = 0;


    Glib::RefPtr<Gio::ListStore<Torrent>> raw_model_;/*ListStore<Torrent>*/

    Glib::RefPtr<SortListModel<Torrent>> sorted_model_;/*SortListModel<Torrent>*/

    Glib::RefPtr<TorrentSorter> sorter_ = TorrentSorter::create();


    // lt::settings_pack sett_pack_cache_;


    /*handle with Torrent RefPtr*/
    std::unordered_map<lt::torrent_handle, Glib::RefPtr<Torrent>> m_all_torrents_;

    /*uniq-id with info-hash  */
    std::unordered_map<std::uint32_t, lt::info_hash_t> m_uid_ih_;

    FaviconCache<Glib::RefPtr<Gdk::Pixbuf>> favicon_cache_;

    std::queue<tr_addtor_cb_t> fetch_tor_handle_fun_;

    /*error code for move storage or rename file*/
    lt::error_code move_storage_ec_ = {};

    rename_done_func rename_done_cb_;

    void* rename_done_cb_user_data_ = nullptr;


    accum_stats across_ses_stats{std::time(nullptr)/* time now*/};


    // sigc::connection idle_handle_alerts_;


    std::mutex handle_alerts_mutex_;

    bool closing_ = false;


    // AlertQueue alert_queue_;
    GObject* btdemux_gobj_ = nullptr;
    std::uint32_t totem_uniq_id_ = -1;

    int num_outstanding_resume_data_ = 0;

    struct session_view
    {
        session_view();

	    void calc_speed();

    	void update_counters(lt::span<std::int64_t const> stats_counters, lt::clock_type::time_point t);

        int download_rate_ = {};

        int upload_rate_ = {};

        private:
        // there are two sets of counters. the current one and the last one. This
        // is used to calculate rates
        std::vector<std::int64_t> m_cnt_[2];   

        // the timestamps of the counters in m_cnt_[0] and m_cnt_[1]
        // respectively.
        lt::clock_type::time_point m_timestamp_[2];

        int const m_recv_idx_ = lt::find_metric_idx("net.recv_bytes");
        int const m_sent_idx_ = lt::find_metric_idx("net.sent_bytes");

        std::int64_t value(int idx) const;
        std::int64_t prev_value(int idx) const;

    public:
        std::int64_t get_recv_bytes();

        std::int64_t get_sent_bytes();

    };
	
    session_view ses_view_{};
};



lt::torrent_handle Session::Impl::empty_handle_ = lt::torrent_handle();
Glib::RefPtr<Torrent> Session::Impl::empty_Tor_RefPtr_ = Glib::RefPtr<Torrent>();


bool Session::is_session_valid()
{
    return impl_->is_session_valid();
}

bool Session::Impl::is_session_valid()
{
    return session_.is_valid();
}

/*********************session_view*************************/
Session::Impl::session_view::session_view()
{
	std::vector<lt::stats_metric> metrics = lt::session_stats_metrics();
	m_cnt_[0].resize(metrics.size(), 0);
	m_cnt_[1].resize(metrics.size(), 0);
}

std::int64_t Session::Impl::session_view::value(int idx) const
{
	if (idx < 0) return 0;
	return m_cnt_[0][std::size_t(idx)];
}

std::int64_t Session::Impl::session_view::prev_value(int idx) const
{
	if (idx < 0) return 0;
	return m_cnt_[1][std::size_t(idx)];
}

void Session::Impl::session_view::update_counters(span<std::int64_t const> stats_counters
	, lt::clock_type::time_point const t)
{
	// only update the previous counters if there's been enough
	// time since it was last updated
	if (t - m_timestamp_[1] > lt::seconds(2))
	{
		m_cnt_[1].swap(m_cnt_[0]);
		m_timestamp_[1] = m_timestamp_[0];
	}

	m_cnt_[0].assign(stats_counters.begin(), stats_counters.end());
	m_timestamp_[0] = t;

    calc_speed();
}


void Session::Impl::session_view::calc_speed()
{   
    using std::chrono::duration_cast;
	double const seconds = duration_cast<lt::milliseconds>(m_timestamp_[0] - m_timestamp_[1]).count() / 1000.0;

    download_rate_ = int((value(m_recv_idx_) - prev_value(m_recv_idx_))
    / seconds);
	upload_rate_ = int((value(m_sent_idx_) - prev_value(m_sent_idx_))
    / seconds);

}

std::int64_t Session::Impl::session_view::get_recv_bytes()
{
    return value(m_recv_idx_);
}


std::int64_t Session::Impl::session_view::get_sent_bytes()
{
    return value(m_sent_idx_);
}


/**********************************************************/

// Glib::RefPtr<Session> Session::Impl::get_core_ptr() const
// {
//     core_.reference();
//     return Glib::make_refptr_for_instance(&core_);
// }





/***************************************Model Model Model*************************************** */

/*Raw Model, return type is Glib::RefPtr<Gio::ListModel>*/
Glib::RefPtr<Gio::ListModel> Session::get_model() const
{
    return impl_->get_raw_model();
}
Glib::RefPtr<Gio::ListStore<Torrent>> Session::Impl::get_raw_model() const
{
    return raw_model_;
}



//Used for FilterBar, return type is `Glib::RefPtr<Gtk::TreeModel>`
Glib::RefPtr<Session::Model> Session::get_sorted_model() const
{
    return impl_->get_sorted_model();
}
Glib::RefPtr<SortListModel<Torrent>> Session::Impl::get_sorted_model()
{
    return sorted_model_;
}




/**
 * get libtorrent::session
 */
lt::session& Session::get_session() 
{
    return impl_->get_session();
}

lt::session& Session::Impl::get_session() 
{
    return session_;
}





/**
* get settings_pack of this session
*/
lt::settings_pack Session::get_sett() const
{
    return impl_->get_sett();
}
lt::settings_pack Session::Impl::get_sett() const
{
    //  std::printf("In Session::Impl::get_sett() %p !\n", &session_);

    // return session_.get_settings();

    return ses_params_cache_.settings;
    
}


int Session::get_upload_rate_limit() const
{
    return impl_->get_upload_rate_limit();
}

int Session::Impl::get_upload_rate_limit() const
{
    return session_.upload_rate_limit();
}



int Session::get_download_rate_limit() const
{
    return impl_->get_download_rate_limit();
}

int Session::Impl::get_download_rate_limit() const
{
    return session_.download_rate_limit();
}

int Session::get_session_global_up_speed() const
{
    return impl_->get_session_global_up_speed();
}
int Session::get_session_global_down_speed() const
{
    return impl_->get_session_global_down_speed();
    
}
int Session::Impl::get_session_global_up_speed() const
{
    return ses_view_.upload_rate_;
}

int Session::Impl::get_session_global_down_speed() const
{
    return ses_view_.download_rate_;
}
/***
****  BUSY
***/

bool Session::Impl::is_busy() const
{
    return busy_count_ > 0;
}

void Session::Impl::add_to_busy(int addMe)
{
    bool const wasBusy = is_busy();

    busy_count_ += addMe;

    if (wasBusy != is_busy())
    {
        signal_busy_.emit(is_busy());
    }
}

void Session::Impl::inc_busy()
{
    add_to_busy(1);
}

void Session::Impl::dec_busy()
{
    add_to_busy(-1);
}





/***
********************************************  WATCHDIR **************************************************
***/
namespace
{

    time_t get_file_mtime(Glib::RefPtr<Gio::File> const &file)
    {
        try
        {
            return file->query_info(G_FILE_ATTRIBUTE_TIME_MODIFIED)->get_attribute_uint64(G_FILE_ATTRIBUTE_TIME_MODIFIED);
        }
        catch (Glib::Error const &)
        {
            return 0;
        }
    }

    /*w.added means this torrent has been prompt to add,so the next time it won't prompt to add*/
    void watchdir_rename_torrent(Glib::RefPtr<Gio::File> const &file)
    {
        auto info = Glib::RefPtr<Gio::FileInfo>();

        try
        {
            info = file->query_info(G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);
        }
        catch (Glib::Error const &)
        {
            return;
        }

        auto const old_name = info->get_attribute_as_string(G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);
        auto const new_name = fmt::format("{}.added", old_name);

        try
        {
            file->set_display_name(new_name);
        }
        catch (Glib::Error const &e)
        {
            gtr_message(fmt::format(
                _("Couldn't rename '{old_path}' as '{path}': {error} ({error_code})"),
                fmt::arg("old_path", old_name),
                fmt::arg("path", new_name),
                fmt::arg("error", e.what()),
                fmt::arg("error_code", e.code())));
        }
    }


    std::string timet_to_string(std::time_t time)
    {
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }


} // anonymous namespace







bool Session::Impl::watchdir_idle()
{
    std::vector<Glib::RefPtr<Gio::File>> changing;
    std::vector<Glib::RefPtr<Gio::File>> unchanging;
    std::time_t const now = time(nullptr); //it's 1970-01-01 01:00:00

    /* separate the files into two lists: changing and unchanging */
    for (auto const &file : watchdir_monitor_files_)
    {
        //get file modify-time
        std::time_t const mtime = get_file_mtime(file);
                    
                        // auto timetstr = timet_to_string(mtime);
                        // auto nowtimetstr = timet_to_string(now);

                        // std::cout << "mtime is " << timetstr << ",now is " << nowtimetstr << std::endl;

        /*this .torrent file was modified within 2 sec*/
        if (mtime + 2 >= now)
        {
            changing.push_back(file);
        }
        /*this .torrent file was modified earlier than 2 sec ago*/
        else
        {
        
            unchanging.push_back(file);
        }
    }

    auto const& sett = get_sett();

    /* add the files that have stopped changing */
    if (!unchanging.empty())
    {
        bool const do_start = sett.get_bool(settings_pack::TR_KEY_start_added_torrents);
        bool const do_prompt = sett.get_bool(settings_pack::TR_KEY_show_options_window);


        adding_from_watch_dir_ = true;
        //add batch of torrents
        add_files(unchanging, do_prompt);
        std::for_each(unchanging.begin(), unchanging.end(), watchdir_rename_torrent);
        adding_from_watch_dir_ = false;
    }

    /* keep monitoring the ones that are still changing */
    watchdir_monitor_files_ = changing;

    /* if monitor_files is non-empty, keep checking every second */
    if (!watchdir_monitor_files_.empty())
    {
        return true;
    }

    watch_monitor_idle_tag_.disconnect();
    return false;
}



/* If this file is a torrent file, add it to our session */
void Session::Impl::watchdir_monitor_file(Glib::RefPtr<Gio::File> const &file)
{
    auto const filename = file->get_path();
    bool const is_torrent = Glib::str_has_suffix(filename, ".torrent");


    if (is_torrent)
    {
                                // std::cout << "WATCH_Dir --> got a file on watchdir " << filename << std::endl;

        bool const found = std::any_of(
            watchdir_monitor_files_.begin(),
            watchdir_monitor_files_.end(),
            [file](auto const &f)
            { return file->equal(f); });
        /* if we're not already watching this file, start watching it now */
        if (!found)
        {   
                                // std::cout << "ad one to monitor file vec " << std::endl;

            watchdir_monitor_files_.push_back(file);

            if (!watch_monitor_idle_tag_.connected())
            {
                watch_monitor_idle_tag_ = Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &Impl::watchdir_idle), 1);
            }
        }
    }
}



/* walk through the pre-existing files in the watchdir */
void Session::Impl::watchdir_scan()
{
    auto const& sett = get_sett();
    auto const dirname = sett.get_str(settings_pack::TR_KEY_watch_dir);

    try
    {
        for (auto const &name : Glib::Dir(dirname))
        {
            watchdir_monitor_file(Gio::File::create_for_path(Glib::build_filename(dirname, name)));
        }
    }
    catch (Glib::FileError const &)
    {
    }
}


/* GFileMonitor noticed a file was created */
void Session::Impl::on_file_changed_in_watchdir(
    Glib::RefPtr<Gio::File> const &file,
    Glib::RefPtr<Gio::File> const & /*other_type*/,
    IF_GLIBMM2_68(Gio::FileMonitor::Event, Gio::FileMonitorEvent) event_type)
{
    if (event_type == TR_GIO_FILE_MONITOR_EVENT(CREATED))
    {
        watchdir_monitor_file(file);
    }
}



void Session::Impl::watchdir_update()
{
    auto const& sett = get_sett();

    bool const is_enabled = sett.get_bool(settings_pack::TR_KEY_watch_dir_enabled);
    auto const dir = Gio::File::create_for_path(sett.get_str(settings_pack::TR_KEY_watch_dir));

            // std::cout << "in watchdir_update_, is_enabled is " << static_cast<int>(is_enabled)
            //     << " and tha tdir is " << dir->get_uri() << std::endl;

    /*if watch dir feature is disabled, disconnect all*/
    if (watchdir_monitor_ != nullptr && (!is_enabled || !dir->equal(monitor_dir_)))
    {
        watchdir_monitor_tag_.disconnect();
        watchdir_monitor_->cancel();

        monitor_dir_.reset();
        watchdir_monitor_.reset();
    }
    if (!is_enabled || watchdir_monitor_ != nullptr)
    {
        return;
    }

    /*if watch-dir enabled*/
    auto monitor = Glib::RefPtr<Gio::FileMonitor>();
    try
    {
        monitor = dir->monitor_directory();
    }
    catch (Glib::Error const &)
    {
        return;
    }

    watchdir_scan();

    watchdir_monitor_ = monitor;
    monitor_dir_ = dir;
    watchdir_monitor_tag_ = watchdir_monitor_->signal_changed().connect(sigc::mem_fun(*this, &Impl::on_file_changed_in_watchdir));
}





/*move storage or rename file*/
void Session::clear_move_file_err()
{
    impl_->clear_move_file_err();
}


void Session::Impl::clear_move_file_err()
{
    move_storage_ec_.clear(); /*same as `xx = error_code{}`*/;
}



lt::error_code Session::FetchMoveFileErr()
{
    return impl_->FetchMoveFileErr();
}


lt::error_code Session::Impl::FetchMoveFileErr()
{
    return move_storage_ec_; 
}



void Session::FetchTorRenameResult(rename_done_func callback, void* callback_user_data)
{
    impl_->FetchTorRenameResult(callback, callback_user_data);

}

void Session::Impl::FetchTorRenameResult(rename_done_func callback, void* callback_user_data)
{
    rename_done_cb_ = callback;
    rename_done_cb_user_data_ = callback_user_data;
}



void Session::Impl::OnRenameDone(lt::error_code ec)
{
    if(rename_done_cb_)
    {
        rename_done_cb_(rename_done_cb_user_data_, ec);
    }
}





void Session::Impl::on_pref_changed(int const key)
{
    auto const& sett = get_sett();
    auto& ses = get_session();

    //variable can't define in switch case body, or it will copmiler error
    bool bl_port_forward = {};
    bool bl_utp_enable = {};

    int byps = {};
    int conn_limit = {};

    switch (key)
    {
        case lt::settings_pack::TR_KEY_sort_mode:
            sorter_->set_mode(sett.get_str(lt::settings_pack::TR_KEY_sort_mode));
            break;

        case lt::settings_pack::TR_KEY_sort_reversed:
            sorter_->set_reversed(sett.get_bool(lt::settings_pack::TR_KEY_sort_reversed));
            break;

        //THIS IS GLOBAL LIMIT,*it actually modify the settings_pack::upload_rate_limit in libtorrent*
        case lt::settings_pack::TR_KEY_speed_limit_up:
             byps = sett.get_int(lt::settings_pack::TR_KEY_speed_limit_up);
                std::cout <<"on prefs changed : speed litmit up : " << byps << std::endl;
            // ses.set_upload_rate_limit(byps);
            //settings_pack::upload_rate_limit default value is `0`
            set_sett_cache(lt::settings_pack::upload_rate_limit, byps);       

            break;

/*
        "
        SET(upload_rate_limit, 0, &session_impl::update_upload_rate),
		SET(download_rate_limit, 0, &session_impl::update_download_rate),
        "

        when settings_pack::upload_rate_limit changed, session_impl::update_upload_rate will be called
        when settings_pack::download_rate_limit changed, session_impl::update_download_rate will be called

*/
        //THIS IS GLOBAL LIMIT,*it actually modify the settings_pack::download_rate_limit in libtorrent*
        case lt::settings_pack::TR_KEY_speed_limit_down:
            byps = sett.get_int(lt::settings_pack::TR_KEY_speed_limit_down);
            std::cout <<"on prefs changed : speed litmit down : " << byps << std::endl;
            // ses.set_download_rate_limit(byps);
            //settings_pack::download_rate_limit default value is `0`
            set_sett_cache(lt::settings_pack::download_rate_limit, byps);       

            break;

        //Stop the computer to sleep when there are torrents tasks 
        case lt::settings_pack::TR_KEY_inhibit_desktop_hibernation:
            maybe_inhibit_hibernation();
            break;

        
        case lt::settings_pack::TR_KEY_enable_port_forwarding:
            bl_port_forward = sett.get_bool(lt::settings_pack::TR_KEY_enable_port_forwarding);
            set_sett_cache(lt::settings_pack::enable_upnp, bl_port_forward);
            set_sett_cache(lt::settings_pack::enable_natpmp, bl_port_forward); 
                                /*
                                "          
                                SET(enable_upnp, true, &session_impl::update_upnp),
                                SET(enable_natpmp, true, &session_impl::update_natpmp),
                                "
                                when settings_pack::enable_upnp changed, session_impl::update_upnp will be called
                                when settings_pack::enable_natpmp changed, session_impl::enable_natpmp will be called
                                */
            break;

        //THIS IS GLOBAL LIMIT *it actually modify the settings_pack::connections_limit in libtorrent*
       case lt::settings_pack::TR_KEY_connections_limit:
                conn_limit = sett.get_int(lt::settings_pack::TR_KEY_connections_limit);

                 
                std::cout <<"on prefs changed : conn litmit  : " << conn_limit << std::endl;

            set_sett_cache(lt::settings_pack::connections_limit, conn_limit);       

            // ses.set_max_connections(conn_limit);

            break;


        case lt::settings_pack::TR_KEY_enable_utp:      
            bl_utp_enable = sett.get_bool(lt::settings_pack::TR_KEY_enable_utp);  
            set_sett_cache(lt::settings_pack::enable_outgoing_utp, bl_utp_enable);
            set_sett_cache(lt::settings_pack::enable_incoming_utp, bl_utp_enable);                          
            break;


        case lt::settings_pack::TR_KEY_watch_dir:
        case lt::settings_pack::TR_KEY_watch_dir_enabled:
            watchdir_update();
            break;




        default:
            //nothing needs to be done here, the settings item just modified
            break;
            
    };
}




/**
*** CTOR
**/

Glib::RefPtr<Session> Session::create(lt::session_params&& sp)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    return Glib::make_refptr_for_instance(new Session(std::move(sp)));
}






Session::Session(lt::session_params&& sp)
    : Glib::ObjectBase(typeid(Session)), impl_(std::make_unique<Impl>(*this, std::move(sp)))
{

}

Session::~Session() /*= default;*/
{
     std::printf("Session::~Session() \n");
}



void Session::Impl::dispatch_torrent_status_alert(std::vector<lt::torrent_status> && st)
{
    for (lt::torrent_status& t : st)
    {
        auto j = m_all_torrents_.find(t.handle);   

        //----add new one
        if (j == m_all_torrents_.end())
		{
            continue;
		}
        //----update existent one
		else
		{           
                    // std::cout << "in Session::Impl::dispatch_torrent_status_alert" << std::endl;
            auto underlying = j->second;

            std::lock_guard<std::mutex> lock(underlying->fetch_tor_status_mutex());  // Lock before modifying

            //**get_status_ref() exposed a non-const reference of its private field "tor_status_" in class Torrent::Impl 
            // Safely update: 1.The content of t is moved into tor_status_. 2.The old value of tor_status_ is destroyed (freed) 
            underlying->get_status_ref() = std::move(t);

                    // std::cout << "update tor_statsu_ " << std::endl;

                    // if(underlying->get_status_ref().handle.is_valid())
                    // {
                    //     std::cout << "get_status_ref().handle.is_valid(" << std::endl;
                    // }
		}
    }
}


void Session::Impl::dispatch_file_progress_alert(lt::torrent_handle& hdl,  std::vector<std::int64_t> && fps)
{
    auto j = m_all_torrents_.find(hdl); 
    if (j != m_all_torrents_.end())
    {
        auto& underlying = j->second;
        underlying->get_file_progress_vec_ref() = std::move(fps);
    }
}



void Session::Impl::dispatch_peers_alert(lt::torrent_handle& hdl, std::vector<lt::peer_info> && peers)
{
    auto j = m_all_torrents_.find(hdl); 
    if (j != m_all_torrents_.end())
    {
        auto& underlying = j->second;
        underlying->get_peers_vec_ref() = std::move(peers);
    }    
}



void Session::Impl::dispatch_trackers_alert(lt::torrent_handle& hdl , std::vector<lt::announce_entry> && trackers)
{
    auto j = m_all_torrents_.find(hdl); 
    if (j != m_all_torrents_.end())
    {
        auto& underlying = j->second;
        underlying->get_trackers_vec_ref() = std::move(trackers);
    }
}



// void Session::Impl::process_alerts()
// {
          
//     // if (alert_queue_.processing_stopped())
//     //     return; // Skip processing if stopped


//     std::vector<lt::alert*> alerts;
//     {
//         std::lock_guard<std::mutex> lock(handle_alerts_mutex_);
//         // alerts = alert_queue_.pop();  // Pop the alerts for processing

//     }


//     for(lt::alert* a : alerts)
//     {   
     
//         //when torrents state not update, p->status is empty
//         if (auto* p = alert_cast<state_update_alert>(a))
//         {       
//             if(p->status.empty())
//             {
//                 break;
//             }
//             dispatch_torrent_status_alert(std::move(p->status));
//         }

//         //details -> peers_info (ipaddr,)
//         else if (auto* p = alert_cast<peer_info_alert>(a))
//         {
//             dispatch_peers_alert(p->handle ,std::move(p->peer_info));
//         }
//         else if (auto* p = alert_cast<file_progress_alert>(a))
//         {
//             auto j = m_all_torrents_.find(p->handle);
//             if (j != m_all_torrents_.end())
//             {
//                 auto& underlying = j->second;
//                 underlying->get_file_progress_vec_ref() = std::move(p->files);
//             }
//         }
//         // else if (auto* p = alert_cast<piece_availability_alert>(a))
//         // {
            
//         // }
//         // else if (auto* p = alert_cast<piece_info_alert>(a))
//         // {
           
//         // }
//         //
//         // else if (auto* p = alert_cast<torrent_deleted_alert>(a))
//         // {
           
//         // }
//         else if(auto* p = alert_cast<metadata_received_alert>(a))
//         {
// 		    p->handle.save_resume_data(torrent_handle::save_info_dict);
//             ++num_outstanding_resume_data_;	
//         }
//         //details -> trackers, in filterbar tracker_combonox
//         else if (auto* p = alert_cast<tracker_list_alert>(a))
//         {
//             dispatch_trackers_alert(p->handle, std::move(p->trackers));
//         }

//         //add or load from resume 
//         else if (auto* p = alert_cast<add_torrent_alert>(a))
//         {   

//                                 std::cout << "add_torrent_alert!!!!!!!!!!!!!!" << std::endl;


//             if(p->error)
//             {
//                 //trying to add duplicated torrents
//                 if(p->error == lt::errors::duplicate_torrent)
//                 {
//                     //libtorrent internal will handle duplicate torrent
//                     // signal_add_error_.emit(ERR_ADD_TORRENT_DUP, p->torrent_name());
//                 }
//                 //another errors
//                 else 
//                 {
//                     signal_add_error_.emit(ERR_ADD_TORRENT_ERR, p->torrent_name());     
//                 }
//             }
//             else        
//             {       
//                 //a copy, handle is assumed to be valid
//                 auto h = p->handle;
    
//                 //store the handle in our Session field
//                 auto j = m_all_torrents_.emplace(std::move(h), Glib::RefPtr<Torrent>{}).first;
//                 if(!j->first.is_valid())
//                 {                       
//                    return;
//                 }

//                 Glib::RefPtr<Torrent> newTorr = Torrent::create(j->first);
//                 if(auto tmp_iter = m_all_torrents_.find(j->first); tmp_iter != m_all_torrents_.end())
//                 {
//                     //move ownership to our Session field
//                     tmp_iter->second = std::move(newTorr);
//                 }   
    
//                                 std::cout << "\n\n" << std::endl;
//                                 for(auto const& pari : m_all_torrents_)
//                                 {
//                                     std::cout << pari.first.name() << " ," << (pari.second->is_Tor_valid() ? "YES " : "NO ")  << pari.second->get_uniq_id() << std::endl; 
//                                 }
//                                 std::cout << "info hash:" << j->second->get_ih_str_form() << std::endl;
//                                 std::cout << "\n\n" << std::endl;


//                 //record to our Session field, just use copy
//                 m_uid_ih_.emplace(j->second->get_uniq_id(), j->second->get_ih());
                                
    

//                 //call callback others register to us
//                 bool add_or_load_ref {};
//                 auto mag_link = onTorHandleFetched(j->first, j->second, add_or_load_ref);
//                 if(!mag_link.empty())
//                 {
//                     j->second->saveMagnetLink(std::move(mag_link));
//                 }

//                 //add new torrents
//                 if(add_or_load_ref)
//                 {          
//                          std::cout << "from add" << std::endl;
//                     add_torrent_ui(j->second);
//                 }
//                 //load resume torrents
//                 else
//                 {           
//                         std::cout << "from load" << std::endl; 
//                     auto const model = get_raw_model();
//                     model->append(j->second);
//                 }

//                 //dont forget to save fast-resume file 
//                 j->first.save_resume_data(torrent_handle::save_info_dict | torrent_handle::if_metadata_changed);
//                 ++num_outstanding_resume_data_;	

//             }
//         }

//         /*for session global stats*/
//         else if (auto* p = alert_cast<session_stats_alert>(a))
// 	    {
// 		    ses_view_.update_counters(p->counters(), p->timestamp());
// 	    }


//         //torrent finished - need save resume data
//         else if (auto* p = alert_cast<torrent_finished_alert>(a))
//         {
//             lt::torrent_handle h = p->handle;
// 		    // h.set_max_connections(max_connections_per_torrent / 2);
//             h.save_resume_data(torrent_handle::save_info_dict | torrent_handle::if_download_progress);
//             ++num_outstanding_resume_data_;	
//         }
//         // when resume data is ready, save it     
//         else if (auto* p = alert_cast<save_resume_data_alert>(a))
//         {
//                     std::cout << "In save_resume_data_alert" << std::endl;
//             --num_outstanding_resume_data_;	
//             tr_torrentSaveResume(p->params); 
//         }
//         //resume data-saving failed
//         else if (auto* p = alert_cast<save_resume_data_failed_alert>(a))
//         {
           
//         }
//         //torrent paused
//         else if (auto* p = alert_cast<torrent_paused_alert>(a))
//         {
           
//         }

       
//         else if (auto* p = alert_cast<torrent_removed_alert>(a))
//         {
            
//         }
//         else if (auto* p = alert_cast<piece_info_alert>(a))
//         {
         
//         }


//         else if (auto* p = alert_cast<storage_moved_failed_alert>(a))
//         {
//             auto j = m_all_torrents_.find(p->handle);
//             //we found it 
//             if(j != m_all_torrents_.end())
//             {
//                 move_storage_ec_ = p->error;
  
//             }
//         }
//         else if (auto* p = alert_cast<storage_moved_alert>(a))
//         {   
//             //save new save_path to resume file
//             p->handle.save_resume_data(torrent_handle::if_config_changed);
//             ++num_outstanding_resume_data_;	
//         }

      
   
//         else if (auto* p = alert_cast<file_rename_failed_alert>(a))
//         {
//             OnRenameDone(std::move(p->error));
        
//         }
//         else if (auto* p = alert_cast<file_renamed_alert>(a))
//         {
//             p->handle.save_resume_data(torrent_handle::if_config_changed);
//             ++num_outstanding_resume_data_;	
//             OnRenameDone(lt::error_code{}/*empty*/);
//         }

//         else if(auto* p = alert_cast<alerts_dropped_alert>(a))
//         {
//             std::cerr<< " alert dropped: "<< a->message() << std::endl;
//         }
//     }

//     {
//         std::lock_guard<std::mutex> lock(handle_alerts_mutex_);
//         alert_queue_.signal_processing_complete();
//     }


// }



// void Session::Impl::handle_alerts() 
// {                           
//         // std::cout << "we handle alerts" << std::endl;
//     auto& ses = get_session();
//     std::vector<lt::alert*> alerts;

//     // Lock the mutex to ensure safe access to alerts
//     {
//         std::lock_guard<std::mutex> lock(handle_alerts_mutex_);
//         session_.pop_alerts(&alerts);
//     }

//             // std::cout << "KKKL:got alerts cluster " << alerts.size() << std::endl;

//     // Process alerts outside the locked section
//     for(auto a : alerts)
//     {
//         //when torrents state not update, p->status is empty
//         if (auto* p = alert_cast<state_update_alert>(a))
//         {       
//             if(p->status.empty())
//             {
//                 break;
//             }
//             dispatch_torrent_status_alert(std::move(p->status));
//         }

//         //details -> peers
//         else if (auto* p = alert_cast<peer_info_alert>(a))
//         {
//             dispatch_peers_alert(p->handle ,std::move(p->peer_info));
//         }
//         else if (auto* p = alert_cast<file_progress_alert>(a))
//         {
//             auto j = m_all_torrents_.find(p->handle);
//             if (j != m_all_torrents_.end())
//             {
//                 auto& underlying = j->second;
//                 underlying->get_file_progress_vec_ref() = std::move(p->files);
//             }
//         }
//         // else if (auto* p = alert_cast<piece_availability_alert>(a))
//         // {
            
//         // }
//         // else if (auto* p = alert_cast<piece_info_alert>(a))
//         // {
           
//         // }
//         //
//         // else if (auto* p = alert_cast<torrent_deleted_alert>(a))
//         // {
           
//         // }
//         else if(auto* p = alert_cast<metadata_received_alert>(a))
//         {
// 		    p->handle.save_resume_data(torrent_handle::save_info_dict);
//         }
//         //details -> trackers
//         else if (auto* p = alert_cast<tracker_list_alert>(a))
//         {
//             dispatch_trackers_alert(p->handle, std::move(p->trackers));
//         }

//         //add or load from resume 
//         else if (auto* p = alert_cast<add_torrent_alert>(a))
//         {   

//                                 std::cout << "add_torrent_alert!!!!!!!!!!!!!!" << std::endl;


//             if(p->error)
//             {
//                 //trying to add duplicated torrents
//                 if(p->error == lt::errors::duplicate_torrent)
//                 {
//                     //handled elsewhere
//                     // signal_add_error_.emit(ERR_ADD_TORRENT_DUP, p->torrent_name());
//                 }
//                 //another errors
//                 else 
//                 {
//                     signal_add_error_.emit(ERR_ADD_TORRENT_ERR, p->torrent_name());     
//                 }
//             }
//             else        
//             {       
//                 //a copy, handle is assumed to be valid
//                 auto h = p->handle;
    
//                 //store the handle in our Session field
//                 auto j = m_all_torrents_.emplace(std::move(h), Glib::RefPtr<Torrent>{}).first;
//                 if(!j->first.is_valid())
//                 {                       
//                    return;
//                 }

//                 Glib::RefPtr<Torrent> newTorr = Torrent::create(j->first);
//                 if(auto tmp_iter = m_all_torrents_.find(j->first); tmp_iter != m_all_torrents_.end())
//                 {
//                     //move ownership to our Session field
//                     tmp_iter->second = std::move(newTorr);
//                 }   
    
//                                 std::cout << "\n\n" << std::endl;
//                                 for(auto const& pari : m_all_torrents_)
//                                 {
//                                     std::cout << pari.first.name() << " ," << (pari.second->is_Tor_valid() ? "YES " : "NO ")  << pari.second->get_uniq_id() << std::endl; 
//                                 }
//                                 std::cout << "info hash:" << j->second->get_ih_str_form() << std::endl;
//                                 std::cout << "\n\n" << std::endl;


//                 //record to our Session field, just use copy
//                 m_uid_ih_.emplace(j->second->get_uniq_id(), j->second->get_ih());
                                
    

//                 //call callback others register to us
//                 bool add_or_load_ref {};
//                 auto mag_link = onTorHandleFetched(j->first, j->second, add_or_load_ref);
//                 if(!mag_link.empty())
//                 {
//                     j->second->saveMagnetLink(std::move(mag_link));
//                 }

//                 //add new torrents
//                 if(add_or_load_ref)
//                 {          
//                          std::cout << "from add" << std::endl;
//                     add_torrent_ui(j->second);
//                 }
//                 //load resume torrents
//                 else
//                 {           
//                         std::cout << "from load" << std::endl; 
//                     auto const model = get_raw_model();
//                     model->append(j->second);
//                 }

//                 //dont forget to save fast-resume file 
//                 // j->first.save_resume_data(torrent_handle::save_info_dict | torrent_handle::if_metadata_changed);
//             }
//         }

//         /*for session global stats*/
//         else if (auto* p = alert_cast<session_stats_alert>(a))
// 	    {
// 		    ses_view_.update_counters(p->counters(), p->timestamp());
// 	    }


//         //torrent finished - need save resume data
//         else if (auto* p = alert_cast<torrent_finished_alert>(a))
//         {
//             lt::torrent_handle h = p->handle;
// 		    // h.set_max_connections(max_connections_per_torrent / 2);
//             h.save_resume_data();
//         }
//         // when resume data is ready, save it     
//         else if (auto* p = alert_cast<save_resume_data_alert>(a))
//         {
//                     std::cout << "In save_resume_data_alert" << std::endl;
//             tr_torrentSaveResume(p->params); 
//         }
//         //resume data-saving failed
//         else if (auto* p = alert_cast<save_resume_data_failed_alert>(a))
//         {
           
//         }
//         //torrent paused
//         else if (auto* p = alert_cast<torrent_paused_alert>(a))
//         {
           
//         }

       
//         else if (auto* p = alert_cast<torrent_removed_alert>(a))
//         {
            
//         }
//         else if (auto* p = alert_cast<piece_info_alert>(a))
//         {
         
//         }


//         else if (auto* p = alert_cast<storage_moved_failed_alert>(a))
//         {
//             auto j = m_all_torrents_.find(p->handle);
//             //we found it 
//             if(j != m_all_torrents_.end())
//             {
//                 move_storage_ec_ = p->error;
  
//             }
//         }
//         else if (auto* p = alert_cast<storage_moved_alert>(a))
//         {   
//             //save new save_path to resume file
//             p->handle.save_resume_data(torrent_handle::if_config_changed);
//         }

      
   
//         else if (auto* p = alert_cast<file_rename_failed_alert>(a))
//         {
//             OnRenameDone(std::move(p->error));
        
//         }
//         else if (auto* p = alert_cast<file_renamed_alert>(a))
//         {
//             p->handle.save_resume_data(torrent_handle::if_config_changed);
//             OnRenameDone(lt::error_code{}/*empty*/);
//         }

//         else if(auto* p = alert_cast<alerts_dropped_alert>(a))
//         {
//             std::cerr<< " alert dropped: "<< a->message() << std::endl;
//         }
//     }


// }



bool Session::Impl::post_alerts_on_session()try
{
   
    /****post alerts ****/
    auto& ses = get_session();
    ses.post_torrent_updates();//post state_update_alert
    ses.post_session_stats();

    
    /*****push alerts to AlertQueue */
    std::vector<lt::alert*> alerts;
    {
        std::lock_guard<std::mutex> lock(handle_alerts_mutex_);
        // Collect alerts and push them to the queue
        /*******************IMPORTANT********************/
        // if (alert_queue_.is_processing_complete()  ) 
        // {
            ses.pop_alerts(&alerts);
         
            if(!alerts.empty())
            {
                                                // std::cout << "popped " << alerts.size() << " cnt " << std::endl;

                for (alert* a : alerts) 
                {           
                                                // std::cout << a->type() << "\t";
                                
                    switch(a->type())
                    {
                        case state_update_alert::alert_type:
                        {
                            auto* p = static_cast<state_update_alert*>(a);
                            if(!p->status.empty())
                            {
                                dispatch_torrent_status_alert(std::move(p->status));
                            }
                            break;
                        }

                        case save_resume_data_alert::alert_type:
                        {
                                            std::cout << "resume data is ready, save it " << std::endl;
                            auto* p = static_cast<save_resume_data_alert*>(a);
                            --num_outstanding_resume_data_;	
                            tr_torrentSaveResume(p->params); 
                            break;
                        }


                        case save_resume_data_failed_alert::alert_type:
                        {
                                            std::cout << "resume data not modified , no need to save it " << std::endl;
                            auto* p = static_cast<save_resume_data_failed_alert*>(a);
                            --num_outstanding_resume_data_;	
                        
                            break;
                        }

       
                        case add_torrent_alert::alert_type:
                        {   
                                            std::cout << "!!!!!!!!!!!!!!add_torrent_alert!!!!!!!!!!!!!!" << std::endl;
                            auto* p = static_cast<add_torrent_alert*>(a);

                            if(p->error)
                            {
                                //trying to add duplicated torrents
                                if(p->error == lt::errors::duplicate_torrent)
                                {
                                    //handled elsewhere
                                    // signal_add_error_.emit(ERR_ADD_TORRENT_DUP, p->torrent_name());
                                }
                                //another errors
                                else 
                                {
                                    signal_add_error_.emit(ERR_ADD_TORRENT_ERR, p->torrent_name());     
                                }
                            }
                            else        
                            {       
                                //a copy, handle is assumed to be valid
                                lt::torrent_handle h = p->handle;
                    
                                //store the handle in our Session field
                                auto j = m_all_torrents_.emplace(std::move(h), Glib::RefPtr<Torrent>{}).first;
                                if(!j->first.is_valid())
                                {                       
                                    break;;
                                }

                                Glib::RefPtr<Torrent> newTorr = Torrent::create(j->first);
                                if(auto tmp_iter = m_all_torrents_.find(j->first); tmp_iter != m_all_torrents_.end())
                                {
                                    //move ownership to our Session field
                                    tmp_iter->second = std::move(newTorr);
                                }   
                    
                                                std::cout << "\n\n" << std::endl;
                                                for(auto const& pari : m_all_torrents_)
                                                {
                                                    std::cout << pari.first.name() << " ," << (pari.second->is_Tor_valid() ? "YES " : "NO ")  << pari.second->get_uniq_id() << std::endl; 
                                                }
                                                std::cout << "info hash:" << j->second->get_ih_str_form() << std::endl;
                                                std::cout << "\n\n" << std::endl;


                                //record to our Session field, just use copy
                                m_uid_ih_.emplace(j->second->get_uniq_id(), j->second->get_ih());
                                                
                    

                                //call callback others register to us
                                bool add_or_load_ref {};
                                auto mag_link = onTorHandleFetched(j->first, j->second, add_or_load_ref);
                                if(!mag_link.empty())
                                {
                                    j->second->saveMagnetLink(std::move(mag_link));
                                }

                                //add new torrents
                                if(add_or_load_ref)
                                {          
                                        std::cout << "from add" << std::endl;
                                    add_torrent_ui(j->second);
                                }
                                //load resume torrents
                                else
                                {           
                                        std::cout << "from load" << std::endl; 
                                    auto const model = get_raw_model();
                                    model->append(j->second);
                                }

                                //dont forget to save fast-resume file 
                                                        std::cout << "add_torrent_alert, num_oustanding_resume++" << std::endl;
                                j->first.save_resume_data(torrent_handle::save_info_dict | torrent_handle::if_metadata_changed);
                                ++num_outstanding_resume_data_;	
                            }
                        break;
                    }

                    case tracker_list_alert::alert_type:
                    {
                        auto* p = static_cast<tracker_list_alert*>(a);
                        dispatch_trackers_alert(p->handle, std::move(p->trackers));
                    }    

                    case file_progress_alert::alert_type:
                    {
                        auto* p = static_cast<file_progress_alert*>(a);
                        dispatch_file_progress_alert(p->handle, std::move(p->files));
                        break;
                    }                        
                
                    case peer_info_alert::alert_type:
                    {
                        auto* p = static_cast<peer_info_alert*>(a);
                        dispatch_peers_alert(p->handle ,std::move(p->peer_info));
                        break;
                    }
                        
                
                    case storage_moved_failed_alert::alert_type:
                    {
                                                    std::cout << "got storage_moved_failed_alert" << std::endl;

                        auto* p = static_cast<storage_moved_failed_alert*>(a);
                        auto j = m_all_torrents_.find(p->handle);
                        //we found it 
                        if(j != m_all_torrents_.end())
                        {
                            move_storage_ec_ = p->error;
                        }
                        break;
                    }

                    case storage_moved_alert::alert_type:
                    {
                        auto* p = static_cast<storage_moved_alert*>(a);

                        std::cout << "storage moved alert: moved_Path  "  << p->storage_path()
                        << " old_Path " << p->old_path() << std::endl;

                        p->handle.save_resume_data(torrent_handle::if_config_changed);
                        ++num_outstanding_resume_data_;	

                        break;
                    }

                    case file_rename_failed_alert::alert_type:
                    {
                                                    std::cout << "got file_rename_failed_alert" << std::endl;

                        auto* p = static_cast<file_rename_failed_alert*>(a);

                        OnRenameDone(std::move(p->error));

                        break;
                    }


                    case file_renamed_alert::alert_type:
                    {
                        auto* p = static_cast<file_renamed_alert*>(a);

                        std::cout << "file renamed alert: old name  "  << p->old_name()
                        << " new name " << p->new_name() << std::endl;

                        p->handle.save_resume_data(torrent_handle::if_config_changed);
                        ++num_outstanding_resume_data_;	

                        OnRenameDone(lt::error_code{}/*no-error*/);
                        break;
                    }

                    case torrent_paused_alert::alert_type:
                    {
                        if (!closing_)
                        {
                            auto* p = static_cast<torrent_paused_alert*>(a);
                            torrent_handle h = p->handle;
                            h.save_resume_data(torrent_handle::save_info_dict);
                            ++num_outstanding_resume_data_;
                        }
                        break;
                    }

                    case torrent_finished_alert::alert_type:
                    {
                        auto* p = static_cast<torrent_finished_alert*>(a);

                        // p->handle.set_max_connections(max_connections_per_torrent / 2);
                        // write resume data for the finished torrent
                        // the alert handler for save_resume_data_alert
                        // will save it to disk

                        p->handle.save_resume_data(torrent_handle::save_info_dict | torrent_handle::if_download_progress);
                        ++num_outstanding_resume_data_;
                        break;
                    }

                    case session_stats_alert::alert_type:
                    {
                        auto* p = static_cast<session_stats_alert*>(a);
                        ses_view_.update_counters(p->counters(), p->timestamp());
                        break;

                    }
                    /*******************feed gst-btdemux loop********************/
                    case read_piece_alert::alert_type:
                    {
                        if(totem_uniq_id_ > -1){
                            btdemux_feed_read_piece_alert(btdemux_gobj_, a);
                        }
                        break;
                    }

                    case piece_finished_alert::alert_type:
                    {
                        if(totem_uniq_id_ > -1){
                            btdemux_feed_piece_finished_alert(btdemux_gobj_, a);
                        }
                        break;
                    }


                }

            }
                                                // std::cout << std::endl;
                // alert_queue_.push(std::move(alerts)); 
            }
        // }
    }
    
    // Ensure that *only one* idle handler is connected
    // if (!idle_handle_alerts_.connected()) 
    // {
    //     idle_handle_alerts_ = Glib::signal_idle().connect([this]() { 
    //         try{ 

    //             process_alerts();
    //             return false;//call once
    //         }
    //         catch(const std::exception& e) 
    //         {
    //             // Handle the exception (e.g., log it, clean up resources)
    //             std::cerr << "Exception caught in handle_alerts handler: " << e.what() << std::endl;
    //             return false; // Stop calling this idle function if necessary
    //         }
    //     });

    // }

    return true;//continue timer
}
catch(const std::exception& e) 
{
    // Handle the exception (e.g., log it, clean up resources)
    std::cerr << "Exception caught in handle_alerts handler: " << e.what() << std::endl;
    return false; // Stop calling this idle function if necessary
}




Session::Impl::Impl(Session &core, lt::session_params&& sp)
    : core_(core), 
    // use session_params object to Constructs the session object
    ses_params_cache_(std::move(sp)),
    session_(lt::session(ses_params_cache_))
{
     std::printf("In Session::Impl Ctor %p !\n", &session_);

    //get the settings_pack copy 
    // sett_pack_cache_ <> ses_params_cache_.settings;

    across_ses_stats.inject(ses_params_cache_.settings);

    /******** */
    raw_model_ = Gio::ListStore<Torrent>::create();
    signal_torrents_changed_.connect(sigc::hide<0>(sigc::mem_fun(*sorter_, &TorrentSorter::update)));
    sorted_model_ = SortListModel<Torrent>::create(gtr_ptr_static_cast<Gio::ListModel>(raw_model_), sorter_);


    /* init from prefs & listen to pref changes */
    on_pref_changed(lt::settings_pack::TR_KEY_sort_mode);
    on_pref_changed(lt::settings_pack::TR_KEY_sort_reversed);
    
    //strat watch-dir feature (like a daemon)according to given settings
    on_pref_changed(lt::settings_pack::TR_KEY_watch_dir_enabled);
    on_pref_changed(lt::settings_pack::TR_KEY_inhibit_desktop_hibernation);
    signal_prefs_changed_.connect([this](int const key)
                                  { on_pref_changed(key); });


    post_torrent_update_timer_ = Glib::signal_timeout().connect_seconds(
    sigc::mem_fun(*this, &Impl::post_alerts_on_session),
    1);

    // handle_alerts_timer_ = Glib::signal_timeout().connect_seconds([this]() { 
    //         try{ 

    //             process_alerts();
    //             return true;//not call just once
    //         }
    //         catch(const std::exception& e) 
    //         {
    //             // Handle the exception (e.g., log it, clean up resources)
    //             std::cerr << "Exception caught in handle_alerts handler: " << e.what() << std::endl;
    //             return false; // Stop calling this idle function if necessary
    //         }
    //     }, 1);
}

Session::Impl::~Impl()
{
                    std::cout << "Session::Impl::~Impl...\n" << std::endl;
    // close();
    watchdir_monitor_tag_.disconnect();
    watch_monitor_idle_tag_.disconnect();
    post_torrent_update_timer_.disconnect();
    // alert_queue_.stop_processing();
    handle_alerts_timer_.disconnect();

    if(btdemux_gobj_){
        btdemux_gobj_ = nullptr;
    }

}




void Session::close()
{
    impl_->close();
    

}

void Session::Impl::close()
{

    if(closing_)
    {
        return;
    }
    closing_ = true;

                    std::cout << "close session...\n" << std::endl;

    // save ses.sett file
    // lt::session_params ret = session_.session_state();
    // only non-default settings(default settings no need to be serialized)
    session_.pause();                    

 
    across_ses_stats.dump(ses_params_cache_.settings);
    tr_sessionSaveSettings(ses_params_cache_);

    // save all torrents' resume files
    // get all the torrent handles that we need to save resume data for
	std::vector<lt::torrent_status> const temp = session_.get_torrent_status
    (
		[](torrent_status const& st)
		{
			return st.handle.is_valid() && st.has_metadata && st.need_save_resume;
		}, {}
    );
    for (auto const& st : temp)
	{
		// save_resume_data will generate an alert when it's done
                                std::cout << "save one torrents' resume " << std::endl;
		st.handle.save_resume_data(torrent_handle::save_info_dict);
		++num_outstanding_resume_data_;	
	}

    while (num_outstanding_resume_data_ > 0 || !session_.is_paused())
	{
		                        // std::cout << num_outstanding_resume_data_ << "," << static_cast<int>(session_.is_paused()) << ";";
	}

    

    // Cleanup all Torrents
    for (auto& pair : m_all_torrents_)
    {
        // Access the Torrent object through the RefPtr
        Glib::RefPtr<Torrent>& Tor = pair.second;

        if (Tor)
        {   
            // Call a method to disconnect signals in the Torrent instance
            Tor->disconnect_signals(); // Ensure this method exists
                                    std::cout << "...unreferencing Torrent object" << std::endl;
            Tor->unreference();
        }
    }
    
    m_uid_ih_.clear();
    m_all_torrents_.clear(); // This will release the references held by the map
    //also clear singal in Session
    watchdir_monitor_tag_.disconnect();
    watch_monitor_idle_tag_.disconnect();
    post_torrent_update_timer_.disconnect();
    handle_alerts_timer_.disconnect();


    std::cout << "close session, save resume data and cleanup finished.\n" << std::endl;
}



void Session::inc_num_outstanding_resume_data()
{
    impl_->inc_num_outstanding_resume_data();
}
void Session::Impl::inc_num_outstanding_resume_data()
{
    ++num_outstanding_resume_data_;
}



void Session::all_time_accum_clear()
{
    impl_->all_time_accum_clear();
}
void Session::Impl::all_time_accum_clear()
{
    across_ses_stats.zerofy();
}



void Session::all_time_accum_stats(
    std::uint64_t& all_up,
    std::uint64_t& all_dn,
    std::uint64_t& active_sec,
    std::uint64_t& open_times,
    std::uint64_t& current_sec
)
{

    impl_->all_time_accum_stats(all_up, all_dn, active_sec, open_times, current_sec);
}

void Session::Impl::all_time_accum_stats(
    std::uint64_t& all_up,
    std::uint64_t& all_dn,
    std::uint64_t& active_sec,
    std::uint64_t& open_times,
    std::uint64_t& current_sec
)
{
    across_ses_stats.offer(all_up, all_dn, active_sec, open_times, current_sec);
}


void Session::only_current_stats(std::int64_t& accum_up, std::int64_t& accum_dn)
{
    impl_->only_current_stats(accum_up, accum_dn);

}

void Session::Impl::only_current_stats(std::int64_t& accum_up, std::int64_t& accum_dn)
{
    accum_dn = ses_view_.get_recv_bytes();
    accum_up = ses_view_.get_sent_bytes();
}





std::pair<Glib::RefPtr<Torrent>, guint> Session::Impl::find_torrent_by_uniq_id(std::uint32_t uniq_id) const
{
    auto begin_position = 0U;
    auto end_position = raw_model_->get_n_items();

    while (begin_position < end_position)
    {
        auto const position = begin_position + (end_position - begin_position) / 2;
        auto const torrent = raw_model_->get_item(position);
        auto const current_torrent_uniq_id = torrent->get_uniq_id();

        if (current_torrent_uniq_id == uniq_id)
        {
            return {torrent, position};
        }
        (current_torrent_uniq_id < uniq_id ? begin_position : end_position) = position;
    }

    return {};
}




















// void Session::Impl::add_file_async_callback(
//     Glib::RefPtr<Gio::File> const &file,
//     Glib::RefPtr<Gio::AsyncResult> &result,
//     tr_ctor *ctor,
//     bool do_prompt)
// {
//     try
//     {
//         gsize length = 0;
//         char *contents = nullptr;

//         if (!file->load_contents_finish(result, contents, length))
//         {
//             gtr_message(fmt::format(_("Couldn't read '{path}'"), fmt::arg("path", file->get_parse_name())));
//         }

//         // else if (tr_ctorSetMetainfo(ctor, contents, length, nullptr))
//         // {
//         //     add_impl(ctor, do_prompt, do_notify);
//         // }
//         // else
//         // {
//         //     tr_ctorFree(ctor);
//         // }

//     }
//     catch (Glib::Error const &e)
//     {
//         gtr_message(fmt::format(
//             _("Couldn't read '{path}': {error} ({error_code})"),
//             fmt::arg("path", file->get_parse_name()),
//             fmt::arg("error", e.what()),
//             fmt::arg("error_code", e.code())));
//     }

//     dec_busy();
// }





 


/********************************ADD NEW TORRENTS TO SESSIOn***************************************/
bool Session::add_from_url(Glib::ustring const &url)
{
    return impl_->add_from_url(url);
}

bool Session::Impl::add_from_url(Glib::ustring const &maglink)
{
    // auto& ses = get_session();
    // if (!ses.is_valid())
    // {
    //     return false;
    // }

    /* load global settings */
    auto sett = get_sett();
    auto const download_dir = sett.get_str(settings_pack::TR_KEY_download_dir);
    auto const do_prompt = sett.get_bool(settings_pack::TR_KEY_show_options_window);

    bool handled = false;
    bool loaded = false;
    lt::error_code ec;
	lt::add_torrent_params atp = lt::parse_magnet_uri(maglink.c_str(), ec);
    //handle exceptions here ... user may input invalid magnet-url, 
    if (ec)
	{
        std::printf("sorry, got invalid magnet link \"%s\": %s\n"
			, maglink.c_str(), ec.message().c_str());
		return false;
	}
    loaded = true;

    if (loaded)
    {   
        add_impl(std::move(atp), ""/*no torrent file*/, download_dir.c_str(), do_prompt);
        handled = true;
    }
 
    //emit ERR_NO_MORE_TORRENTS via signal_add_error_, which means it's time to flush errors now
    torrents_added();
    return handled;
}



/**
    used by TorrentFileChooserDialog 
    do_start \ do_prompt loaded by TorrentFileChooserDialog
    can multi-select to add multiple torrent at the same time
 */
void Session::add_files(std::vector<Glib::RefPtr<Gio::File>> const &files, bool do_prompt)
{
    /* logging */
    tr_logAddInfo(_("Add Torrent from TorrentFileChooserDialog"));

    impl_->add_files(files, do_prompt);
}


void Session::Impl::add_files(std::vector<Glib::RefPtr<Gio::File>> const &files, bool do_prompt)
{
    lt::info_hash_t avoid_duplicate = {};
    bool ret = {true};
    /*multi-select torrents and add*/
    for (auto const &file : files)
    {
        ret &= add_file(file, do_prompt, avoid_duplicate);
    }

    //emit ERR_NO_MORE_TORRENTS via signal_add_error_, which means it's the end of add torrents file, we can flush errors now
    torrents_added(); 
}


bool Session::Impl::add_file(Glib::RefPtr<Gio::File> const &file,  bool do_prompt, lt::info_hash_t& avoid_duplicate)
{
    auto& ses = get_session();
    if (!ses.is_valid())
    {
        return false;
    }

    /*get torrent file full-path*/
    auto const path = file->get_path();
    if (std::empty(path))
        return false;
    
    /*get default download_dir*/
    auto sett = get_sett();
    auto download_dir = sett.get_str(settings_pack::TR_KEY_download_dir);

    //user may add a corrupted torrent file, if that happen, no need to abort whole program
    try
    {
        /*create add_torrent_params for later use */
        lt::add_torrent_params atp = lt::load_torrent_file(path);
        //trying to add duplicates torrent will notify using add_torrent_alert

        atp.flags |= lt::torrent_flags::duplicate_is_error;

        /*avoid add duplicate torrents which has identical info-hash*/
        if(!avoid_duplicate.has_v1())
        {
            avoid_duplicate = atp.info_hashes;
        }
        else if(atp.info_hashes == avoid_duplicate)
        { 
            signal_add_error_.emit(ERR_ADD_TORRENT_DUP, !atp.name.empty() ? atp.name.c_str() : "Unknow Torrent");
            return false;
        }

        //pop up torrent options dialog
        add_impl(std::move(atp), path.c_str(), download_dir.c_str(), do_prompt);
    
        return true;
    }
    catch(lt::system_error const& se)
    {
                        std::cout << " Session::Impl::add_file catch lt::system_error: " << se.what() << std::endl;
        signal_add_error_.emit(ERR_ADD_TORRENT_ERR, std::string{se.what()});
        
    tr_logAddInfo(fmt::format(_("Add Torrent from torrentfile GOT ERROR: {err}"), fmt::arg("err", se.what())));



        return false;
    }
    
 


}



/**
 * Make a torrent -> add to session
 */
void Session::add_impl(lt::add_torrent_params && atp, const char* dottor_path, const char* down_dir)
{
    auto sett = get_sett();
    bool const do_prompt = sett.get_bool(settings_pack::TR_KEY_show_options_window);

    /* logging */
    tr_logAddInfo(_("Add Torrent from MakeDialog"));

    impl_->add_impl(std::move(atp) ,dottor_path/*full path of new .tor file*/, down_dir/*parent dir of our new-made .tor file*/, do_prompt);
}



void Session::Impl::add_impl(lt::add_torrent_params&& atp, char const* dottor_filename, char const* download_dir, bool do_prompt)
{
    /* 
        way 1: this not show Option Dialog, just pass the params for async_add_torrent()
        -- defaults to use TR_KEY_download_dir  as download destination dir
        -- defaults to download all files in this torrent
        -- cannot set priority of this torrent
        -- defaults to TR_KEY_start_added to decide start added torrent or not
        -- defaults to TR_KEY_trash_original_torrent to decide trash torrent file after add
    */
    if (!do_prompt)
    {
        add_torrent_no_prompt(atp, std::string(dottor_filename), std::string(download_dir));
        return;
    }

    /*
        way 2: this will pop up Torrent Options Dialog
        -- can change torrent in source pathButton
        -- can change download dir destination
        -- can pick which file you want  to download
        -- priority spinbutton
        -- checkbox for start_added and trash_original_files
    */
    signal_add_prompt_.emit(std::move(atp), dottor_filename , download_dir);
}






void Session::Impl::add_torrent_no_prompt(lt::add_torrent_params atp, std::string const& dottor_path, std::string const& download_dir)
{
    /*this method used when not using the OptionsDialog, so need to check whether trash original torrent files*/
    bool const trash_origin = get_sett().get_bool(settings_pack::TR_KEY_trash_original_torrent_files);
    bool const start_added = get_sett().get_bool(settings_pack::TR_KEY_start_added_torrents);
    bool const add_from_magnet = dottor_path.empty();

    atp.save_path = download_dir;
    std::string mag_link = lt::make_magnet_uri(atp); 

    if(!start_added)
    {
        // stop_when_ready will stop the torrent immediately when it's done
        // checking.
        atp.flags |= lt::torrent_flags::stop_when_ready;
    }

    session_.async_add_torrent(std::move(atp));

    if(!add_from_magnet)
    {
        fetchTorHandleCB(
             static_cast<tr_addtor_cb_t>(
            [start_added, trash_origin, dottor_path, mag_link](lt::torrent_handle const& handle, Glib::RefPtr<Torrent> const& Tor, bool& add_or_load)
        {
                                        std::cout << "hello from Session::Impl::::add_torrent_no_prompt " << std::endl;
                                        std::cout << "maglink is " << mag_link << std::endl;
            add_or_load = true;
            // if(!start_added)
            // {
            //     /*
            //     pause has two procedures
            //         1.unset auto-managed
            //         2.pause()
            //     */
            //     handle.unset_flags(lt::torrent_flags::auto_managed);
            //     handle.pause(torrent_handle::graceful_pause);
            // }
            if(trash_origin)
            {
                gtr_file_trash_or_remove(dottor_path, nullptr);
            }

            return mag_link;
        }
        )
        );
    }
    else
    {
        fetchTorHandleCB(
             static_cast<tr_addtor_cb_t>(
            [start_added, dottor_path, mag_link](lt::torrent_handle const& handle,   Glib::RefPtr<Torrent> const& Tor, bool& add_or_load)
        {
                                        std::cout << "hello from Session::Impl::::add_torrent_no_prompt magnet-uri Branch" << std::endl;
                                        std::cout << "maglink is " << mag_link << std::endl;

            add_or_load = true;
            // if(!start_added)
            // {
            //     /*
            //     pause has two procedures
            //         1.unset auto-managed
            //         2.pause()
            //     */
            //     handle.unset_flags(lt::torrent_flags::auto_managed);
            //     handle.pause(torrent_handle::graceful_pause);
            // }

            return mag_link;

        }));
    }

}




// void Session::add_torrent_ui(Glib::RefPtr<Torrent> const &torrent)
// {
//     impl_->add_torrent(torrent);
// }
void Session::Impl::add_torrent_ui(Glib::RefPtr<Torrent> const &Tor)
{           
                            std::cout << "In Session::Impl::add_torrent_ui(" << std::endl;
    if (Tor)
    {
        raw_model_->insert_sorted(Tor, &Torrent::compare_by_uniq_id);
    }
}






void Session::torrents_added()
{
    impl_->torrents_added();
}

void Session::Impl::torrents_added()
{
    update();
    signal_add_error_.emit(ERR_NO_MORE_TORRENTS, {});
}




void Session::fetchTorHandleCB(tr_addtor_cb_t cb)
{
    impl_->fetchTorHandleCB(cb);
}

void Session::Impl::fetchTorHandleCB(tr_addtor_cb_t cb)
{
    if(cb == nullptr)
        return;
    fetch_tor_handle_fun_.push(cb);
}



std::string Session::Impl::onTorHandleFetched(lt::torrent_handle const& h,  Glib::RefPtr<Torrent> const& Tor, bool& add_or_load)
{
    std::string mag_link {};
    if(!fetch_tor_handle_fun_.empty())
    {
        tr_addtor_cb_t job = fetch_tor_handle_fun_.front();
        if(job)
        {
            mag_link = job(h, Tor, add_or_load);
        }
        fetch_tor_handle_fun_.pop();
    }
    return mag_link;
}



void Session::torrent_changed(std::uint32_t uniqid)
{
    if (auto const &[torrent, position] = impl_->find_torrent_by_uniq_id(uniqid); torrent)
    {
        torrent->update();
    }
}


/****Remove***** */
void Session::remove_torrent(lt::torrent_handle const& han, bool delete_files)
{
    impl_->remove_torrent(han, delete_files);
}

void Session::Impl::remove_torrent(lt::torrent_handle const& han, bool delete_files)
{
    std::uint32_t unqid{};
    lt::info_hash_t const& target_ih = han.info_hashes();
    /*To find whether this torrent already in our stored field according to info hash*/
    auto it = std::find_if(std::begin(m_uid_ih_), std::end(m_uid_ih_),
        [&target_ih](auto& p) { return p.second == target_ih; });
    //found it
    if (it != std::end(m_uid_ih_))
    {
        unqid = it->first;
        m_uid_ih_.erase(it);
    }

    auto j = m_all_torrents_.find(han);
    //found it
    if (j != m_all_torrents_.end())
    {
        auto& Tor = j->second;
        std::int64_t delta_up{}, delta_down{};
        Tor->total_updown_delta_at_remove(delta_up, delta_down);
        across_ses_stats.inc_upload_bytes(delta_up);
        across_ses_stats.inc_download_bytes(delta_down);

        // Disconnect any signals/timers associated with the Big Torrent
        Tor->disconnect_signals(); // Ensure you have a method to handle this

        m_all_torrents_.erase(j);
    }


    if (auto const &[torrent, position] = find_torrent_by_uniq_id(unqid); torrent)
    {   
        /* remove from the gui */
        get_raw_model()->remove(position);
        
        auto& ses = get_session();

        // also delete the resume file
        std::string resumepath = FetchTorResumePath(target_ih);

        if(delete_files)
        {
            ses.remove_torrent(han, lt::session::delete_files);
             //if resume file do exists, delete it really
            if (!resumepath.empty())
            {
                gtr_file_trash_or_remove(resumepath, nullptr);
            }  
        }
        else
        {
            ses.remove_torrent(han);
        }  
    }

    /* logging */
    tr_logAddInfo(_("Remove Torrent "));
}



void Session::load(bool force_paused)
{
    impl_->load(force_paused);
}


/*LOAD HISTORY TORRENTS FROM RESUME DATA*/
void Session::Impl::load(bool force_paused)
{    
    /*load from each resume files in config dir*/
    auto apts = tr_torrentLoadResume();
    for(auto param : apts)
    {
        /*add to stats*/
        std::int64_t all_dn = param.total_downloaded;
        std::int64_t all_up = param.total_uploaded;
        add_stats_when_load(all_up, all_dn);
        
        if(force_paused)
        {
            // stop_when_ready will stop the torrent immediately when it's done
            // checking.
            param.flags |= lt::torrent_flags::stop_when_ready;
        }

        std::string mag_link = lt::make_magnet_uri(param);
        auto& ses = get_session();
        ses.async_add_torrent(std::move(param));
        //register callback after receive torrent handle
        fetchTorHandleCB(
        static_cast<tr_addtor_cb_t>(    
        [mag_link, force_paused, all_up, all_dn ](lt::torrent_handle const& handle,  Glib::RefPtr<Torrent> const& Tor, bool& add_or_load)
        {
            add_or_load = false;
            // if(force_paused)
            // {
            //     /*
            //     pause has two procedures
            //         1.unset auto-managed
            //         2.pause()
            //     */
            //     handle.unset_flags(lt::torrent_flags::auto_managed);
            //     handle.pause(torrent_handle::graceful_pause);
            // }
            
            if(Tor->is_Tor_valid())
            {
                Tor->feed_total_updown_at_load(all_up, all_dn);
            }
            return mag_link;
        }
        )
        );



    }


/*
    torrents.reserve(raw_torrents.size());
    std::transform(raw_torrents.begin(), raw_torrents.end(), std::back_inserter(torrents), &Torrent::create);
    std::sort(torrents.begin(), torrents.end(), &Torrent::less_by_uniq_id);
    auto const model = impl_->get_raw_model();
    model->splice(0, model->get_n_items(), torrents);
*/

}



void Session::Impl::add_stats_when_load(std::int64_t up, std::int64_t dn)
{
    across_ses_stats.inc_upload_bytes(up);
    across_ses_stats.inc_download_bytes(dn);
}



void Session::clear()
{
    impl_->get_raw_model()->remove_all();
}






/***********************************Update************************************/
void Session::update()
{
    impl_->update();
}
void Session::Impl::update()
{

        // std::cout << "do Session::Impl::update()" << std::endl;

    auto uniq_ids = std::unordered_set<std::uint32_t>();
    auto changes = Torrent::ChangeFlags();

    /* update the model */
    for (auto i = 0U, count = raw_model_->get_n_items(); i < count; ++i)
    {
        auto const Tor = raw_model_->get_item(i);
        if (auto const torrent_changes = Tor->update(); torrent_changes.any())
        {
            uniq_ids.insert(Tor->get_uniq_id());
            changes |= torrent_changes;
        }
    }

    /* update hibernation */
    maybe_inhibit_hibernation();

    if (changes.any())
    {       
        signal_torrents_changed_.emit(uniq_ids, changes);
    }
}





/**
***  Hibernate
**/

namespace
{

    auto const SessionManagerServiceName = "org.gnome.SessionManager"sv; // TODO(C++20): Use ""s
    auto const SessionManagerInterface = "org.gnome.SessionManager"sv;   // TODO(C++20): Use ""s
    auto const SessionManagerObjectPath = "/org/gnome/SessionManager"sv; // TODO(C++20): Use ""s

    bool gtr_inhibit_hibernation(guint32 &cookie)
    {               
        bool success = false;
        char const *application = "Transmission BitTorrent Client";
        char const *reason = "BitTorrent Activity";
        int const toplevel_xid = 0;
        int const flags = 4; /* Inhibit suspending the session or computer */

        try
        {
            auto const connection = Gio::DBus::Connection::get_sync(TR_GIO_DBUS_BUS_TYPE(SESSION));

            auto response = connection->call_sync(
                std::string(SessionManagerObjectPath),
                std::string(SessionManagerInterface),
                "Inhibit",
                Glib::VariantContainerBase::create_tuple({
                    Glib::Variant<Glib::ustring>::create(application),
                    Glib::Variant<guint32>::create(toplevel_xid),
                    Glib::Variant<Glib::ustring>::create(reason),
                    Glib::Variant<guint32>::create(flags),
                }),
                std::string(SessionManagerServiceName),
                1000);

            cookie = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(response.get_child(0)).get();

            /* logging */
            tr_logAddInfo(_("Inhibiting desktop hibernation"));

            success = true;
        }
        catch (Glib::Error const &e)
        {
            tr_logAddError(fmt::format(_("Couldn't inhibit desktop hibernation: {error}"), fmt::arg("error", e.what())));
        }

        return success;
    }

    void gtr_uninhibit_hibernation(guint inhibit_cookie)
    {
        try
        {
            auto const connection = Gio::DBus::Connection::get_sync(TR_GIO_DBUS_BUS_TYPE(SESSION));

            connection->call_sync(
                std::string(SessionManagerObjectPath),
                std::string(SessionManagerInterface),
                "Uninhibit",
                Glib::VariantContainerBase::create_tuple({Glib::Variant<guint32>::create(inhibit_cookie)}),
                std::string(SessionManagerServiceName),
                1000);

            /* logging */
            tr_logAddInfo(_("Allowing desktop hibernation"));
        }
        catch (Glib::Error const &e)
        {
            tr_logAddError(fmt::format(_("Couldn't inhibit desktop hibernation: {error}"), fmt::arg("error", e.what())));
        }
    }


    

} // anonymous namespace

void Session::Impl::set_hibernation_allowed(bool allowed)
{
    inhibit_allowed_ = allowed;

    //Either the "inhibit hibernation" is turned off, or there are torrents still running, and we must have have_inhibit_cookie_ set in advance 
    if (allowed && have_inhibit_cookie_)
    {   
                                // std::cout << "HIBERNATION , allowed to hibernation !!!" << std::endl;

        // Only if we have call `gtr_inhibit_hibernation` bofore, or we no need to call this. 
        // Cuz if no call to `gtr_inhibit_hibernation()`, it means hibernation is not inhibited, which means the computer was allowed to hibernate
        gtr_uninhibit_hibernation(inhibit_cookie_);
        have_inhibit_cookie_ = false;
    }

    //Neither the "inhibit hibernation" is turned off, nor there are torrents still running
    if (!allowed && !have_inhibit_cookie_ && !dbus_error_)
    {
        if (gtr_inhibit_hibernation(inhibit_cookie_))
        {
                                // std::cout << "HIBERNATION : Disallowed to hibernation ~~~ " << std::endl;
                                
            have_inhibit_cookie_ = true;
        }
        else
        {
            dbus_error_ = true;
        }
    }
}

void Session::Impl::maybe_inhibit_hibernation()
{
    auto const& sett = get_sett();

    /* hibernation is allowed if EITHER
     * (a) the "inhibit" pref is turned off OR
     * (b) there aren't any active torrents 

     * if both conditions (a) and (b) are not met,  dis-allowed computer to hibernate 
     */
    bool const hibernation_allowed = !sett.get_bool(settings_pack::TR_KEY_inhibit_desktop_hibernation) || get_active_torrent_count() == 0;

                    // std::cout << "HIBERNATION : prepare to set_hibernation_allowed =-=-=  " 
                    // << static_cast<int>(sett.get_bool(settings_pack::TR_KEY_inhibit_desktop_hibernation)) 
                    // << ", active tor cnt: "
                    // << static_cast<int>(get_active_torrent_count())
                    // << std::endl;
                    
    set_hibernation_allowed(hibernation_allowed);
}






/***********************SETTINGS-RELATED******************** */
void Session::Impl::commit_prefs_change(int const key)
{
    auto& ses = get_session();
    
    signal_prefs_changed_.emit(key);/*notify*/
    
    // tr_sessionSaveSettings(ses.session_state(lt::session_handle::save_settings));/*Serialize*/
    tr_sessionSaveSettings(ses_params_cache_);
}

void Session::set_sett_cache(int const key, bool newVal)
{
    impl_->set_sett_cache(key, newVal);
}

void Session::Impl::set_sett_cache(int const key, bool newVal)
{
    if(ses_params_cache_.settings.get_bool(key) != newVal)
    {
        ses_params_cache_.settings.set_bool(key, newVal);
        apply_settings_pack();
    }
}

void Session::set_sett_cache(int const key, int newVal)
{
    impl_->set_sett_cache(key, newVal);
}
void Session::Impl::set_sett_cache(int const key, int newVal)
{  
    if(ses_params_cache_.settings.get_int(key) != newVal)
    {   
        ses_params_cache_.settings.set_int(key, newVal);
        apply_settings_pack();
    }
}

void Session::set_sett_cache(int const key, std::string const& newVal)
{
    impl_->set_sett_cache(key, newVal);
}
void Session::Impl::set_sett_cache(int const key, std::string const& newVal)
{
    if(ses_params_cache_.settings.get_str(key) != newVal)
    {
        ses_params_cache_.settings.set_str(key, newVal);
        apply_settings_pack();
    }
}

void Session::apply_settings_pack() 
{
    impl_->apply_settings_pack();
}

//it's an asynchronous operation
//ONLY Apply non-default settings ( not the same to its default value )
void Session::Impl::apply_settings_pack()
{
    session_.apply_settings(ses_params_cache_.settings);
}

//string overload
void Session::set_pref(int const key, std::string const &newval)
{
    set_sett_cache(key, newval);
    impl_->commit_prefs_change(key);/*notify*/
    
}

//bool overload
void Session::set_pref(int const key, bool newval)
{
    set_sett_cache(key, newval);
    impl_->commit_prefs_change(key);/*notify*/
}

//int overload
void Session::set_pref(int const key, int newval)
{
    set_sett_cache(key, newval);
    impl_->commit_prefs_change(key);/*notify*/
}











//number of total torrents in GUI
size_t Session::get_torrent_count() const
{
    return impl_->get_raw_model()->get_n_items();
}



//number of Non-Paused torrents 
size_t Session::get_active_torrent_count() const
{
    return impl_->get_active_torrent_count();
}

size_t Session::Impl::get_active_torrent_count() const
{
    size_t activeCount = 0;

    for (auto i = 0U, count = raw_model_->get_n_items(); i < count; ++i)
    {
        tr_torrent_activity act = raw_model_->get_item(i)->get_activity();

                        // std::cout << "Torrent activity is: " << static_cast<int>(act) << std::endl;

        if(act != tr_torrent_activity::TR_STATUS_NULL
        && act != tr_torrent_activity::TR_STATUS_PAUSED
        && act != tr_torrent_activity::TR_STATUS_QUEUE_FOR_CHECKING_FILES
        )
        {
            ++activeCount;
        }
    }

    return activeCount;
}


Glib::RefPtr<Torrent> const& Session::find_Torrent(std::uint32_t uniq_id) 
{
    return impl_->find_Torrent(uniq_id);
}

Glib::RefPtr<Torrent> const& Session::Impl::find_Torrent(std::uint32_t uniq_id) 
{
   

    if (auto& session = get_session(); session.is_valid() )
    {
        auto j = m_uid_ih_.find(uniq_id);
        if(j != m_uid_ih_.end())
        {
            lt::info_hash_t const& tar_ih = j->second;
            lt::torrent_handle const& h = session.find_torrent(tar_ih.get());
            auto k = m_all_torrents_.find(h);
            if(k != m_all_torrents_.end())
            {
                return k->second;
            }

        }
    }
    
    //retutn empty handle means no torrent
    return empty_Tor_RefPtr_;
}


lt::torrent_handle const Session::find_torrent(std::uint32_t uniq_id) 
{
    return impl_->find_torrent(uniq_id);
}

// Find torrent running on session according to its info Hash, return by copy not by ref
lt::torrent_handle const Session::Impl::find_torrent(std::uint32_t uniq_id) 
{
    if (auto& session = get_session(); session.is_valid() )
    {
        auto j = m_uid_ih_.find(uniq_id);
        if(j != m_uid_ih_.end())
        {
            lt::info_hash_t const& tar_ih = j->second;
            lt::torrent_handle const& h = session.find_torrent(tar_ih.get());
            return h;
        }
    }
    
    //retutn empty handle means no torrent
    return empty_handle_;
}


lt::torrent_status const& Session::find_torrent_status(std::uint32_t uniq_id) 
{
    return impl_->find_torrent_status(uniq_id);
}

//wrapper
lt::torrent_status const& Session::Impl::find_torrent_status(std::uint32_t uniq_id) 
{
    
    //got torrent_handle
    auto const& handle = find_torrent(uniq_id);
    // got RefPtr<Torrent> 
    auto j = m_all_torrents_.find(handle);
    if(j != m_all_torrents_.end())
    {
        auto& Tor = j->second;
        //no copy, still origin reference!!!
        lt::torrent_status const& st = Tor->get_status_ref();
        // if(st.handle.is_valid())
        // {
            return st;
        // }
    }
    // return lt::torrent_status();
}


FaviconCache<Glib::RefPtr<Gdk::Pixbuf>>&  Session::favicon_cache() const
{
    return impl_->favicon_cache();
}


/**
 * @brief For Click to Open Selected Torrent Folder Option
 */
void Session::open_folder(std::uint32_t uniq_id)
{
    auto tor = find_torrent(uniq_id);

    if (tor.is_valid())
    {   
        std::shared_ptr<const lt::torrent_info>  ti = tor.torrent_file();
        
        bool const single = ti->num_files() == 1;
        std::string full_path  = tor.save_path();

        if (single)
        {
            gtr_open_file(full_path);
        }
        else
        {
            gtr_open_file(full_path/*Glib::build_filename(currentDir, tr_torrentName(tor))*/);
        }
    }
}



sigc::signal<void(Session::ErrorCode, Glib::ustring const &)>&  Session::signal_add_error()
{
    return impl_->signal_add_error();
}

sigc::signal<void(lt::add_torrent_params, char const*, char const*)> &Session::signal_add_prompt()
{
    return impl_->signal_add_prompt();
}



sigc::signal<void(bool)>&  Session::signal_busy()
{
    return impl_->signal_busy();
}

sigc::signal<void(int const)>& Session::signal_prefs_changed()
{
    return impl_->signal_prefs_changed();
}



sigc::signal<void(std::unordered_set<std::uint32_t> const&, Torrent::ChangeFlags)>&   Session::signal_torrents_changed()
{
    return impl_->signal_torrents_changed();
}









/*-------------------Totem-gstbt-related--------------------------*/
void Session::retrieve_btdemux_gobj(GObject* obj)
{
    impl_->retrieve_btdemux_gobj(obj);
}
void Session::Impl::retrieve_btdemux_gobj(GObject* obj)
{
   if(btdemux_gobj_ == nullptr)
   {
        btdemux_gobj_ = obj;
        //fetch torrent handle of the streaming torrent opened in Totem
        lt::torrent_handle const handle_copy = find_torrent(totem_uniq_id_);
        //pass this handle whether by copy or by ref to gstbtdemux
        btdemux_feed_playlist(btdemux_gobj_, handle_copy);

   }
}


//for update Totem BitfieldScale
lt::bitfield const& Session::totem_fetch_piece_bitfield()
{
    return impl_->totem_fetch_piece_bitfield();
}
//return ref's copy or copy's ref, ref's ref and copy's copy
lt::bitfield const& Session::Impl::totem_fetch_piece_bitfield()
{
    lt::torrent_status const& st = find_torrent_status(totem_uniq_id_);
    return st.pieces;
}



int Session::totem_get_total_num_pieces()
{
    return impl_->totem_get_total_num_pieces();
}
int Session::Impl::totem_get_total_num_pieces()
{
    //totem_uniq_id_ has been set (which means totem has opened for one of our torrent)
    if(totem_uniq_id_/*std::uint32_t*/ > 0)
    {
        //make a ref of copy
        Glib::RefPtr<Torrent> const& Tor = find_Torrent(totem_uniq_id_);
        if(Tor->is_Tor_valid())
        {
            return Tor->get_total_num_pieces();
        }
    }
}



void Session::totem_should_open(std::uint32_t id)
{
    impl_->totem_should_open(id);
}
void Session::Impl::totem_should_open(std::uint32_t id)
{
    if(totem_uniq_id_ == -1)
    {
        totem_uniq_id_ = id;
    }
}


//Stop feed remote gst_btdemux plugin(located at libgstbt.so), this happens before BaconVideoWidget destructs
void Session::btdemux_should_close()
{
    impl_->btdemux_should_close();
}
void Session::Impl::btdemux_should_close()
{
    std::cout << "Session will stop feed control loop to remote gst_btdemux plugin(located at libgstbt.so)"
    if(totem_uniq_id_ >-1)
    {
        totem_uniq_id_ = -1;
    }
}


bool Session::is_totem_active()
{
    return impl_->is_totem_active();
}

bool Session::Impl::is_totem_active()
{
    return (totem_uniq_id_ > -1);
}






