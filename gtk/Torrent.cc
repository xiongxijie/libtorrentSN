// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "Torrent.h"
#include "DynamicPropertyStore.h"
#include "IconCache.h"
#include "Percents.h"
#include "Utils.h"

#include "tr-transmission.h"
#include "tr-utils.h"
#include "tr-values.h"


#include <glibmm/i18n.h>
#include <glibmm/value.h>
#include <glibmm/main.h>


#include <fmt/core.h>

#include <array>
#include <cmath>
#include <utility>
#include <cstdint>
#include <iostream>
#include <mutex>


#include "libtorrent/announce_entry.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/torrent_status.hpp"
#include "libtorrent/torrent_flags.hpp"
#include "libtorrent/peer_info.hpp"
#include "libtorrent/download_priority.hpp"
#include "libtorrent/error_code.hpp"




using namespace lt;

using namespace std::string_view_literals;

using namespace libtransmission::Values;


namespace
{

template<typename T>
Glib::Value<T>& column_value_cast(Glib::ValueBase& value, Gtk::TreeModelColumn<T> const& /*column*/)
{
    return static_cast<Glib::Value<T>&>(value);
}

template<typename T, typename U, typename = std::enable_if_t<!std::is_floating_point_v<T>>>
void update_cache_value(T& value, U&& new_value, Torrent::ChangeFlags& changes, Torrent::ChangeFlag flag)
{
    using std::rel_ops::operator!=;

    if (value != new_value)
    {   
        value = std::forward<U>(new_value);
        changes.set(flag);
    }
}

template<typename T, typename U, typename = std::enable_if_t<std::is_floating_point_v<T>>>
void update_cache_value(T& value, U new_value, T epsilon, Torrent::ChangeFlags& changes, Torrent::ChangeFlag flag)
{
    if (std::fabs(value - new_value) >= epsilon)
    {
        value = new_value;
        changes.set(flag);
    }
}

/*used for check whether trackers list has changed */
unsigned int build_torrent_trackers_hash(std::vector<lt::announce_entry>& tracker_vec)
{
    auto hash = std::uint64_t(0);

    for (auto i = size_t(0), n = tracker_vec.size(); i < n; ++i)
    {
        for (auto const ch : std::string_view{ tracker_vec[i].url })
        {
            hash = (hash << 4U) ^ (hash >> 28U) ^ static_cast<unsigned char>(ch);
        }
    }

    return hash;
}


std::string_view get_mime_type(lt::torrent_handle const& torr)
{
    
    auto ti = torr.torrent_file();
    
    auto n_files = ti ? ti->num_files(): 0;

    if (n_files == 0)
    {
        return UnknownMimeType;
    }

    if (n_files > 1)
    {
        return DirectoryMimeType;
    }

    auto const tor_name = std::string_view(ti->name().c_str());

    return tor_name.find('/') != std::string_view::npos ? DirectoryMimeType : tr_get_mime_type_for_filename(tor_name);
}


std::string_view get_activity_direction(tr_torrent_activity activity)
{
    switch (activity)
    {
    case TR_STATUS_DOWNLOAD:
    case TR_STATUS_QUEUE_FOR_DOWNLOAD:
    case TR_STATUS_DOWNLOAD_METADATA:
    case TR_STATUS_QUEUE_FOR_DOWNLOAD_METADATA:
        return "down"sv;
    case TR_STATUS_SEED:
    case TR_STATUS_QUEUE_FOR_SEED:
        return "up"sv;
    default:
        return "idle"sv;
    }
}

tr_torrent_activity translate_state_to_enum(lt::torrent_status const& s)
{
    std::uint16_t i = {};

    i = s.state;
    if ((s.flags & lt::torrent_flags::paused))	
    {
        if(s.flags & lt::torrent_flags::auto_managed)
        {
            i = i + 10;
        }
        else if(!(s.flags & lt::torrent_flags::auto_managed))
        {
            i = 0;
        }
    }

    return static_cast<tr_torrent_activity>(i);
}

} //anonymous namespace



/****************************** */
Torrent::Columns::Columns()
{
    add(self);
    add(name_collated);
}
/****************************** */



class Torrent::Impl
{
public:
    enum class Property : guint
    {
        ICON = 1,
        NAME,
        PERCENT_DONE,
        SHORT_STATUS,
        LONG_PROGRESS,
        LONG_STATUS,
        SENSITIVE,
        CSS_CLASSES,

        N_PROPS
    };


    static lt::torrent_handle empty_handle_;

    using PropertyStore = DynamicPropertyStore<Torrent, Property>;

    struct Cache
    {
        Glib::ustring name_collated;
        
        /*torrent name*/
        Glib::ustring name;
        /*save path*/
        Glib::ustring save_path;
        
