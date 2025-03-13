// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "TorrentFilter.h"
#include "Torrent.h"
#include "FilterBase.hh"
#include "Utils.h"
#include "Flags.h"
#include "tr-transmission.h"
#include "tr-web-utils.h"

#include <array>
#include <utility>
#include <iostream>


#include "libtorrent/torrent_info.hpp"
#include "libtorrent/file_storage.hpp"
#include "libtorrent/torrent_handle.hpp"

using namespace lt;

TorrentFilter::TorrentFilter()
    : Glib::ObjectBase(typeid(TorrentFilter))
{
}



void TorrentFilter::set_activity(Activity type)
{   
    /*already chosen ,do nothing, just return*/
    if (activity_type_ == type)
    {
        return;
    }

    auto change = Change::DIFFERENT;
    //more  narrow
    if (activity_type_ == Activity::ALL)
    {
        change = Change::MORE_STRICT;
    }
    //more wide
    else if (type == Activity::ALL)
    {
        change = Change::LESS_STRICT;
    }

    activity_type_ = type;
    changed(change);
}




void TorrentFilter::set_tracker(Tracker type, Glib::ustring const& host)
{
    /*already chosen, do nothing, and just return*/
    if (tracker_type_ == type && tracker_host_ == host)
    {
        return;
    }

    auto change = Change::DIFFERENT;
    if (tracker_type_ != type)//tracker_type_ != type 
    {
        //more specific
        if (tracker_type_ == Tracker::ALL)
        {
            change = Change::MORE_STRICT;
        }
        //more generic
        else if (type == Tracker::ALL)
        {
            change = Change::LESS_STRICT;
        }
    }
    else //tracker_type_ == type  AND tracker_host_ != host
    {
        if (tracker_host_.empty() || host.find(tracker_host_) != Glib::ustring::npos)
        {
            change = Change::MORE_STRICT;
        }
        else if (host.empty() || tracker_host_.find(host) != Glib::ustring::npos)
        {
            change = Change::LESS_STRICT;
        }
    }

    tracker_type_ = type;
    tracker_host_ = host;
    changed(change);
}



void TorrentFilter::set_text(Glib::ustring const& text)
{
    auto const normalized_text = gtr_str_strip(text.casefold());
    if (text_ == normalized_text)
    {
        return;
    }

    auto change = Change::DIFFERENT;
    if (text_.empty() || normalized_text.find(text_) != Glib::ustring::npos)
    {
        change = Change::MORE_STRICT;
    }
    else if (normalized_text.empty() || text_.find(normalized_text) != Glib::ustring::npos)
    {
        change = Change::LESS_STRICT;
    }

    text_ = normalized_text;
    changed(change);
}



bool TorrentFilter::match_activity(Torrent const& torrent) const
{
    return match_activity(torrent, activity_type_);
}



bool TorrentFilter::match_tracker(Torrent const& torrent) const
{
    return match_tracker(torrent, tracker_type_, tracker_host_);
}



bool TorrentFilter::match_text(Torrent const& torrent) const
{
    return match_text(torrent, text_);
}



bool TorrentFilter::match(Torrent const& torrent) const
{
    return match_activity(torrent) && match_tracker(torrent) && match_text(torrent);
}


/*whether too show all*/
bool TorrentFilter::matches_all() const
{
    return activity_type_ == Activity::ALL && tracker_type_ == Tracker::ALL && text_.empty();
}



void TorrentFilter::update(Torrent::ChangeFlags changes)
{
    using Flag = Torrent::ChangeFlag;

    bool refilter_needed = false;

    if (activity_type_ != Activity::ALL)
    {
        static constexpr auto ActivityFlags = std::array<std::pair<Activity, Torrent::ChangeFlags>, 7U>{ {
            { Activity::DOWNLOADING, Flag::TORRENT_STATE },
            { Activity::SEEDING, Flag::TORRENT_STATE },
            { Activity::DL_METADATA, Flag::TORRENT_STATE },
            { Activity::PAUSED, Flag::IS_PAUSED | Flag::TORRENT_STATE },
            { Activity::FINISHED, Flag::IS_FINISHED | Flag::TORRENT_STATE},
            { Activity::CHECKING, Flag::TORRENT_STATE },
            { Activity::ERROR, Flag::ERROR_CODE }
        } };

        auto const iter = std::find_if(
            std::begin(ActivityFlags),
            std::end(ActivityFlags),
            [key = activity_type_](auto const& row) { return row.first == key; });
        refilter_needed = (iter != std::end(ActivityFlags) && changes.test(iter->second));
    }

    if (!refilter_needed)
    {
        refilter_needed = tracker_type_ != Tracker::ALL && changes.test(Flag::TRACKERS_HASH);
    }

    if (!refilter_needed)
    {
        refilter_needed = !text_.empty() && changes.test(Flag::NAME);
    }

    if (refilter_needed)
    {
        changed(Change::DIFFERENT);
    }
}



