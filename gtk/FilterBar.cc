// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "FilterBar.h"

#include "FilterListModel.hh"
#include "HigWorkarea.h" // GUI_PAD
#include "ListModelAdapter.h"
#include "Session.h" // torrent_cols
#include "Torrent.h"
#include "TorrentFilter.h"
#include "Utils.h"
#include "tr-web-utils.h"

#include <gdkmm/pixbuf.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/unicode.h>
#include <glibmm/ustring.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/combobox.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treestore.h>

#if GTKMM_CHECK_VERSION(4, 0, 0)
#include <gtkmm/filterlistmodel.h>
#endif

#include <fmt/core.h>

#include <algorithm> // std::transform()
#include <array>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <iostream>




namespace
{
    using ActivityType = TorrentFilter::Activity;
    using TrackerType = TorrentFilter::Tracker;

    constexpr auto ActivitySeparator = static_cast<ActivityType>(-1);
    constexpr auto TrackerSeparator = static_cast<TrackerType>(-1);
} //anonymous namespace


class FilterBar::Impl
{
    using FilterModel = IF_GTKMM4(Gtk::FilterListModel, Gtk::TreeModelFilter);

    public:
        Impl(FilterBar& widget, Glib::RefPtr<Session> const& core);
        ~Impl();

        TR_DISABLE_COPY_MOVE(Impl)

        [[nodiscard]] Glib::RefPtr<FilterModel> get_filter_model() const;

    private:
        template<typename T>
        T* get_template_child(char const* name) const;

    
        static Gtk::CellRendererText* number_renderer_new();
        static void render_number_func(Gtk::CellRendererText& cell_renderer, Gtk::TreeModel::const_iterator const& iter);


        void update_filter_models_idle(Torrent::ChangeFlags changes);
        void update_filter_models(Torrent::ChangeFlags changes);

        void update_count_label_idle();
        bool update_count_label();


        /*TRACKER FILTER COMBO*/
        void tracker_combo_box_init(Gtk::ComboBox& combo);
        bool tracker_filter_model_update();
        static Glib::RefPtr<Gtk::TreeStore> tracker_filter_model_new();
        static void tracker_model_update_count(Gtk::TreeModel::iterator const& iter, int n);
        static Glib::ustring get_name_from_host(std::string const& host);
        void favicon_ready_cb(Glib::RefPtr<Gdk::Pixbuf> const* pixbuf, Gtk::TreeModel::Path const& path);
        static bool is_it_a_separator(Gtk::TreeModel::const_iterator const& iter);
        static void render_pixbuf_func(Gtk::CellRendererPixbuf& cell_renderer, Gtk::TreeModel::const_iterator const& iter);
        void update_filter_tracker();




        /*STATE(ACTIVITY) FILTER COMBO*/
        void activity_combo_box_init(Gtk::ComboBox& combo);
        static void render_activity_pixbuf_func(Gtk::CellRendererPixbuf& cell_renderer, Gtk::TreeModel::const_iterator const& iter);
        static Glib::RefPtr<Gtk::ListStore> activity_filter_model_new();
        static void status_model_update_count(Gtk::TreeModel::iterator const& iter, int n);
        static bool activity_is_it_a_separator(Gtk::TreeModel::const_iterator const& iter);
        void update_filter_activity();
        bool activity_filter_model_update();



        /*TEXT ENTRY(SEARCH BY TORRENT NAME)*/
        void update_filter_text();




    private:
        FilterBar& widget_;

        Glib::RefPtr<Session> const core_;

        Glib::RefPtr<Gtk::ListStore> const activity_model_;

        Glib::RefPtr<Gtk::TreeStore> const tracker_model_;

        Gtk::ComboBox* activity_ = nullptr;
        Gtk::ComboBox* tracker_ = nullptr;
        Gtk::Entry* entry_ = nullptr;
        Gtk::Label* show_lb_ = nullptr;

        Glib::RefPtr<TorrentFilter> filter_ = TorrentFilter::create();