        /*torrent added-time*/
        std::time_t added_time= {};
        /*total size of torrent to download, maybe less `than torrent-full-size`*/
        Storage total_size;
        /*the full size of all files of this torrent*/
        Storage complete_size_total;
        /*upload rate bytes per sec*/
        Speed speed_up;
        /*download rate bytes per sec*/
        Speed speed_down;
        /*percent progress*/
        Percents percent_done;
        /*eta -remaining time*/
        std::time_t eta= {};

        Storage all_time_download;
        Storage all_time_upload;
        Storage total_done;

        int num_pieces_have= {};
        int total_num_pieces= {};
        int num_peers= {};
        int num_connections = {};
        int num_seeders= {};
        int num_leechers= {};
        /*num of unchoked peers*/
        int num_uploads= {};
        /*state of torrent*/
        tr_torrent_activity tor_state= tr_torrent_activity::TR_STATUS_NULL;
        bool is_queued = {}; 
        bool is_paused = {};
        int priority ={};
        std::size_t queue_pos = {};
        bool has_metadata = {};
        std::string_view mime_type;
        bool is_finished = {};
        bool is_seeding = {};
        lt::error_code err_code;

        unsigned int trackers_hash = {};

    };

public:
    Impl(Torrent& torrent, lt::torrent_handle const& handle);

    ~Impl();

    std::uint32_t get_uniq_id() const;

    lt::info_hash_t get_ih() const;

    std::string get_ih_str_form() const;

    
    lt::torrent_handle const& get_handle_ref() const;

    lt::torrent_status& get_status_ref();

    std::vector<lt::announce_entry>& get_trackers_vec_ref();

    std::vector<lt::announce_entry>const& get_trackers_vec_const_ref() const;

    std::vector<lt::peer_info>& get_peers_vec_ref();

    std::vector<std::int64_t>& get_file_progress_vec_ref();
    
    bool is_status_valid() const;

    bool is_Tor_valid() const;

    bool has_metadata();

    void saveMagnetLink(std::string && ml);

    std::string const& fetchMagnetLink() const;

    bool post_for_torrent();

    void feed_total_updown_at_load(std::int64_t up, std::int64_t dn);

    void total_updown_delta_at_remove(std::int64_t& up, std::int64_t& down);
    
    /*Totem-gstbt related*/
    int get_total_num_pieces();


    lt::torrent_handle const& get_raw_torrent()
    {
        return raw_torrent_;
    }

    Cache& get_cache()
    {
        return cache_;
    }

    ChangeFlags update_cache();

    std::mutex& fetch_tor_status_mutex();

    void disconnect_signals();

    void notify_property_changes(ChangeFlags changes) const;

    void get_value(int column, Glib::ValueBase& value) const;

    [[nodiscard]] Glib::RefPtr<Gio::Icon> get_icon() const;
    [[nodiscard]] Glib::ustring get_short_status_text() const;
    [[nodiscard]] Glib::ustring get_long_progress_text() const;
    [[nodiscard]] Glib::ustring get_long_status_text() const;
    [[nodiscard]] std::vector<Glib::ustring> get_css_classes() const;

    static void class_init(void* cls, void* user_data);

private:
    [[nodiscard]] Glib::ustring get_short_transfer_text() const;
    [[nodiscard]] Glib::ustring get_error_text() const;
    [[nodiscard]] Glib::ustring get_activity_text() const;

private:

    Torrent& torrent_;
    
    //depends-on
    lt::torrent_handle const& raw_torrent_;

    lt::info_hash_t ih_;

    //its a big value , such as 4,293,656,597, it is integer representation of torrent obj's memory address
    std::uint32_t uniq_id_;



    std::string magnet_link_ = {};

    lt::torrent_status tor_status_;
    std::mutex status_mutex_;

    std::vector<std::int64_t> files_progress_vec_;

    std::vector<lt::announce_entry> trackers_vec_;

    std::vector<lt::peer_info> peers_vec_;
    
    //do post-cycle from libtorrent : post_file_progress,post_peer_info,post_trackers,others are bit hard to implement now.
    sigc::connection post_torrent_timer_;

    Cache cache_;

    std::int64_t total_upload_when_load_ = {};
    std::int64_t total_download_when_load_ = {};



};


lt::torrent_handle Torrent::Impl::empty_handle_ = lt::torrent_handle();


Torrent::~Torrent()
{
            std::cout << "Torrent::~Torrent" << std::endl;
}
Torrent::Impl::~Impl()
{
            std::cout << "Torrent::Impl::~Impl" << std::endl;
    if(post_torrent_timer_.connected())
    {
        post_torrent_timer_.disconnect();
    }
}




bool Torrent::Impl::post_for_torrent()
{
    if(!raw_torrent_.is_valid())
    {
        return false;
    }
    
    /*post for peer info */
    raw_torrent_.post_peer_info();
    /*post for tracker info*/
    raw_torrent_.post_trackers();
    /*post for file progress*/
    raw_torrent_.post_file_progress({});
    
    /*post for piece availability*/
    // raw_torrent_.post_piece_availability();
    /*post for download queue*/
    // raw_torrent_.post_download_queue();


    // Return true to continue firing the timeout, false to stop it
    return true;
}