Glib::RefPtr<TorrentFilter> TorrentFilter::create()
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    return Glib::make_refptr_for_instance(new TorrentFilter());
}




bool TorrentFilter::match_activity(Torrent const& torrent, Activity type)
{
    auto activity = tr_torrent_activity{};
          
    switch (type)
    {
        /*return all torrents whatever their activity are*/
        case Activity::ALL:
            return true;

        case Activity::DOWNLOADING:
            activity = torrent.get_activity();
            return activity == TR_STATUS_DOWNLOAD || activity == TR_STATUS_QUEUE_FOR_DOWNLOAD;

        case Activity::DL_METADATA:
            activity = torrent.get_activity();
            return activity == TR_STATUS_DOWNLOAD_METADATA || activity == TR_STATUS_QUEUE_FOR_DOWNLOAD_METADATA;

        case Activity::SEEDING:
            activity = torrent.get_activity();
            return activity == TR_STATUS_SEED || activity == TR_STATUS_QUEUE_FOR_SEED;
            
        case Activity::PAUSED:
            return torrent.get_activity() == TR_STATUS_PAUSED;

        case Activity::FINISHED:
            return torrent.get_finished();

        case Activity::CHECKING:
            activity = torrent.get_activity();
            return 
                activity == (TR_STATUS_CHECKING_FILES || activity == TR_STATUS_QUEUE_FOR_CHECKING_FILES
                || TR_STATUS_CHECKING_RESUME_DATA || TR_STATUS_QUEUE_FOR_CHECKING_RESUME_DATA);
                
        case Activity::ERROR:
            if( torrent.get_error_code() )
            {
                return true;
            }
            else
            {
                return false;
            } 

        default:
            g_assert_not_reached();
            return true;
    }
}



bool TorrentFilter::match_tracker(Torrent const& torrent, Tracker type, Glib::ustring const& host)
{
    /*all trackers*/
    if (type == Tracker::ALL)
    {
        return true;
    }
    
    /*tracker which match our given host*/
    g_assert(type == Tracker::HOST);
    auto const& v_aes = torrent.get_trackers_vec_const_ref();
    for (auto i = size_t{ 0 }, n = v_aes.size(); i < n; ++i)
    {
        if (auto const tracker_url_sitename = tr_urlParse(v_aes[i].url.c_str())->sitename; std::data(tracker_url_sitename) == host)
        {
            return true;
        }
    }
    return false;

}

namespace
{

Glib::ustring from_string_view(std::string_view sv) 
{
    return Glib::ustring(sv.data(), sv.size());
}


}

bool TorrentFilter::match_text(Torrent const& torrent, Glib::ustring const& text)
{
    bool ret = false;

    if (text.empty())
    {
        ret = true;
    }
    else
    {
        auto const& handle = torrent.get_handle_ref();
        auto const& ti = handle.torrent_file();

        /* test the torrent name... */
        ret = torrent.get_name().casefold().find(text) != Glib::ustring::npos;

        if(ti->is_valid())
        {
            lt::file_storage const& fs = ti->files();
            /* also test the files in that torrent... */
            for (size_t i = 0, n = ti->num_files(); i < n && !ret; ++i)
            {
                auto fname_sv = fs.file_name(i);
                ret = Glib::ustring(fs.file_name(i).data(), fname_sv.size()).casefold().find(text) != Glib::ustring::npos;
            }

        }
    
    }

    return ret;
}