        Glib::RefPtr<FilterListModel<Torrent>> filter_model_;

        sigc::connection update_count_label_tag_;
        sigc::connection update_filter_models_tag_;
        sigc::connection update_filter_models_on_add_remove_tag_;
        sigc::connection update_filter_models_on_change_tag_;
};







//---------------------------------------------TRACKERS-------------------------------------------------------
namespace
{
    class TrackerFilterModelColumns : public Gtk::TreeModelColumnRecord
    {
    public:
        TrackerFilterModelColumns() noexcept
        {
            add(displayname);
            add(count);
            add(type);
            add(sitename);
            add(pixbuf);
        }

        Gtk::TreeModelColumn<Glib::ustring> sitename; // pattern-matching text; see tr_parsed_url.sitename
        Gtk::TreeModelColumn<Glib::ustring> displayname; /* human-readable name; ie, Legaltorrents */
        Gtk::TreeModelColumn<int> count; /* how many matches there are */
        Gtk::TreeModelColumn<TrackerType> type;
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> pixbuf;
    };

    TrackerFilterModelColumns const tracker_filter_cols;

} //anonymous namespace


/* human-readable name; ie, Legaltorrents */
Glib::ustring FilterBar::Impl::get_name_from_host(std::string const& host)
{
    std::string name = host;

    if (!name.empty())
    {
        name.front() = Glib::Ascii::toupper(name.front());
    }

    return name;
}


void FilterBar::Impl::tracker_model_update_count(Gtk::TreeModel::iterator const& iter, int n)
{
    if (n != iter->get_value(tracker_filter_cols.count))
    {
        iter->set_value(tracker_filter_cols.count, n);
    }
}


void FilterBar::Impl::favicon_ready_cb(Glib::RefPtr<Gdk::Pixbuf> const* pixbuf, Gtk::TreeModel::Path const& path)
{
    if (pixbuf != nullptr && *pixbuf != nullptr)
    {
        if (auto const iter = tracker_model_->get_iter(path); iter)
        {
            iter->set_value(tracker_filter_cols.pixbuf, *pixbuf);
        }
    }
}