bool Torrent::Impl::is_status_valid() const
{
    if(tor_status_.handle.is_valid())
        return true;
    return false;
}


std::vector<std::int64_t>& Torrent::get_file_progress_vec_ref()
{
    return impl_->get_file_progress_vec_ref();

}
std::vector<std::int64_t>& Torrent::Impl::get_file_progress_vec_ref()
{
    return files_progress_vec_;
}




//******returning a non-const reference to a private member
//This allows the caller to read or write tor_status_ directly
lt::torrent_status& Torrent::get_status_ref()
{
    return impl_->get_status_ref();

}
lt::torrent_status& Torrent::Impl::get_status_ref()
{
    // if(tor_status && tor_status_.handle.is_valid())
        return tor_status_;
    // return lt::torrent_status{};
}




// std::string to_hex(lt::sha1_hash const& s)
// {
// 	std::stringstream ret;
// 	ret << s;
// 	return ret.str();
// }


lt::info_hash_t Torrent::get_ih() const
{
    return impl_->get_ih();

}

lt::info_hash_t Torrent::Impl::get_ih() const
{
    return ih_;

}


std::string Torrent::get_ih_str_form() const
{
    return impl_->get_ih_str_form();

}

std::string Torrent::Impl::get_ih_str_form() const
{
    lt::sha1_hash const& s1 = ih_.get();
    std::stringstream ret;
    ret << s1;
    return ret.str();

}



std::uint32_t Torrent::get_uniq_id() const
{
    return impl_->get_uniq_id();

}



std::uint32_t Torrent::Impl::get_uniq_id() const
{
    return uniq_id_;
}



lt::torrent_handle const& Torrent::get_handle_ref() const
{
    return impl_->get_handle_ref();

}


lt::torrent_handle const& Torrent::Impl::get_handle_ref() const
{ 
    if(raw_torrent_.is_valid())
    {
        return raw_torrent_;
    }
    return empty_handle_;
}



std::vector<lt::announce_entry>const& Torrent::get_trackers_vec_const_ref() const
{
    return impl_->get_trackers_vec_const_ref();
}
std::vector<lt::announce_entry>const& Torrent::Impl::get_trackers_vec_const_ref() const
{
    return trackers_vec_;
}

std::vector<lt::announce_entry>& Torrent::get_trackers_vec_ref()
{
    return impl_->get_trackers_vec_ref();
}

std::vector<lt::announce_entry>& Torrent::Impl::get_trackers_vec_ref()
{
    return trackers_vec_;
}



std::vector<lt::peer_info>& Torrent::get_peers_vec_ref()
{
    return impl_->get_peers_vec_ref();
}

std::vector<lt::peer_info>& Torrent::Impl::get_peers_vec_ref()
{
    return peers_vec_;
}

bool Torrent::is_Tor_valid() const
{
    return impl_->is_Tor_valid();
}


bool Torrent::Impl::is_Tor_valid() const
{
    return raw_torrent_.is_valid();
}

bool Torrent::has_metadata()
{
    return impl_->has_metadata();
}
bool Torrent::Impl::has_metadata()
{
    auto& st = get_status_ref();
    if(st.handle.is_valid() && st.has_metadata)
        return true;
    return false;
}



namespace
{

    std::string sha1_to_hex_str(lt::sha1_hash const& s)
    {
        std::stringstream ret;
        ret << s;
        return ret.str();
    }

}//anonymous namespace



