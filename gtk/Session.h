// This file Copyright © Transmission authors and contributors.
// This file is licensed under the MIT (SPDX: MIT) license,
// A copy of this license can be found in licenses/ .

#pragma once

#include "GtkCompat.h"
#include "Torrent.h"
#include "tr-transmission.h"
#include "tr-favicon-cache.h"
#include "tr-macros.h"




#include <gdkmm/pixbuf.h>
#include <giomm/file.h>
#include <giomm/listmodel.h>
#include <glibmm/object.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/treemodel.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>
#include <functional> // for std::function
#include <mutex>
#include <condition_variable> 
#include <queue>

#include "libtorrent/session.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/settings_pack.hpp"
#include "libtorrent/torrent_status.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/alert.hpp"

using namespace lt;


using tr_addtor_cb_t = std::function<std::string(lt::torrent_handle const&, Glib::RefPtr<Torrent> const&, bool&)>;

class Session : public Glib::Object
{
public:
    enum ErrorCode : uint16_t
    {
        ERR_ADD_TORRENT_ERR = 1,
        ERR_ADD_TORRENT_DUP = 2,
        ERR_NO_MORE_TORRENTS = 1000 /* finished adding a batch */
    };

    using Model = IF_GTKMM4(Gio::ListModel, Gtk::TreeModel);


public:

    ~Session() override;

    TR_DISABLE_COPY_MOVE(Session)

    static Glib::RefPtr<Session> create( lt::session_params && ses);




    void fetchTorHandleCB(tr_addtor_cb_t cb);



    void clear_move_file_err();

    lt::error_code FetchMoveFileErr();
    
    /*rename callback*/
    void FetchTorRenameResult(
        rename_done_func callback,
        void* callback_user_data
    );





    /*session-wide global speed*/
    int get_session_global_up_speed() const;

    int get_session_global_down_speed() const;




    void all_time_accum_clear();



    void all_time_accum_stats(
        std::uint64_t& all_up,
        std::uint64_t& all_dn,
        std::uint64_t& active_sec,
        std::uint64_t& open_times,
        std::uint64_t& current_sec
    );


    void only_current_stats(std::int64_t& accum_up, std::int64_t& accum_dn);




    Glib::RefPtr<Gio::ListModel> get_model() const;
    Glib::RefPtr<Model> get_sorted_model() const;

    void clear();
 
    lt::session& get_session();

    lt::settings_pack get_sett() const;

    void set_sett_cache(int const key, bool newVal);
    void set_sett_cache(int const key, int newVal);
    void set_sett_cache(int const key, std::string const& newVal);


    void apply_settings_pack();

    lt::torrent_handle const find_torrent(std::uint32_t uniq_id);

    lt::torrent_status const& find_torrent_status(std::uint32_t uniq_id);

    Glib::RefPtr<Torrent> const& find_Torrent(std::uint32_t uniq_id);

    std::uint32_t get_its_uniq_id() const;

    size_t get_active_torrent_count() const;

    size_t get_torrent_count() const;

    int get_upload_rate_limit() const;

    int get_download_rate_limit() const;

    FaviconCache<Glib::RefPtr<Gdk::Pixbuf>>& favicon_cache() const;

  
    /**
     * Load saved state and return number of torrents added.
     * May trigger one or more "error" signals with ERR_ADD_TORRENT
     */
    void load(bool force_paused);

    

    void add_files(std::vector<Glib::RefPtr<Gio::File>> const& files, bool do_prompt);


    bool add_from_url(Glib::ustring const& url);



    void add_impl(lt::add_torrent_params && atp, const char* dottor_path, const char* down_dir);


    void torrents_added();


    void torrent_changed(std::uint32_t id);



    /* remove a torrent */
    void remove_torrent(lt::torrent_handle const& handle, bool delete_files);



    /* update the model with current torrent status */
    void update();

    /**
     * Attempts to start a torrent immediately.
     */
    // void start_now(std::uint32_t id);


    
    /*-----Totem-gstbt-related-----*/
    void retrieve_btdemux_gobj(GObject* obj);
    lt::bitfield const& totem_fetch_piece_bitfield();
    int totem_get_total_num_pieces();
    void totem_should_open(std::uint32_t id);
    void btdemux_should_close();
    bool is_totem_active();



    /**
    ***  Set a preference *settings_pack* value, save the prefs file, and emit the "prefs-changed" signal
    **/
    void set_pref(int const key, std::string const& val);
    void set_pref(int const key, bool val);
    void set_pref(int const key, int val);

    bool is_session_valid();

    void open_folder(std::uint32_t uniq_id);
    
    void close();

    void inc_num_outstanding_resume_data();

    sigc::signal<void(ErrorCode, Glib::ustring const&)>& signal_add_error();
    sigc::signal<void(lt::add_torrent_params, char const*, char const*)>& signal_add_prompt();
    sigc::signal<void(std::unordered_set<std::uint32_t> const&, Torrent::ChangeFlags)>& signal_torrents_changed();
    sigc::signal<void(bool)>& signal_busy();
    sigc::signal<void(int const)>& signal_prefs_changed();

        
protected:
        explicit Session(lt::session_params && sp);

private:
        class Impl;
        std::unique_ptr<Impl> const impl_;
};