bool FilterBar::Impl::tracker_filter_model_update()
{
    struct site_info
    {
        int count = 0;/*number of torrent who has this same tracker url*/
        std::string sitename;/*as google in http://www.google.com*/
        std::string url;
        // std::string host;

        //compare based on first capital of sitename alphabetically
        bool operator<(site_info const& that) const
        {
            return sitename < that.sitename;
        }
    };

    auto const torrents_model = core_->get_model();

    /* Walk through all the torrents, tallying how many matches there are
     * for the various categories. Also make a sorted list of all tracker
     * hosts s.t. we can merge it with the existing list */
    auto n_torrents = 0;

    /*hold info about each track-site*/
    auto site_infos = std::unordered_map<std::string /*tracker-url*/, site_info>{};

    /*iterate through all Torrents, add new tracker if not exist, merge already exist one*/
    for (auto i = 0U, count = torrents_model->get_n_items(); i < count; ++i)
    {
        auto const Torr = gtr_ptr_dynamic_cast<Torrent>(torrents_model->get_object(i));
        if (Torr == nullptr)
        {
            continue;
        }

        auto v_tracker_url = std::vector<std::string>{};
        /*query trackers list*/
        auto& trackers = Torr->get_trackers_vec_ref();

        for (auto const& ae : trackers)
        {
            v_tracker_url.push_back(ae.url);
        }

        // populates site_infos with the count of how many torrents each tracker is associated with.
        for (auto const& tracker_url : v_tracker_url)
        {
            auto& si_ref = site_infos[tracker_url];
            auto url_pased_ret =  tr_urlParse(tracker_url.c_str());
            auto sname = url_pased_ret ? url_pased_ret->sitename : "";

            si_ref.sitename = sname;
            si_ref.url = tracker_url;
            ++si_ref.count;/*num of torrent who also has this same tracker */
        }

        ++n_torrents;
    }

    //num of various trackers
    auto const n_sites = std::size(site_infos);
    auto sites_v = std::vector<site_info>(n_sites);
    std::transform(std::begin(site_infos), std::end(site_infos), std::begin(sites_v), [](auto const& it) { return it.second; });
    std::sort(std::begin(sites_v), std::end(sites_v));

    // update the "all" count
    auto iter = tracker_model_->children().begin();
    if (iter)
    {
        tracker_model_update_count(iter, n_torrents);
    }

    // offset past the "All" and the separator
    ++iter;
    ++iter;

    size_t i = 0;
    for (;;)
    {
        // are we done yet?
        bool const new_sites_done = i >= n_sites;
        bool const old_sites_done = !iter;
        if (new_sites_done && old_sites_done)
        {
            break;
        }

        // decide what to do
        bool remove_row = false;
        bool insert_row = false;
        //less tracker than existent one, need to remove some
        if (new_sites_done)
        {
            remove_row = true;
        }
        //more trackers than existent one, need to add some
        else if (old_sites_done)
        {
            insert_row = true;
        }
        //tracker list stay unchanged compared to existent one
        else
        {
            auto const sitename = iter->get_value(tracker_filter_cols.sitename);
            int const cmp = sitename.raw().compare(sites_v.at(i).sitename);

            if (cmp < 0)
            {
                remove_row = true;
            }
            else if (cmp > 0)
            {
                insert_row = true;
            }
        }

        // do something
        if (remove_row)
        {
            iter = tracker_model_->erase(iter);
        }
        else if (insert_row)
        {
            auto const& site = sites_v.at(i);
            auto const add = tracker_model_->insert(iter);
            add->set_value(tracker_filter_cols.sitename, Glib::ustring{ site.sitename });
            add->set_value(tracker_filter_cols.displayname, get_name_from_host(site.sitename));
            add->set_value(tracker_filter_cols.count, site.count);
            add->set_value(tracker_filter_cols.type, TrackerType::HOST);
            auto path = tracker_model_->get_path(add);
            core_->favicon_cache().load(
                site.url,
                [this, path = std::move(path)](auto const* pixbuf) { favicon_ready_cb(pixbuf, path); });
            ++i;
        }
        else // update row
        {
            tracker_model_update_count(iter, sites_v.at(i).count);
            ++iter;
            ++i;
        }
    }

    return false;
}


Glib::RefPtr<Gtk::TreeStore> FilterBar::Impl::tracker_filter_model_new()
{
    auto store = Gtk::TreeStore::create(tracker_filter_cols);

    /* add 'All' Record */
    auto iter = store->append();
    iter->set_value(tracker_filter_cols.displayname, Glib::ustring(_("All")));
    iter->set_value(tracker_filter_cols.type, TrackerType::ALL);

    /*add Separator */
    iter = store->append();
    iter->set_value(tracker_filter_cols.type, TrackerSeparator);

    return store;
}


void FilterBar::Impl::tracker_combo_box_init(Gtk::ComboBox& combo)
{
    combo.set_model(tracker_model_);
    combo.set_row_separator_func(sigc::hide<0>(&Impl::is_it_a_separator));
    combo.set_active(0);

    {
        /*Icon*/
        auto* r = Gtk::make_managed<Gtk::CellRendererPixbuf>();
        combo.pack_start(*r, false);
        combo.set_cell_data_func(*r, [r](auto const& iter) { render_pixbuf_func(*r, iter); });
        combo.add_attribute(r->property_pixbuf(), tracker_filter_cols.pixbuf);
    }

    {
        /*Display Name*/
        auto* r = Gtk::make_managed<Gtk::CellRendererText>();
        combo.pack_start(*r, false);
        combo.add_attribute(r->property_text(), tracker_filter_cols.displayname);
    }

    {
        /*Count Number*/
        auto* r = number_renderer_new();
        combo.pack_end(*r, true);
        combo.set_cell_data_func(*r, [r](auto const& iter) { render_number_func(*r, iter); });
    }
}