Torrent::ChangeFlags Torrent::Impl::update_cache()
{ 
    /*initialize changeFlags*/
    auto result = ChangeFlags();

    std::lock_guard<std::mutex> lock(status_mutex_);  // Lock the mutex

    auto const& st = get_status_ref();// This will be safe as it is within the lock

    if(!st.handle.is_valid())
    {
        return result;
    }

    // g_return_val_if_fail(st.handle.is_valid()==true, Torrent::ChangeFlags());
                // std::cout << "start v update_cache"  << std::endl;

    //only those who change since last time will be included in ChangeFlags
    /*added time*/
    update_cache_value(cache_.added_time, st.added_time, result, ChangeFlag::ADDED_DATE);


    /*total size of this torrent*/
    update_cache_value(cache_.total_size, Storage{ st.total, Storage::Units::Bytes }, result, ChangeFlag::TOTAL_SIZE);


    /*save_path*/
    update_cache_value(cache_.save_path, st.save_path.c_str(), result, ChangeFlag::SAVE_PATH);


    /*torrent name*/
    update_cache_value(cache_.name, st.name.c_str(), result, ChangeFlag::NAME);


    /*upload rate*/
    update_cache_value(cache_.speed_up, Speed{ st.upload_rate, Speed::Units::Byps },result,ChangeFlag::SPEED_UP);


    /*dwonload rate -- bytes per second*/
    update_cache_value(cache_.speed_down, Speed{ st.download_rate, Speed::Units::Byps },result, ChangeFlag::SPEED_DOWN);


    /*progress - percent complete*/
    update_cache_value(cache_.percent_done, Percents(st.progress), result, ChangeFlag::PERCENT_DONE);


    /*eta -- second*/
    auto const bytes_progress = double(st.progress_ppm)	/ 1000000. * double(st.total);
    auto eta = (st.total - bytes_progress) / double(st.download_rate);
    update_cache_value(cache_.eta, eta, result, ChangeFlag::ETA);


    /*total we've downloaded */
    update_cache_value(cache_.all_time_download, Storage{ st.all_time_download, Storage::Units::Bytes },result,ChangeFlag::DOWNLOAD_TOTAL);


    update_cache_value(cache_.total_done, Storage{ st.total_done, Storage::Units::Bytes },result,ChangeFlag::DOWNLOAD_TOTAL);

    /*total_we've uploaded*/
    update_cache_value(cache_.all_time_upload, Storage{ st.all_time_upload, Storage::Units::Bytes },result,ChangeFlag::UPLOAD_TOTAL);


    /*num_pieces we have*/
    update_cache_value(cache_.num_pieces_have, st.num_pieces, result, ChangeFlag::NUM_PIECES_WE_HAVE);
    
  
    /*total pieces*/
    std::shared_ptr<const lt::torrent_info> ti = st.torrent_file.lock();
	int const total_num_pieces = ti && ti->is_valid() ? ti->num_pieces() : 0;
    update_cache_value(cache_.total_num_pieces, total_num_pieces, result, ChangeFlag::NUM_TOTAL_PIECES);

	int const ttl_sz = ti && ti->is_valid() ? ti->total_size() : 0;

    update_cache_value(cache_.complete_size_total, Storage{ ttl_sz, Storage::Units::Bytes }, result, ChangeFlag::TOTAL_SIZE);

    /*num peers we connect to*/
    update_cache_value(cache_.num_peers, st.num_peers, result, ChangeFlag::NUM_PEERS);

    update_cache_value(cache_.num_connections, st.num_connections, result, ChangeFlag::NUM_CONN);


    /*num of seeders*/
    // update_cache_value(cache_.num_seeders, st.num_seeds, result, ChangeFlag::xxx);
    /*num of leechers*/
    // update_cache_value(cache_.num_leechers, st.num_peers-st.num_seeds, result, ChangeFlag::xxx);
    /*num of unchoked peers*/
    // update_cache_value(cache_.num_uploads, st.num_uploads, result, ChangeFlag::xx);


    /*state_t state */
    update_cache_value(cache_.tor_state, translate_state_to_enum(st), result, ChangeFlag::TORRENT_STATE);
            // std::cout << "in update cache, tor acticvity is : " << static_cast<int>(cache_.tor_state) << std::endl;
    /*priority of this torrent*/
    update_cache_value(cache_.priority, st.priority, result, ChangeFlag::PRIORITY);

    /*queue position*/
    update_cache_value(cache_.queue_pos, st.queue_position, result, ChangeFlag::QUEUE_POSITION);

    /*whether has metadata*/
    update_cache_value(cache_.has_metadata, st.has_metadata, result, ChangeFlag::HAS_METADATA);

    /*mime-type for icon*/
    update_cache_value(cache_.mime_type, get_mime_type(get_handle_ref()), result, ChangeFlag::MIME_TYPE);

    /*is finished*/
    update_cache_value(cache_.is_finished, st.is_finished, result, ChangeFlag::IS_FINISHED);

    /*is seeding -- have all pieces*/
    update_cache_value(cache_.is_seeding, st.is_seeding ,result,ChangeFlag::IS_SEEDING);

    /*error_code*/
    update_cache_value(cache_.err_code, st.errc, result, ChangeFlag::ERROR_CODE);

    /*is queued */
    bool const is_q =  ((st.flags & lt::torrent_flags::auto_managed) && (st.flags & lt::torrent_flags::paused));
    update_cache_value(cache_.is_queued, is_q, result, ChangeFlag::IS_QUEUED);

    /*is paused*/
    bool const is_pause = st.flags & lt::torrent_flags::paused;
    update_cache_value(cache_.is_paused, is_pause, result, ChangeFlag::IS_PAUSED);

    /*trackers hash*/
    auto& tracker_vec = get_trackers_vec_ref();
    if(!tracker_vec.empty())
    {
        update_cache_value(cache_.trackers_hash,  build_torrent_trackers_hash(tracker_vec), result, ChangeFlag::TRACKERS_HASH);
    }

    if (result.test(ChangeFlag::NAME))
    {
        cache_.name_collated = fmt::format("{}\t{}", cache_.name.lowercase(), sha1_to_hex_str(st.info_hashes.get()));
    }

    return result;

}