struct accum_stats 
{
    accum_stats(std::time_t now):start_time_(now)
    {}

    void zerofy()
    {
        start_time_ = std::time(nullptr);
        uploadedBytes_ = {}; 
        downloadedBytes_ = {};
        sessionCount_ = {};
        secondsActive_ = {}; 
    }

    void inject(settings_pack const& sett)
    {
        std::int64_t up_bytes_high = sett.get_int(settings_pack::TR_KEY_uploadedBytes_high32bit);
        std::int64_t up_bytes_low = sett.get_int(settings_pack::TR_KEY_uploadedBytes_low32bit);
        uploadedBytes_ = (up_bytes_high << 32) | up_bytes_low;

        std::int64_t dn_bytes_high = sett.get_int(settings_pack::TR_KEY_downloadedBytes_high32bit);
        std::int64_t dn_bytes_low = sett.get_int(settings_pack::TR_KEY_downloadedBytes_low32bit);
        downloadedBytes_ = (dn_bytes_high << 32) | dn_bytes_low;

        secondsActive_ = sett.get_int(settings_pack::TR_KEY_secondsActive);
        sessionCount_ = sett.get_int(settings_pack::TR_KEY_sessionCount);
    }

    void offer(std::uint64_t& all_up,
            std::uint64_t& all_dn,
            std::uint64_t& active_sec,
            std::uint64_t& open_times,
            std::uint64_t& current_sec)
    {
        all_up = uploadedBytes_;
        all_dn = downloadedBytes_;
        active_sec = secondsActive_;
        open_times = sessionCount_;
        current_sec = std::time(nullptr) - start_time_;
    } 

    void inc_download_bytes(std::uint64_t bytes)
    {
        downloadedBytes_ += bytes;
    }


    void inc_upload_bytes(std::uint64_t bytes)
    {
        uploadedBytes_ += bytes;
    }



    void dump(lt::settings_pack& sett)
    {
        int up_high{},up_low{};
        get_upload_bytes(up_high, up_low);
        sett.set_int(settings_pack::TR_KEY_uploadedBytes_high32bit, up_high);
        sett.set_int(settings_pack::TR_KEY_uploadedBytes_low32bit, up_low);

        int dn_high{},dn_low{};
        get_download_bytes(dn_high, dn_low);
        sett.set_int(settings_pack::TR_KEY_downloadedBytes_high32bit, dn_high);
        sett.set_int(settings_pack::TR_KEY_downloadedBytes_low32bit, dn_low);
        sett.set_int(settings_pack::TR_KEY_sessionCount, get_open_times());
        sett.set_int(settings_pack::TR_KEY_secondsActive, get_active_seconds());
    }

    private:

        void get_upload_bytes(int& high, int& low)
        {
            high = (uploadedBytes_ >> 16);
            low = (uploadedBytes_ & 0x0000ffff);
        }

        void get_download_bytes(int& high, int& low)
        {
            high = (downloadedBytes_ >> 16);
            low = (downloadedBytes_ & 0x0000ffff);
        }

        int get_open_times()
        {
            ++sessionCount_;
            if(sessionCount_ > std::numeric_limits<int>::max())
            {
                return 0;
            }
            return sessionCount_;
        }

        int get_active_seconds()
        {
            secondsActive_ += (std::time(nullptr) - start_time_);
            if(secondsActive_ > std::numeric_limits<int>::max())
            {
                return 0;
            }
            return secondsActive_;
        }

        std::time_t start_time_;
        std::uint64_t uploadedBytes_ = {}; /* total up */
        std::uint64_t downloadedBytes_ = {}; /* total down */
        std::uint64_t sessionCount_ = {}; /* program started N times */
        std::uint64_t secondsActive_ = {}; /* how long Transmission's been running */
};





class AlertQueue
{
public:
    void push(std::vector<lt::alert*> alerts)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.emplace(std::move(alerts));
        processing_complete_ = false;  // Indicate that new alerts are available and not processed yet
        cond_var_.notify_one();  // Notify waiting thread that new alerts are available
    }

    std::vector<lt::alert*> pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty() || stop_processing_; });

        if (stop_processing_ && queue_.empty()) {
            // If we're stopping and the queue is empty, return an empty vector
            return {};
        }

        auto alerts = std::move(queue_.front());
        queue_.pop();
        processing_complete_ = false;  // Set processing state to false before processing
        return alerts;
    }

    void signal_processing_complete()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        processing_complete_ = true;
        cond_var_.notify_one();  // Notify that processing is complete
    }

    void stop_processing()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_processing_ = true;
        cond_var_.notify_all();  // Notify all waiting threads to stop processing
    }

    bool is_processing_complete() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return processing_complete_;
    }

    bool processing_stopped() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stop_processing_;
    }
    
private:
    std::queue<std::vector<lt::alert*>> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    bool processing_complete_ = true;  // Track whether the queue has been processed
    bool stop_processing_ = false;     // Flag to stop processing
};