bool FilterBar::Impl::is_it_a_separator(Gtk::TreeModel::const_iterator const& iter)
{
    return iter->get_value(tracker_filter_cols.type) == TrackerSeparator;
}


void FilterBar::Impl::render_pixbuf_func(Gtk::CellRendererPixbuf& cell_renderer, Gtk::TreeModel::const_iterator const& iter)
{
    cell_renderer.property_width() = TrackerType{ iter->get_value(tracker_filter_cols.type) } == TrackerType::HOST ? 20 : 0;
}


void FilterBar::Impl::render_number_func(Gtk::CellRendererText& cell_renderer, Gtk::TreeModel::const_iterator const& iter)
{
    auto const count = iter->get_value(tracker_filter_cols.count);
    cell_renderer.property_text() = count >= 0 ? fmt::format("{:L}", count) : "";
}


Gtk::CellRendererText* FilterBar::Impl::number_renderer_new()
{
    auto* r = Gtk::make_managed<Gtk::CellRendererText>();

    r->property_alignment() = TR_PANGO_ALIGNMENT(RIGHT);
    r->property_weight() = TR_PANGO_WEIGHT(ULTRALIGHT);
    r->property_xalign() = 1.0;
    r->property_xpad() = GUI_PAD;

    return r;
}



void FilterBar::Impl::update_filter_tracker()
{
    /* set the active tracker type & host from the tracker combobox */
    if (auto const iter = tracker_->get_active(); iter)
    {
        filter_->set_tracker(
            static_cast<TrackerType>(iter->get_value(tracker_filter_cols.type)),
            iter->get_value(tracker_filter_cols.sitename));
    }
    else
    {
        filter_->set_tracker(TrackerType::ALL, {});
    }
}






/***
*****************************************************(ACTIVITY)**********************************************************
***/
namespace
{

    class ActivityFilterModelColumns : public Gtk::TreeModelColumnRecord
    {
        public:
            ActivityFilterModelColumns() noexcept
            {
                add(name);
                add(count);
                add(type);
                add(icon_name);
            }

            Gtk::TreeModelColumn<Glib::ustring> name;
            Gtk::TreeModelColumn<int> count;
            Gtk::TreeModelColumn<ActivityType> type;
            Gtk::TreeModelColumn<Glib::ustring> icon_name;
    };

    ActivityFilterModelColumns const activity_filter_cols;

} //anonymous namespace


bool FilterBar::Impl::activity_is_it_a_separator(Gtk::TreeModel::const_iterator const& iter)
{
    return iter->get_value(activity_filter_cols.type) == ActivitySeparator;
}

void FilterBar::Impl::status_model_update_count(Gtk::TreeModel::iterator const& iter, int n)
{
            // std::cout << ",,,FilterBar::Impl::status_model_update_count,  " << n << std::endl;
    if (n != iter->get_value(activity_filter_cols.count))
    {
        iter->set_value(activity_filter_cols.count, n);
    }
}

bool FilterBar::Impl::activity_filter_model_update()
{

    /*get raw_model_ of Session::Impl*/
    auto const torrents_model = core_->get_model();
    /*iterate each row, Checking,Paused,Downloading,.., etc.*/
    for (auto& row : activity_model_->children())
    {
        auto const type = row.get_value(activity_filter_cols.type);
        if (type == ActivitySeparator)
        {
            continue;
        }

        auto hits = 0;

        for (auto i = 0U, count = torrents_model->get_n_items(); i < count; ++i)
        {
            auto const torrent = gtr_ptr_dynamic_cast<Torrent>(torrents_model->get_object(i));
            if (torrent != nullptr && TorrentFilter::match_activity(*torrent, static_cast<ActivityType>(type)))
            {
                ++hits;
            }
        }
    
        status_model_update_count(TR_GTK_TREE_MODEL_CHILD_ITER(row), hits);
    }

    return false;
}