void Torrent::Impl::notify_property_changes(ChangeFlags changes) const
{
    // Updating the model triggers off resort/refresh, so don't notify unless something's actually changed
    if (changes.none())
    {
        return;
    }

#if GTKMM_CHECK_VERSION(4, 0, 0)

    static auto constexpr properties_flags = std::array<std::pair<Property, ChangeFlags>, PropertyStore::PropertyCount - 1>({ {
        { Property::ICON, ChangeFlag::MIME_TYPE },
        { Property::NAME, ChangeFlag::NAME },
        { Property::PERCENT_DONE, ChangeFlag::PERCENT_DONE },
        { Property::SHORT_STATUS,
          ChangeFlag::ACTIVE_PEERS_DOWN | ChangeFlag::ACTIVE_PEERS_UP | ChangeFlag::ACTIVITY | ChangeFlag::FINISHED |
              ChangeFlag::RATIO | ChangeFlag::RECHECK_PROGRESS | ChangeFlag::SPEED_DOWN | ChangeFlag::SPEED_UP },
        { Property::LONG_PROGRESS,
          ChangeFlag::ACTIVITY | ChangeFlag::ETA | ChangeFlag::LONG_PROGRESS | ChangeFlag::PERCENT_COMPLETE |
              ChangeFlag::PERCENT_DONE | ChangeFlag::RATIO | ChangeFlag::TOTAL_SIZE },
        { Property::LONG_STATUS,
          ChangeFlag::ACTIVE_PEERS_DOWN | ChangeFlag::ACTIVE_PEERS_UP | ChangeFlag::ACTIVITY | ChangeFlag::ERROR_CODE |
              ChangeFlag::ERROR_MESSAGE | ChangeFlag::HAS_METADATA | ChangeFlag::LONG_STATUS | ChangeFlag::SPEED_DOWN |
              ChangeFlag::SPEED_UP | ChangeFlag::STALLED },
        { Property::SENSITIVE, ChangeFlag::ACTIVITY },
        { Property::CSS_CLASSES, ChangeFlag::ACTIVITY | ChangeFlag::ERROR_CODE },
    } });

    auto& properties = PropertyStore::get();

    for (auto const& [property, flags] : properties_flags)
    {
        if (changes.test(flags))
        {
            properties.notify_changed(torrent_, property);
        }
    }

#else

    // Reduce redraws by emitting non-detailed signal once for all changes
    gtr_object_notify_emit(torrent_);

#endif
}







Glib::RefPtr<Gio::Icon> Torrent::Impl::get_icon() const
{
    return gtr_get_mime_type_icon(cache_.mime_type);
}


//for compact view
Glib::ustring Torrent::Impl::get_short_status_text() const
{
    switch (cache_.tor_state)
    {
        case TR_STATUS_PAUSED:
            return cache_.is_finished ? _("Finished") : _("Paused");
        
        case TR_STATUS_FINISHED:
            return _("Finished");

        case TR_STATUS_QUEUE_FOR_CHECKING_FILES:
            return _("Queued for checking");

        case TR_STATUS_QUEUE_FOR_CHECKING_RESUME_DATA:
            return _("Queued for check resume");

        case TR_STATUS_QUEUE_FOR_DOWNLOAD:
            return _("Queued for download");

        case TR_STATUS_QUEUE_FOR_DOWNLOAD_METADATA:
            return _("Queued for dl metadata");

        case TR_STATUS_QUEUE_FOR_SEED:
            return _("Queued for seeding");

        case TR_STATUS_CHECKING_RESUME_DATA:
            return fmt::format(
                // xgettext:no-c-format
                _("Checking resume data ({percent_done}% checked)"),
                fmt::arg("percent_done", cache_.percent_done.to_string()));

         case TR_STATUS_CHECKING_FILES:
            return fmt::format(
                // xgettext:no-c-format
                _("Checking files ({percent_done}% checked)"),
                fmt::arg("percent_done", cache_.percent_done.to_string()));

        case TR_STATUS_DOWNLOAD:
        case TR_STATUS_SEED:
            return fmt::format(
            "{:s}",
                get_short_transfer_text()
            );

        default:
            return {};
    }
}

/*eg. 5.35MB of 1.06GB (0.51%), uploaded None*/
Glib::ustring Torrent::Impl::get_long_progress_text() const
{
    Glib::ustring gstr;

    bool const isDone = cache_.is_finished;
    auto const haveTotal = cache_.total_done;
    bool const isSeed = cache_.is_seeding;

    if (!isDone) // downloading
    {
        // 50 MB of 200 MB (25%)
        gstr += fmt::format(
            _("{current_size} of {complete_size} ({percent_done}%)"),
            fmt::arg("current_size", tr_strlsize(haveTotal)),
            fmt::arg("complete_size", tr_strlsize(cache_.total_size)),
            fmt::arg("percent_done", cache_.percent_done.to_string()));
    }

    else if (isDone && !isSeed) // partial seed
    {
        gstr += fmt::format(
            // xgettext:no-c-format
            _("{current_size} of {complete_size} ({percent_complete}%), uploaded {uploaded_size} "),
            fmt::arg("current_size", tr_strlsize(haveTotal)),
            fmt::arg("complete_size", tr_strlsize(cache_.total_size)),
            fmt::arg("percent_complete", cache_.percent_done.to_string()),
            fmt::arg("uploaded_size", tr_strlsize(cache_.all_time_upload))
            );
    }

    else if(isDone && isSeed) // seed
    {
        gstr += fmt::format(
            _("{complete_size}, uploaded {uploaded_size} "),
            fmt::arg("complete_size", tr_strlsize(cache_.total_size)),
            fmt::arg("uploaded_size", tr_strlsize(cache_.all_time_upload))
            );
    }

    // add time remaining when applicable
    if (cache_.tor_state == TR_STATUS_DOWNLOAD || (cache_.tor_state == TR_STATUS_SEED))
    {
        gstr += " - ";

        if (cache_.eta < 0)
        {
            gstr += _("Remaining time unknown");
        }
        else
        {
            gstr += tr_format_time_left(cache_.eta);
        }
    }

    return gstr;
}



