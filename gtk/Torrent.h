// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include "Flags.h"
#include "tr-transmission.h"
#include "tr-values.h"




#include <giomm/icon.h>
#include <glibmm/extraclassinit.h>
#include <glibmm/object.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/treemodelcolumn.h>

// #include <glib.h>
// #include <glib-object.h> 

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <initializer_list>
#include <memory>


#include "libtorrent/torrent_flags.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/torrent_status.hpp"
#include "libtorrent/info_hash.hpp"
#include "libtorrent/announce_entry.hpp"
#include "libtorrent/peer_info.hpp"

using namespace lt;

class Percents;

class Torrent : public Glib::ExtraClassInit, public Glib::Object
{
public:

    /********************COLUMN************************* */
    //Used for ListModelAdapter
    class Columns : public Gtk::TreeModelColumnRecord
    {
        public:
            Columns();

            Gtk::TreeModelColumn<Torrent*> self;
            Gtk::TreeModelColumn<Glib::ustring> name_collated;
    };
    /**************************************************** */



    /*which category has changed since last time, enum has more than 8 items, use uint32_t instead of uint8_t  */
    enum class ChangeFlag : uint32_t /*uint8_t*/
    {
        NAME,
        TOTAL_SIZE,
        SPEED_UP,
        SPEED_DOWN,
        SAVE_PATH,
        ADDED_DATE,
        LONG_PROGRESS,
        ETA,
        NUM_PIECES,
        MIME_TYPE,
        HAS_METADATA,
        QUEUE_POSITION,
        PRIORITY,
        IS_FINISHED,
        IS_SEEDING,
        IS_QUEUED,
        IS_PAUSED,
        PERCENT_DONE,
        TORRENT_STATE,
        NUM_PEERS,
        NUM_TOTAL_PIECES,
        NUM_PIECES_WE_HAVE,
        NUM_CONN,
        UPLOAD_TOTAL,
        DOWNLOAD_TOTAL,
        ERROR_CODE,

        TRACKERS_HASH

    };

    using ChangeFlags = Flags<ChangeFlag>;

public:
    using Speed = libtransmission::Values::Speed;
    using Storage = libtransmission::Values::Storage;




    int get_active_peer_count() const;



    tr_torrent_activity get_activity() const;
    time_t get_added_date() const;
    lt::error_code get_error_code() const;
    // Glib::ustring const& get_error_message() const;
    time_t get_eta() const;
    bool get_finished() const;



    std::string get_ih_str_form() const;

    //info hash of the torrent 
    lt::info_hash_t get_ih() const;

    //an integer identifier
    std::uint32_t get_uniq_id() const;
    


    Glib::ustring const& get_name_collated() const;
    Glib::ustring get_name() const;
    Percents get_percent_done() const;
    float get_percent_done_fraction() const;
    lt::download_priority_t  get_priority() const;
    size_t get_queue_position() const;
    Speed get_speed_down() const;
    Speed get_speed_up() const;


    Storage get_total_size() const;



    Glib::RefPtr<Gio::Icon> get_icon() const;
    Glib::ustring get_short_status_text() const;
    Glib::ustring get_long_progress_text() const;
    Glib::ustring get_long_status_text() const;
    bool get_sensitive() const;
    std::vector<Glib::ustring> get_css_classes() const;

    ChangeFlags update();

    static Glib::RefPtr<Torrent> create(lt::torrent_handle const& handle);




    static Columns const& get_columns();
    static std::uint32_t get_item_id(Glib::RefPtr<Glib::ObjectBase const> const& item);
    static void get_item_value(Glib::RefPtr<Glib::ObjectBase const> const& item, int column, Glib::ValueBase& value);


    static int compare_by_uniq_id(Glib::RefPtr<Torrent const> const& lhs, Glib::RefPtr<Torrent const> const& rhs);
    static bool less_by_uniq_id(Glib::RefPtr<Torrent const> const& lhs, Glib::RefPtr<Torrent const> const& rhs);




    void total_updown_delta_at_remove(std::int64_t& up, std::int64_t& down);

    void feed_total_updown_at_load(std::int64_t up, std::int64_t dn);


    lt::torrent_handle const& get_handle_ref() const;

    lt::torrent_status& get_status_ref();
    std::mutex& fetch_tor_status_mutex();

    std::vector<lt::announce_entry>& get_trackers_vec_ref();

    std::vector<lt::announce_entry>const& get_trackers_vec_const_ref() const;

    std::vector<lt::peer_info>& get_peers_vec_ref();

    std::vector<std::int64_t>& get_file_progress_vec_ref();



    bool has_metadata();

    bool is_Tor_valid() const;

    void saveMagnetLink(std::string && ml);

    std::string const& fetchMagnetLink() const;

    //use when remove or delete torrent, both on gtk-GUI and on libtorrent internal
    void disconnect_signals();

    //make it public for empty RefPtr<Torrent>
    Torrent();
private:
    explicit Torrent(lt::torrent_handle const& torrent);

    ~Torrent();
private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
};

DEFINE_FLAGS_OPERATORS(Torrent::ChangeFlag)