Glib::RefPtr<Gtk::ListStore> FilterBar::Impl::activity_filter_model_new()
{
    struct FilterTypeInfo
    {
        ActivityType type;
        char const* context;
        char const* name;
        char const* icon_name;
    };

    static auto constexpr types = std::array<FilterTypeInfo, 9>({ {
        { ActivityType::ALL, nullptr, N_("All"), nullptr },
        { ActivityType{ -1 }, nullptr, nullptr, nullptr },
        // { ActivityType::ACTIVE, nullptr, N_("Active"), "system-run" },
        { ActivityType::DOWNLOADING, "Verb", NC_("Verb", "Downloading"), "network-receive" },
        { ActivityType::DL_METADATA, "Verb", NC_("Verb", "Downloading metadata"), "network-receive" },
        { ActivityType::SEEDING, "Verb", NC_("Verb", "Seeding"), "network-transmit" },
        { ActivityType::PAUSED, nullptr, N_("Paused"), "media-playback-pause" },
        { ActivityType::FINISHED, nullptr, N_("Finished"), "media-playback-stop" },
        { ActivityType::CHECKING, "Verb", NC_("Verb", "Checkinging"), "view-refresh" },
        { ActivityType::ERROR, nullptr, N_("Error"), "dialog-error" }
    } });

    auto store = Gtk::ListStore::create(activity_filter_cols);

    for (auto const& type : types)
    {
        auto const name = type.name != nullptr ?
            Glib::ustring(type.context != nullptr ? g_dpgettext2(nullptr, type.context, type.name) : _(type.name)) :
            Glib::ustring();
        auto const iter = store->append();
        iter->set_value(activity_filter_cols.name, name);
        iter->set_value(activity_filter_cols.type, type.type);
        iter->set_value(activity_filter_cols.icon_name, Glib::ustring(type.icon_name != nullptr ? type.icon_name : ""));
    }

    return store;
}


void FilterBar::Impl::render_activity_pixbuf_func(
    Gtk::CellRendererPixbuf& cell_renderer,
    Gtk::TreeModel::const_iterator const& iter)
{
    auto const type = ActivityType{ iter->get_value(activity_filter_cols.type) };
    cell_renderer.property_width() = type == ActivityType::ALL ? 0 : 20;
    cell_renderer.property_ypad() = type == ActivityType::ALL ? 0 : 2;
}

void FilterBar::Impl::activity_combo_box_init(Gtk::ComboBox& combo)
{
    combo.set_model(activity_model_);
    combo.set_row_separator_func(sigc::hide<0>(&Impl::activity_is_it_a_separator));
    combo.set_active(0);

    {
        auto* r = Gtk::make_managed<Gtk::CellRendererPixbuf>();
        combo.pack_start(*r, false);
        combo.add_attribute(r->property_icon_name(), activity_filter_cols.icon_name);
        combo.set_cell_data_func(*r, [r](auto const& iter) { render_activity_pixbuf_func(*r, iter); });
    }

    {
        auto* r = Gtk::make_managed<Gtk::CellRendererText>();
        combo.pack_start(*r, true);
        combo.add_attribute(r->property_text(), activity_filter_cols.name);
    }

    {
        auto* r = number_renderer_new();
        combo.pack_end(*r, true);
        combo.set_cell_data_func(*r, [r](auto const& iter) { render_number_func(*r, iter); });
    }
}




void FilterBar::Impl::update_filter_activity()
{
    /* set active_activity_type_ from the activity combobox */
    if (auto const iter = activity_->get_active(); iter)
    {
        filter_->set_activity(ActivityType{ iter->get_value(activity_filter_cols.type) });
    }
    else
    {
        filter_->set_activity(ActivityType::ALL);
    }
}





/******************************************Text Entry*******************************************************/
void FilterBar::Impl::update_filter_text()
{
    filter_->set_text(entry_->get_text());
}