/*eg. Downloading from x of y connected peers - 2.56MB/s▼ 49KB/s▲ */
Glib::ustring Torrent::Impl::get_long_status_text() const
{
    auto status_str = get_error_text();
    if (status_str.empty())
    {
        status_str = get_activity_text();
    }

    switch (cache_.tor_state)
    {
        case TR_STATUS_PAUSED:
        case TR_STATUS_QUEUE_FOR_CHECKING_FILES:
        case TR_STATUS_QUEUE_FOR_CHECKING_RESUME_DATA:
        case TR_STATUS_CHECKING_FILES:
        case TR_STATUS_QUEUE_FOR_DOWNLOAD:
        case TR_STATUS_QUEUE_FOR_DOWNLOAD_METADATA:
        case TR_STATUS_QUEUE_FOR_SEED:
            break;
        
        default:
            if (auto const buf = get_short_transfer_text(); !std::empty(buf))
            {
                status_str += fmt::format(" - {:s}", buf);
            }
    }

    return status_str;
}

std::vector<Glib::ustring> Torrent::Impl::get_css_classes() const
{
    auto result = std::vector<Glib::ustring>({
        fmt::format("tr-transfer-{}", get_activity_direction(cache_.tor_state))
    });

    if (cache_.err_code)
    {
        result.emplace_back("tr-error");
    }

    return result;
}

void Torrent::Impl::class_init(void* cls, void* /*user_data*/)
{
    PropertyStore::get().install(
        G_OBJECT_CLASS(cls),
        {
            { Property::ICON, "icon", "Icon", "Icon based on torrent's likely MIME type", &Torrent::get_icon },
            { Property::NAME, "name", "Name", "Torrent name / title", &Torrent::get_name },
            { Property::PERCENT_DONE,
              "percent-done",
              "Percent done",
              "Percent done (0..1) for current activity (leeching or seeding)",
              &Torrent::get_percent_done_fraction },
            { Property::SHORT_STATUS,
              "short-status",
              "Short status",
              "Status text displayed in compact view mode",
              &Torrent::get_short_status_text },
            { Property::LONG_PROGRESS,
              "long-progress",
              "Long progress",
              "Progress text displayed in full view mode",
              &Torrent::get_long_progress_text },
            { Property::LONG_STATUS,
              "long-status",
              "Long status",
              "Status text displayed in full view mode",
              &Torrent::get_long_status_text },
            { Property::SENSITIVE,
              "sensitive",
              "Sensitive",
              "Visual sensitivity of the view item, unrelated to activation possibility",
              &Torrent::get_sensitive },
            { Property::CSS_CLASSES,
              "css-classes",
              "CSS classes",
              "CSS class names used for styling view items",
              &Torrent::get_css_classes },
        });
}


/*eg. 2.56MB/s▼ 49KB/s▲ */
Glib::ustring Torrent::Impl::get_short_transfer_text() const
{
    if (cache_.has_metadata && cache_.is_finished)
    {
        return fmt::format(fmt::runtime(_("{upload_speed} ▲")), fmt::arg("upload_speed", cache_.speed_up.to_string()));
    }

    if (cache_.has_metadata && cache_.num_peers > 0)
    {
        return fmt::format(
            fmt::runtime(_("{download_speed} ▼  {upload_speed} ▲")),
            fmt::arg("upload_speed", cache_.speed_up.to_string()),
            fmt::arg("download_speed", cache_.speed_down.to_string()));
    }


    return {};
}

Glib::ustring Torrent::Impl::get_error_text() const
{
    if(cache_.err_code)
    {
        return fmt::format(_("Ops!!: '{msg}'"), fmt::arg("msg", cache_.err_code.message().c_str()));
    }
    return {};
}



