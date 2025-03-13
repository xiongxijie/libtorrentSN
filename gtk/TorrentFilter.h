// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include "FilterBase.h"
#include "Torrent.h"

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>

#include <cstdint>

class TorrentFilter : public FilterBase<Torrent>
{
public:
    enum class Activity : int8_t
    {
        ALL,
        CHECKING,
        DL_METADATA,
        DOWNLOADING,
        FINISHED,
        SEEDING,
        PAUSED,
        ERROR,
    };

    enum class Tracker : int8_t
    {
        ALL,
        HOST,
    };

public:
    void set_activity(Activity type);
    void set_tracker(Tracker type, Glib::ustring const& host);
    void set_text(Glib::ustring const& text);

    bool match_activity(Torrent const& torrent) const;
    bool match_tracker(Torrent const& torrent) const;
    bool match_text(Torrent const& torrent) const;

    // FilterBase<Torrent>
    bool match(Torrent const& torrent) const override;
    bool matches_all() const override;

    void update(Torrent::ChangeFlags changes);

    static Glib::RefPtr<TorrentFilter> create();

    static bool match_activity(Torrent const& torrent, Activity type);
    static bool match_tracker(Torrent const& torrent, Tracker type, Glib::ustring const& host);
    static bool match_text(Torrent const& torrent, Glib::ustring const& text);

private:
    TorrentFilter();

private:
    /*Activity */
    Activity activity_type_ = Activity::ALL;
    /*Tracker */
    Tracker tracker_type_ = Tracker::ALL;

    Glib::ustring tracker_host_;
    /*text Entry*/
    Glib::ustring text_;
};