bool FilterBar::Impl::update_count_label()
{
    /* get the visible count */
    auto const visibleCount = static_cast<int>(filter_model_->get_n_items());

    /* get the tracker count */
    int trackerCount = 0;
    if (auto const iter = tracker_->get_active(); iter)
    {
        trackerCount = iter->get_value(tracker_filter_cols.count);
    }

    /* get the activity count */
    int activityCount = 0;
    if (auto const iter = activity_->get_active(); iter)
    {
        activityCount = iter->get_value(activity_filter_cols.count);
    }

    /* set the text */
    if (auto const new_markup = visibleCount == std::min(activityCount, trackerCount) ?
            _("_Show:") :
            fmt::format(_("_Show {count:L} of:"), fmt::arg("count", visibleCount));
        new_markup != show_lb_->get_label().raw())
    {
        show_lb_->set_markup_with_mnemonic(new_markup);
    }

    return false;
}

void FilterBar::Impl::update_count_label_idle()
{
    if (!update_count_label_tag_.connected())
    {
        update_count_label_tag_ = Glib::signal_idle().connect(sigc::mem_fun(*this, &Impl::update_count_label));
    }
}

void FilterBar::Impl::update_filter_models(Torrent::ChangeFlags changes)
{               

    /*when these flag set , we should update activity filter*/
    static auto constexpr activity_flags = Torrent::ChangeFlag::TORRENT_STATE 
                                        | Torrent::ChangeFlag::ERROR_CODE 
                                        | Torrent::ChangeFlag::IS_FINISHED 
                                        | Torrent::ChangeFlag::IS_SEEDING
                                        | Torrent::ChangeFlag::IS_QUEUED
                                        | Torrent::ChangeFlag::IS_PAUSED;

    /*when this flag set , we should update activity filter*/
    static auto constexpr tracker_flags = Torrent::ChangeFlag::TRACKERS_HASH;

    if (changes.test(activity_flags))
    {
        activity_filter_model_update();
    }

    if (changes.test(tracker_flags))
    {
        tracker_filter_model_update();
    }

    filter_->update(changes);

    if (changes.test(activity_flags | tracker_flags))
    {
        update_count_label_idle();
    }
}



void FilterBar::Impl::update_filter_models_idle(Torrent::ChangeFlags changes)
{
    if (!update_filter_models_tag_.connected())
    {
        update_filter_models_tag_ = Glib::signal_idle().connect(
            [this, changes]()
            {
                update_filter_models(changes);
                return false;
            });
    }
}




/***
***********************************************CTOR*************************************************
***/


FilterBarExtraInit::FilterBarExtraInit()
    : ExtraClassInit(&FilterBarExtraInit::class_init, nullptr, &FilterBarExtraInit::instance_init)
{
}

void FilterBarExtraInit::class_init(void* klass, void* /*user_data*/)
{
    // printf ("(FilterBarExtraInit::class_init)\n");

    auto* const widget_klass = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_klass, gtr_get_full_resource_path("FilterBar.ui").c_str());

    gtk_widget_class_bind_template_child_full(widget_klass, "activity_combo", FALSE, 0);
    gtk_widget_class_bind_template_child_full(widget_klass, "tracker_combo", FALSE, 0);
    gtk_widget_class_bind_template_child_full(widget_klass, "text_entry", FALSE, 0);
    gtk_widget_class_bind_template_child_full(widget_klass, "show_label", FALSE, 0);
}

void FilterBarExtraInit::instance_init(GTypeInstance* instance, void* /*klass*/)
{
    // printf ("(FilterBarExtraInit::class_init)\n");

    gtk_widget_init_template(GTK_WIDGET(instance));
}



FilterBar::FilterBar()
    : Glib::ObjectBase(typeid(FilterBar))
{
}