Glib::ustring Torrent::Impl::get_activity_text() const
{
 
    switch (cache_.tor_state)
    {
    case TR_STATUS_PAUSED:
    case TR_STATUS_QUEUE_FOR_DOWNLOAD_METADATA:
    case TR_STATUS_QUEUE_FOR_DOWNLOAD:
    case TR_STATUS_QUEUE_FOR_SEED:
    case TR_STATUS_CHECKING_FILES:
    case TR_STATUS_CHECKING_RESUME_DATA:
    case TR_STATUS_QUEUE_FOR_CHECKING_FILES:
    case TR_STATUS_QUEUE_FOR_CHECKING_RESUME_DATA:
    case TR_STATUS_FINISHED:
    case TR_STATUS_NULL:
        return get_short_status_text();


    case TR_STATUS_DOWNLOAD_METADATA:
        if (!cache_.has_metadata)
        {
            return fmt::format(
                ngettext(
                    // xgettext:no-c-format
                    "Downloading metadata from {active_count} connected peer ({percent_done}% done)",
                    "Downloading metadata from {active_count} connected peers ({percent_done}% done)",
                    cache_.num_peers),
                fmt::arg("active_count", cache_.num_peers),
                fmt::arg("percent_done", cache_.percent_done.to_string()));
        }

    case TR_STATUS_DOWNLOAD:
        return fmt::format(
            ngettext(
                "Downloading from {active_count} of {connected_count} connected peer",
                "Downloading from {active_count} of {connected_count} connected peers",
                cache_.num_peers),
            fmt::arg("active_count", cache_.num_peers),
            fmt::arg("connected_count", cache_.num_peers));



    case TR_STATUS_SEED:
        return fmt::format(
            ngettext(
                "Seeding to {active_count} of {connected_count} connected peer",
                "Seeding to {active_count} of {connected_count} connected peers",
                cache_.num_peers),
            fmt::arg("active_count", cache_.num_leechers),
            fmt::arg("connected_count", cache_.num_peers));

    default:
        g_assert_not_reached();
        return {};
    }
}



/**************
*****CTOR******
***************/
Glib::RefPtr<Torrent> Torrent::create(lt::torrent_handle const& handle)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    return Glib::make_refptr_for_instance(new Torrent(handle));
}


Torrent::Torrent()
    : Glib::ObjectBase(typeid(Torrent))
    , ExtraClassInit(&Impl::class_init)
{
}

Torrent::Torrent(lt::torrent_handle const& handle)
    : Glib::ObjectBase(typeid(Torrent))
    , ExtraClassInit(&Impl::class_init)
    , impl_(std::make_unique<Impl>(*this, handle))
{
    g_assert(handle.is_valid());
}

Torrent::Impl::Impl(Torrent& torrent, lt::torrent_handle const& handle)
    : torrent_(torrent)
    , raw_torrent_(handle)
{
    //ID
    ih_ = handle.info_hashes();
    uniq_id_ = handle.id();


    //post torrent related infomation
    post_torrent_timer_ = Glib::signal_timeout().connect_seconds(
    sigc::mem_fun(*this, &Impl::post_for_torrent), 1);


    //update iniitally
    if (raw_torrent_.is_valid())
    {
        update_cache();
    }
}




//USEd by TorrentSorter
Glib::ustring const& Torrent::get_name_collated() const
{
    return impl_->get_cache().name_collated;
}


void Torrent::saveMagnetLink(std::string && ml)
{
    impl_->saveMagnetLink(std::move(ml));
}

void Torrent::Impl::saveMagnetLink(std::string && ml)
{
    magnet_link_ = std::move(ml);
}


std::string const& Torrent::fetchMagnetLink() const
{
    return impl_->fetchMagnetLink();
}

std::string const& Torrent::Impl::fetchMagnetLink() const
{
    return magnet_link_;
}







//USED FOR MainWIndow Session Global Speed
Speed Torrent::get_speed_up() const
{
    return impl_->get_cache().speed_up;
}
//USED FOR MainWIndow Session Global Speed
Speed Torrent::get_speed_down() const
{
    return impl_->get_cache().speed_down;
}








//used for TorrentFilter
bool Torrent::get_finished() const
{
    return impl_->get_cache().is_finished;
}


//NOT USED
lt::download_priority_t Torrent::get_priority() const
{
    return impl_->get_cache().priority;
}


//used for TorrentSorter
size_t Torrent::get_queue_position() const
{
    return impl_->get_cache().queue_pos;
}





//Used for TorrentCellRender
lt::error_code Torrent::get_error_code() const
{
    return impl_->get_cache().err_code;
}

//NOT USED
// Glib::ustring const& Torrent::get_error_message() const
// {
//     return impl_->get_cache().err_code.message();
// }


//used for TorrentFilter
int Torrent::get_active_peer_count() const
{
    return impl_->get_cache().num_peers;
}

//USED for TorrentCellRender and TorrentSorter
tr_torrent_activity Torrent::get_activity() const
{
    return impl_->get_cache().tor_state;
}



//used for TorrentSorter
Storage Torrent::get_total_size() const
{
    return impl_->get_cache().complete_size_total;
}



//used for TorrentSorter
time_t Torrent::get_eta() const
{
    return impl_->get_cache().eta;
}


