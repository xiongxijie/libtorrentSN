// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "TorrentSorter.h"
#include "Percents.h"
#include "SorterBase.hh"
#include "Utils.h"

#include "tr-transmission.h"
#include "tr-utils.h"

#include <algorithm>
#include <array>
#include <utility>

using namespace std::string_view_literals;

namespace
{

    constexpr bool is_valid_eta(time_t value)
    {
        // return value != TR_ETA_NOT_AVAIL && value != TR_ETA_UNKNOWN;
        return value > 0;
    }


    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_name(Torrent const& lhs, Torrent const& rhs)
    {
        return tr_compare_3way(lhs.get_name_collated(), rhs.get_name_collated());
    }


    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_queue(Torrent const& lhs, Torrent const& rhs)
    {
        return tr_compare_3way(lhs.get_queue_position(), rhs.get_queue_position());
    }



    int compare_by_activity(Torrent const& lhs, Torrent const& rhs)
    {
        if (auto val = -tr_compare_3way(lhs.get_speed_up() + lhs.get_speed_down(), rhs.get_speed_up() + rhs.get_speed_down());
            val != 0)
        {
            return val;
        }

        if (auto val = -tr_compare_3way(lhs.get_active_peer_count(), rhs.get_active_peer_count()); val != 0)
        {
            return val;
        }

        return compare_by_queue(lhs, rhs);
    }



    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_age(Torrent const& lhs, Torrent const& rhs)
    {
        if (auto val = -tr_compare_3way(lhs.get_added_date(), rhs.get_added_date()); val != 0)
        {
            return val;
        }

        return compare_by_name(lhs, rhs);
    }



    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_size(Torrent const& lhs, Torrent const& rhs)
    {
        if (auto val = -tr_compare_3way(lhs.get_total_size(), rhs.get_total_size()); val != 0)
        {
            return val;
        }

        return compare_by_name(lhs, rhs);
    }


    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_progress(Torrent const& lhs, Torrent const& rhs)
    {
        if (auto val = -tr_compare_3way(lhs.get_percent_done(), rhs.get_percent_done()); val != 0)
        {
            return val;
        }

        // if (auto val = -tr_compare_3way(lhs.get_seed_ratio_percent_done(), rhs.get_seed_ratio_percent_done()); val != 0)
        // {
        //     return val;
        // }

        return compare_by_name(lhs, rhs);
    }



    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    constexpr int compare_eta(time_t lhs, time_t rhs)
    {
        bool const lhs_valid = is_valid_eta(lhs);
        bool const rhs_valid = is_valid_eta(rhs);

        if (!lhs_valid && !rhs_valid)
        {
            return 0;
        }

        if (!lhs_valid)
        {
            return -1;
        }

        if (!rhs_valid)
        {
            return 1;
        }

        return -tr_compare_3way(lhs, rhs);
    }



    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_eta(Torrent const& lhs, Torrent const& rhs)
    {
        if (auto val = compare_eta(lhs.get_eta(), rhs.get_eta()); val != 0)
        {
            return val;
        }

        return compare_by_name(lhs, rhs);
    }



    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    int compare_by_state(Torrent const& lhs, Torrent const& rhs)
    {
        if (auto val = -tr_compare_3way(lhs.get_activity(), rhs.get_activity()); val != 0)
        {
            return val;
        }

        return compare_by_queue(lhs, rhs);
    }


} //anonymous namespace









/*
********
**CTOR**
********
*/
TorrentSorter::TorrentSorter()
    : Glib::ObjectBase(typeid(TorrentSorter))
{
}






/*Set compare criterion*/
void TorrentSorter::set_mode(std::string_view mode)
{
    static auto constexpr DefaultCompareFunc = &compare_by_name;
    static auto constexpr CompareFuncs = std::array<std::pair<std::string_view, CompareFunc>, 8U>{ {
        { "sort-by-activity"sv, &compare_by_activity },
        { "sort-by-age"sv, &compare_by_age },
        { "sort-by-name"sv, &compare_by_name },
        { "sort-by-progress"sv, &compare_by_progress },
        { "sort-by-queue"sv, &compare_by_queue },
        { "sort-by-size"sv, &compare_by_size },
        { "sort-by-state"sv, &compare_by_state },
        { "sort-by-time-left"sv, &compare_by_eta }
    } };

    auto const iter = std::find_if(
        std::begin(CompareFuncs),
        std::end(CompareFuncs),
        [key = mode](auto const& row) { return row.first == key; });
    auto const compare_func = iter != std::end(CompareFuncs) ? iter->second : DefaultCompareFunc;
    if (compare_func_ == compare_func)
    {
        return;
    }

    compare_func_ = compare_func;
    changed(Change::DIFFERENT);
}


/*set sort order , ascending or descending*/
void TorrentSorter::set_reversed(bool is_reversed)
{
    if (is_reversed_ == is_reversed)
    {
        return;
    }

    is_reversed_ = is_reversed;
    changed(Change::INVERTED);
}




/* Used for SortListModel */
int TorrentSorter::compare(Torrent const& lhs, Torrent const& rhs) const
{
    return compare_func_ != nullptr ? std::clamp(compare_func_(lhs, rhs), -1, 1) * (is_reversed_ ? -1 : 1) : 0;
}





void TorrentSorter::update(Torrent::ChangeFlags changes)
{
    //use alias
    using Flag = Torrent::ChangeFlag;
    static auto constexpr CompareFlags = std::array<std::pair<CompareFunc, Torrent::ChangeFlags>, 7U>{ {
        { &compare_by_activity, Flag::NUM_CONN | Flag::QUEUE_POSITION | Flag::SPEED_DOWN | Flag::SPEED_UP },
        { &compare_by_age, Flag::ADDED_DATE | Flag::NAME },
        { &compare_by_eta, Flag::ETA | Flag::NAME },
        { &compare_by_name, Flag::NAME },
        { &compare_by_progress, Flag::PERCENT_DONE | Flag::QUEUE_POSITION },
        { &compare_by_size, Flag::NAME | Flag::TOTAL_SIZE },
        { &compare_by_state, Flag::TORRENT_STATE | Flag::QUEUE_POSITION }
    } };

    if (auto const iter = std::find_if(
            std::begin(CompareFlags),
            std::end(CompareFlags),
            [key = compare_func_](auto const& row) { return row.first == key; });
        iter != std::end(CompareFlags) && changes.test(iter->second))
    {
        changed(Change::DIFFERENT);
    }
}




Glib::RefPtr<TorrentSorter> TorrentSorter::create()
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    return Glib::make_refptr_for_instance(new TorrentSorter());
}