FilterBar::FilterBar(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& /*builder*/,
    Glib::RefPtr<Session> const& core)
    : Glib::ObjectBase(typeid(FilterBar))
    , Gtk::Box(cast_item)
    , impl_(std::make_unique<Impl>(*this, core))
{
}



FilterBar::~FilterBar() /*= default;*/
{
    std::cout << "FilterBar::~FilterBar() " << std::endl;

}



FilterBar::Impl::Impl(FilterBar& widget, Glib::RefPtr<Session> const& core)
    : widget_(widget)

    , core_(core)

    , activity_model_(activity_filter_model_new())
    
    , tracker_model_(tracker_filter_model_new())

    /*Filter Activity Combobox*/
    , activity_(get_template_child<Gtk::ComboBox>("activity_combo"))

    /*Filter Tracker Combobox*/
    , tracker_(get_template_child<Gtk::ComboBox>("tracker_combo"))

    /*FIlter Torrent name or File name text Entry*/
    , entry_(get_template_child<Gtk::Entry>("text_entry"))

    , show_lb_(get_template_child<Gtk::Label>("show_label"))

{
    /*ListModel::signal_items_changed()*/
    update_filter_models_on_add_remove_tag_ = core_->get_model()->signal_items_changed().connect(
    [this](guint /*position*/, guint /*num removed*/, guint /*num added*/) 
    { 
        update_filter_models_idle(~Torrent::ChangeFlags()); 
    });


    update_filter_models_on_change_tag_ = core_->signal_torrents_changed().connect(
        sigc::hide<0>(sigc::mem_fun(*this, &Impl::update_filter_models_idle)));


    activity_filter_model_update();


    tracker_filter_model_update();


    activity_combo_box_init(*activity_);

    tracker_combo_box_init(*tracker_);


    /*update when TorrentFilter changed , updat the count label for both activity ,traker combo*/
    filter_->signal_changed().connect([this](auto /*changes*/) { update_count_label_idle(); });



    /********** */
    filter_model_ = FilterListModel<Torrent>::create(core_->get_sorted_model() /*Glib::RefPtr<Gtk::TreeModel>*/, filter_);



    /*update when tracker combobox chosen changed*/
    tracker_->signal_changed().connect(sigc::mem_fun(*this, &Impl::update_filter_tracker));

    /*update when acticity combobox chosen changed*/
    activity_->signal_changed().connect(sigc::mem_fun(*this, &Impl::update_filter_activity));

#if GTKMM_CHECK_VERSION(4, 0, 0)
    entry_->signal_icon_release().connect([this](auto /*icon_position*/) { entry_->set_text({}); });
#else
    entry_->signal_icon_release().connect([this](auto /*icon_position*/, auto const* /*event*/) { entry_->set_text({}); });
#endif

    /*update when text entry changed*/
    entry_->signal_changed().connect(sigc::mem_fun(*this, &Impl::update_filter_text));

}





FilterBar::Impl::~Impl()
{
    update_filter_models_on_change_tag_.disconnect();
    update_filter_models_on_add_remove_tag_.disconnect();
    update_filter_models_tag_.disconnect();
    update_count_label_tag_.disconnect();
}





/*Used for Mainwindow ,return Glib::RefPtr<Gtk::TreeModel>*/
Glib::RefPtr<FilterBar::Model> FilterBar::get_filter_model() const
{
    return impl_->get_filter_model();
}
Glib::RefPtr<FilterBar::Impl::FilterModel> FilterBar::Impl::get_filter_model() const
{
    return filter_model_;
}





template<typename T>
T* FilterBar::Impl::get_template_child(char const* name) const
{
    auto full_type_name = std::string("gtkmm__CustomObject_");
    Glib::append_canonical_typename(full_type_name, typeid(FilterBar).name());

    return Glib::wrap(G_TYPE_CHECK_INSTANCE_CAST(
        gtk_widget_get_template_child(GTK_WIDGET(widget_.gobj()), g_type_from_name(full_type_name.c_str()), name),
        T::get_base_type(),
        typename T::BaseObjectType));
}