//used for TorrentSorter
time_t Torrent::get_added_date() const
{
    return impl_->get_cache().added_time;
}



//Used for TorrentCellRender
Glib::RefPtr<Gio::Icon> Torrent::get_icon() const
{
    return impl_->get_icon();
}

//Used for TorrentCellRender
Glib::ustring Torrent::get_name() const
{
    return impl_->get_cache().name;
}

//Used for TorrentCellRender
Percents Torrent::get_percent_done() const
{
    return impl_->get_cache().percent_done;
}

//Used for TorrentCellRender
float Torrent::get_percent_done_fraction() const
{
    return get_percent_done().to_fraction();
}

//Used for TorrentCellRender
Glib::ustring Torrent::get_short_status_text() const
{
    return impl_->get_short_status_text();
}

//Used for TorrentCellRender
Glib::ustring Torrent::get_long_progress_text() const
{
    return impl_->get_long_progress_text();
}

//Used for TorrentCellRender
Glib::ustring Torrent::get_long_status_text() const
{
    return impl_->get_long_status_text();
}


//Used for TorrentCellRender
bool Torrent::get_sensitive() const
{
    return impl_->get_cache().tor_state != TR_STATUS_PAUSED;
}


std::vector<Glib::ustring> Torrent::get_css_classes() const
{
    return impl_->get_css_classes();
}



Torrent::ChangeFlags Torrent::update()
{
    auto result = impl_->update_cache();
    impl_->notify_property_changes(result);
    return result;
}














//USED for ListModelAdapter::TreeModelColumnRecord columns_
Torrent::Columns const& Torrent::get_columns()
{
    static Columns const columns;
    return columns;
}

//USED for  ListModelAdapter::id_getter_
std::uint32_t Torrent::get_item_id(Glib::RefPtr<Glib::ObjectBase const> const& item)
{
    if (auto const torrent = gtr_ptr_dynamic_cast<Torrent const>(item); torrent != nullptr)
    {
        return torrent->get_uniq_id();
    }

    return std::uint32_t{};
}


//USED for ListModelAdapter::value_getter_
void Torrent::get_item_value(Glib::RefPtr<Glib::ObjectBase const> const& item, int column, Glib::ValueBase& value/*ref*/)
{
    if (auto const torrent = gtr_ptr_dynamic_cast<Torrent const>(item); torrent != nullptr)
    {
        torrent->impl_->get_value(column, value);
    }
}

void Torrent::Impl::get_value(int column, Glib::ValueBase& value) const
{
    static auto const& columns = Torrent::get_columns();

    if (column == columns.self.index())
    {
        column_value_cast(value, columns.self).set(&torrent_);
    }
    else if (column == columns.name_collated.index())
    {
        column_value_cast(value, columns.name_collated).set(cache_.name_collated);
    }
}







int Torrent::compare_by_uniq_id(Glib::RefPtr<Torrent const> const& lhs, Glib::RefPtr<Torrent const> const& rhs)
{
    return tr_compare_3way(lhs->get_uniq_id(), rhs->get_uniq_id());
}

bool Torrent::less_by_uniq_id(Glib::RefPtr<Torrent const> const& lhs, Glib::RefPtr<Torrent const> const& rhs)
{
    return lhs->get_uniq_id() < rhs->get_uniq_id();
}




void Torrent::feed_total_updown_at_load(std::int64_t up, std::int64_t dn)
{
    impl_->feed_total_updown_at_load(up, dn);
}


void Torrent::Impl::feed_total_updown_at_load(std::int64_t up, std::int64_t dn)
{
    total_upload_when_load_ = up;
    total_download_when_load_ = dn;
}

void Torrent::total_updown_delta_at_remove(std::int64_t& delta_up, std::int64_t& delta_down)
{

   impl_->total_updown_delta_at_remove(delta_up, delta_down);
}

void Torrent::Impl::total_updown_delta_at_remove(std::int64_t& delta_up, std::int64_t& delta_down)
{
    if(is_status_valid())
    {   
        return;
    }
   
    delta_up =  tor_status_.all_time_upload - total_upload_when_load_;
    delta_down =  tor_status_.all_time_download - total_download_when_load_;

}


std::mutex& Torrent::fetch_tor_status_mutex()
{
    return impl_->fetch_tor_status_mutex();
}

std::mutex& Torrent::Impl::fetch_tor_status_mutex()
{
    return status_mutex_;

}


//use when remove/delete torrent, cuz torrent obj within torrent_handle may destruct before GTK Torrent,so we stop access the torrent_handle
void Torrent::disconnect_signals()
{
    impl_->disconnect_signals();
}

void Torrent::Impl::disconnect_signals()
{
    post_torrent_timer_.disconnect();
}




int Torrent::get_total_num_pieces()
{
    return impl_->get_total_num_pieces();
}
int Torrent::Impl::get_total_num_pieces()
{
    return cache_.total_num_pieces;
}