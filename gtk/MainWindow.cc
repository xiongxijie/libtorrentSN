// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#include "MainWindow.h"
#include "Actions.h"
#include "FilterBar.h"
#include "GtkCompat.h"
#include "ListModelAdapter.h"
#include "Prefs.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Torrent.h"
#include "Utils.h"

#if !GTKMM_CHECK_VERSION(4, 0, 0)
#include "TorrentCellRenderer.h"
#endif

#include "tr-transmission.h"
#include "tr-values.h"

#include <gdkmm/cursor.h>
#include <gdkmm/rectangle.h>
#include <giomm/menu.h>
#include <giomm/menuitem.h>
#include <giomm/menumodel.h>
#include <giomm/simpleaction.h>
#include <giomm/simpleactiongroup.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>
#include <glibmm/variant.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>
#include <gtkmm/widget.h>
#include <gtkmm/window.h>

#if GTKMM_CHECK_VERSION(4, 0, 0)
#include <gtkmm/listitemfactory.h>
#include <gtkmm/multiselection.h>
#include <gtkmm/popovermenu.h>
#else
#include <gdkmm/display.h>
#include <gdkmm/window.h>
#include <gtkmm/menu.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/treeviewcolumn.h>
#endif

#include <array>
#include <memory>
#include <string>
#include <iostream>

#include "libtorrent/settings_pack.hpp"
using namespace lt;

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace libtransmission::Values;

using VariantInt = Glib::Variant<int>;
using VariantDouble = Glib::Variant<double>;
using VariantString = Glib::Variant<Glib::ustring>;





class MainWindow::Impl
{
    // struct OptionMenuInfo
    // {
    //     Glib::RefPtr<Gio::SimpleAction> action;
    //     Glib::RefPtr<Gio::MenuItem> on_item;
    //     Glib::RefPtr<Gio::Menu> section;
    // };

    using TorrentView = IF_GTKMM4(Gtk::ListView, Gtk::TreeView);
    using TorrentViewSelection = IF_GTKMM4(Gtk::MultiSelection, Gtk::TreeSelection);

public:
    Impl(
        MainWindow& window,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Glib::RefPtr<Gio::ActionGroup> const& actions,
        Glib::RefPtr<Session> const& core);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

    [[nodiscard]] Glib::RefPtr<TorrentViewSelection> get_selection() const;

    void refresh();

    void prefsChanged(int const key);

    auto& signal_selection_changed()
    {
        return signal_selection_changed_;
    }

private:
    void updateSpeeds();

    void init_view(TorrentView* view, Glib::RefPtr<FilterBar::Model> const& model);

    void on_popup_menu(double event_x, double event_y);




    // Glib::RefPtr<Gio::MenuModel> createStatsMenu();
    // Glib::RefPtr<Gio::MenuModel> createOptionsMenu();
    // Glib::RefPtr<Gio::MenuModel> createRatioMenu(Glib::RefPtr<Gio::SimpleActionGroup> const& actions);
    // Glib::RefPtr<Gio::MenuModel> createSpeedMenu(Glib::RefPtr<Gio::SimpleActionGroup> const& actions, tr_direction dir);




    // void updateStats();
    // void onOptionsClicked();
    // void syncAltSpeedButton();
    // void alt_speed_toggled_cb();
    // void onAltSpeedToggledIdle();
    // void onRatioSet(double ratio);
    // void onSpeedSet(tr_direction dir, int KBps);
    // void onRatioToggled(std::string const& action_name, bool enabled);
    // void onSpeedToggled(std::string const& action_name, tr_direction dir, bool enabled);
    // void status_menu_toggled_cb(std::string const& action_name, Glib::ustring const& val);

private:
    MainWindow& window_;

    //depends-on 
    Glib::RefPtr<Session> const core_;

    Gtk::ScrolledWindow* scroll_ = nullptr;

    TorrentView* view_ = nullptr;

    Gtk::Widget* toolbar_ = nullptr;

    FilterBar* filter_;
    Gtk::Widget* status_ = nullptr;
    Gtk::Label* ul_lb_ = nullptr;
    Gtk::Label* dl_lb_ = nullptr;


    sigc::signal<void()> signal_selection_changed_;

#if GTKMM_CHECK_VERSION(4, 0, 0)
    Glib::RefPtr<Gtk::ListItemFactory> item_factory_compact_;
    Glib::RefPtr<Gtk::ListItemFactory> item_factory_full_;
    Glib::RefPtr<Gtk::MultiSelection> selection_;
#else
    
    TorrentCellRenderer* renderer_ = nullptr;

    Gtk::TreeViewColumn* column_ = nullptr;

#endif

    IF_GTKMM4(Gtk::PopoverMenu*, Gtk::Menu*) popup_menu_ = nullptr;
    sigc::connection pref_handler_id_;


    // Gtk::Label* stats_lb_ = nullptr;
    // Gtk::Image* alt_speed_image_ = nullptr;
    // Gtk::ToggleButton* alt_speed_button_ = nullptr;
};

/***
****
***/

void MainWindow::Impl::on_popup_menu([[maybe_unused]] double event_x, [[maybe_unused]] double event_y)
{
    if (popup_menu_ == nullptr)
    {
        auto const menu = gtr_action_get_object<Gio::Menu>("main-window-popup");

#if GTKMM_CHECK_VERSION(4, 0, 0)
        popup_menu_ = Gtk::make_managed<Gtk::PopoverMenu>(menu, Gtk::PopoverMenu::Flags::NESTED);
        popup_menu_->set_parent(*view_);
        popup_menu_->set_has_arrow(false);
        popup_menu_->set_halign(view_->get_direction() == Gtk::TextDirection::RTL ? Gtk::Align::END : Gtk::Align::START);

        view_->signal_destroy().connect(
            [this]()
            {
                popup_menu_->unparent();
                popup_menu_ = nullptr;
            });
#else
        popup_menu_ = Gtk::make_managed<Gtk::Menu>(menu);
        popup_menu_->attach_to_widget(window_);
#endif
    }

#if GTKMM_CHECK_VERSION(4, 0, 0)
    popup_menu_->set_pointing_to({ static_cast<int>(event_x), static_cast<int>(event_y), 1, 1 });
    popup_menu_->popup();
#else
    popup_menu_->popup_at_pointer(nullptr);
#endif
}

namespace
{

#if GTKMM_CHECK_VERSION(4, 0, 0)

class GtrStrvBuilderDeleter
{
public:
    void operator()(GStrvBuilder* builder) const
    {
        if (builder != nullptr)
        {
            g_strv_builder_unref(builder);
        }
    }
};

using GtrStrvBuilderPtr = std::unique_ptr<GStrvBuilder, GtrStrvBuilderDeleter>;

GStrv gtr_strv_join(GObject* /*object*/, GStrv lhs, GStrv rhs)
{
    auto const builder = GtrStrvBuilderPtr(g_strv_builder_new());
    if (builder == nullptr)
    {
        return nullptr;
    }

    g_strv_builder_addv(builder.get(), const_cast<char const**>(lhs)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    g_strv_builder_addv(builder.get(), const_cast<char const**>(rhs)); // NOLINT(cppcoreguidelines-pro-type-const-cast)

    return g_strv_builder_end(builder.get());
}

#else

bool tree_view_search_equal_func(
    Glib::RefPtr<Gtk::TreeModel> const& /*model*/,
    int /*column*/,
    Glib::ustring const& key,
    Gtk::TreeModel::const_iterator const& iter)
{
    static auto const& self_col = Torrent::get_columns().self;

    auto const name = iter->get_value(self_col)->get_name_collated();
    return name.find(key.lowercase()) == Glib::ustring::npos;
}

#endif

} // anonymous namespace



/**********************************************************************************************/
void MainWindow::Impl::init_view(TorrentView* view, Glib::RefPtr<FilterBar::Model> const& model)
{
#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto const create_builder_list_item_factory = [](std::string const& filename)
    {
        auto builder_scope = Glib::wrap(G_OBJECT(gtk_builder_cscope_new()));
        gtk_builder_cscope_add_callback(GTK_BUILDER_CSCOPE(builder_scope->gobj()), gtr_strv_join);

        return Glib::wrap(gtk_builder_list_item_factory_new_from_resource(
            GTK_BUILDER_SCOPE(builder_scope->gobj()),
            gtr_get_full_resource_path(filename).c_str()));
    };

    item_factory_compact_ = create_builder_list_item_factory("TorrentListItemCompact.ui"s);
    item_factory_full_ = create_builder_list_item_factory("TorrentListItemFull.ui"s);

    view->signal_activate().connect([](guint /*position*/) { gtr_action_activate("show-torrent-properties"); });

    selection_ = Gtk::MultiSelection::create(model);
    selection_->signal_selection_changed().connect([this](guint /*position*/, guint /*n_items*/)
                                                   { signal_selection_changed_.emit(); });

    view->set_factory(gtr_pref_flag_get(TR_KEY_compact_view) ? item_factory_compact_ : item_factory_full_);
    view->set_model(selection_);
#else


    static auto const& torrent_cols = Torrent::get_columns();

    view->set_search_column(torrent_cols.name_collated);

    view->set_search_equal_func(&tree_view_search_equal_func);

    /*init column*/
    column_ = view->get_column(0);

    /*init render*/
    renderer_ = Gtk::make_managed<TorrentCellRenderer>();

    column_->pack_start(*renderer_, false);
    column_->add_attribute(renderer_->property_torrent(), torrent_cols.self);

    view->signal_popup_menu().connect_notify([this]() { on_popup_menu(0, 0); });


    view->signal_row_activated().connect([](auto const& /*path*/, auto* /*column*/)
                                         { gtr_action_activate("show-torrent-properties"); });

    view->set_model(model);


    view->get_selection()->signal_changed().connect([this]() { signal_selection_changed_.emit(); });

#endif

    setup_item_view_button_event_handling(
        *view,
        [this, view](guint /*button*/, TrGdkModifierType /*state*/, double view_x, double view_y, bool context_menu_requested)
        {
            return on_item_view_button_pressed(
                *view,
                view_x,
                view_y,
                context_menu_requested,
                sigc::mem_fun(*this, &Impl::on_popup_menu));
        },
        [view](double view_x, double view_y) { return on_item_view_button_released(*view, view_x, view_y); });
}




void MainWindow::Impl::prefsChanged(int const key)
{
    auto const& sett = core_-> get_sett();
    switch (key)
    {
    case lt::settings_pack::TR_KEY_compact_view:
#if GTKMM_CHECK_VERSION(4, 0, 0)
        view_->set_factory(gtr_pref_flag_get(key) ? item_factory_compact_ : item_factory_full_);
#else
        renderer_->property_compact() =  sett.get_bool(key);
        /* since the cell size has changed, we need gtktreeview to revalidate
         * its fixed-height mode values. Unfortunately there's not an API call
         * for that, but this seems to work */
        view_->set_fixed_height_mode(false);
        view_->set_row_separator_func({});
        view_->unset_row_separator_func();
        view_->set_fixed_height_mode(true);
#endif
        break;

    case lt::settings_pack::TR_KEY_show_statusbar:
        status_->set_visible(sett.get_bool(key));
        break;

    case lt::settings_pack::TR_KEY_show_filterbar:
        filter_->set_visible(sett.get_bool(key));
        break;

    case lt::settings_pack::TR_KEY_show_toolbar:
        toolbar_->set_visible(sett.get_bool(key));
        break;

    default:
        break;
    }
}

MainWindow::Impl::~Impl()
{
    std::printf("in MainWindow::Impl::~Impl\n");
    pref_handler_id_.disconnect();
}













/***
****  PUBLIC
***/

std::unique_ptr<MainWindow> MainWindow::create(
    Gtk::Application& app,
    Glib::RefPtr<Gio::ActionGroup> const& actions,
    Glib::RefPtr<Session> const& core)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("MainWindow.ui"));
    return std::unique_ptr<MainWindow>(gtr_get_widget_derived<MainWindow>(builder, "MainWindow", app, actions, core));
}

MainWindow::MainWindow(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Application& app,
    Glib::RefPtr<Gio::ActionGroup> const& actions,
    Glib::RefPtr<Session> const& core)
    : Gtk::ApplicationWindow(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, actions, core))
{
    app.add_window(*this);
}

MainWindow::~MainWindow() /*= default;*/
{
    std::cout << "MainWindow::~MainWindow() " << std::endl;
}

MainWindow::Impl::Impl(
    MainWindow& window,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Gio::ActionGroup> const& actions,
    Glib::RefPtr<Session> const& core)
    : window_(window)
    , core_(core)
    , scroll_(gtr_get_widget<Gtk::ScrolledWindow>(builder, "torrents_view_scroll"))
    , view_(gtr_get_widget<TorrentView>(builder, "torrents_view"))
    , toolbar_(gtr_get_widget<Gtk::Widget>(builder, "toolbar"))
    , filter_(gtr_get_widget_derived<FilterBar>(builder, "filterbar", core_))
    , status_(gtr_get_widget<Gtk::Widget>(builder, "statusbar"))
    , ul_lb_(gtr_get_widget<Gtk::Label>(builder, "upload_speed_label"))
    , dl_lb_(gtr_get_widget<Gtk::Label>(builder, "download_speed_label"))

{

    auto sett = core_->get_sett();

    /* make the window */
    window.set_title(Glib::get_application_name());
    window.set_default_size(sett.get_int(lt::settings_pack::TR_KEY_main_window_width), sett.get_int(lt::settings_pack::TR_KEY_main_window_height));
#if !GTKMM_CHECK_VERSION(4, 0, 0)
    window.move(sett.get_int(lt::settings_pack::TR_KEY_main_window_x), sett.get_int(lt::settings_pack::TR_KEY_main_window_y));
#endif

    if (sett.get_bool(lt::settings_pack::TR_KEY_main_window_is_maximized))
    {
        window.maximize();
    }

    window.insert_action_group("win", actions);




    /**
    *** Workarea
    **/

   /******** */
    init_view(view_, filter_->get_filter_model());

    {
        /* this is to determine the maximum width/height for the label */
        int width = 0;
        int height = 0;
        auto const pango_layout = ul_lb_->create_pango_layout("999.99 kB/s");
        pango_layout->get_pixel_size(width, height);
        ul_lb_->set_size_request(width, height);
        dl_lb_->set_size_request(width, height);
    }


    prefsChanged(lt::settings_pack::TR_KEY_compact_view);
    prefsChanged(lt::settings_pack::TR_KEY_show_filterbar);
    prefsChanged(lt::settings_pack::TR_KEY_show_statusbar);
    prefsChanged(lt::settings_pack::TR_KEY_show_toolbar);

    /* listen for prefs changes that affect the window */
    pref_handler_id_ = core_->signal_prefs_changed().connect(sigc::mem_fun(*this, &Impl::prefsChanged));

    refresh();

#if !GTKMM_CHECK_VERSION(4, 0, 0)
    /* prevent keyboard events being sent to the window first */
    window.signal_key_press_event().connect(
        [this](GdkEventKey* event) { return gtk_window_propagate_key_event(static_cast<Gtk::Window&>(window_).gobj(), event); },
        false);
    window.signal_key_release_event().connect(
        [this](GdkEventKey* event) { return gtk_window_propagate_key_event(static_cast<Gtk::Window&>(window_).gobj(), event); },
        false);
#endif
}






/* session global Speed */
void MainWindow::Impl::updateSpeeds()
{
    auto& ses = core_->get_session();
    
    if (ses.is_valid())
    {
        //global session speed
        auto global_dn_speed = Speed{core_->get_session_global_up_speed(), Speed::Units::Byps};
        auto global_up_speed = Speed{core_->get_session_global_down_speed(), Speed::Units::Byps};


        dl_lb_->set_text(fmt::format(fmt::runtime(_("{download_speed} ▼")), fmt::arg("download_speed", global_dn_speed.is_zero() ? "None" : global_dn_speed.to_string() )));
        // dl_lb_->set_visible(dn_count > 0);

        ul_lb_->set_text(fmt::format(fmt::runtime(_("{upload_speed} ▲")), fmt::arg("upload_speed", global_up_speed.is_zero() ? "None" : global_up_speed.to_string())));
        // ul_lb_->set_visible(dn_count > 0 || up_count > 0);
    }
}

void MainWindow::refresh()
{
    impl_->refresh();
}

void MainWindow::Impl::refresh()
{
    if (core_ != nullptr && core_->get_session().is_valid())
    {
        updateSpeeds();

    }
}

Glib::RefPtr<MainWindow::Impl::TorrentViewSelection> MainWindow::Impl::get_selection() const
{
    // return IF_GTKMM4(selection_, view_->get_selection().set_mode(Gtk::SelectionMode::MULTIPLE));
    
    auto ret = view_->get_selection();

    ret->set_mode(Gtk::SelectionMode::SELECTION_MULTIPLE);

    return ret;
}



void MainWindow::for_each_selected_torrent(std::function<void(Glib::RefPtr<Torrent> const&)> const& callback) const
{
    for_each_selected_torrent_until(sigc::bind_return(callback, false));
}

bool MainWindow::for_each_selected_torrent_until(std::function<bool(Glib::RefPtr<Torrent> const&)> const& callback) const
{
    auto const selection = impl_->get_selection();
    auto const model = selection->get_model();
    bool result = false;

#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto const selected_items = selection->get_selection(); // TODO(C++20): Move into the `for`
    for (auto const position : *selected_items)
    {
        if (callback(gtr_ptr_dynamic_cast<Torrent>(model->get_object(position))))
        {
            result = true;
            break;
        }
    }
#else
    static auto const& self_col = Torrent::get_columns().self;

    for (auto const& path : selection->get_selected_rows())
    {
        auto const torrent = Glib::make_refptr_for_instance(model->get_iter(path)->get_value(self_col));
        torrent->reference();
        if (callback(torrent))
        {
            result = true;
            break;
        }
    }
#endif

    return result;
}

void MainWindow::select_all()
{
    impl_->get_selection()->select_all();
}

void MainWindow::unselect_all()
{
    impl_->get_selection()->unselect_all();
}

void MainWindow::set_busy(bool isBusy)
{
    if (get_realized())
    {
#if GTKMM_CHECK_VERSION(4, 0, 0)
        auto const cursor = isBusy ? Gdk::Cursor::create("wait") : Glib::RefPtr<Gdk::Cursor>();
        set_cursor(cursor);
#else
        auto const display = get_display();
        auto const cursor = isBusy ? Gdk::Cursor::create(display, Gdk::WATCH) : Glib::RefPtr<Gdk::Cursor>();
        get_window()->set_cursor(cursor);
        display->flush();
#endif
    }
}

sigc::signal<void()>& MainWindow::signal_selection_changed()
{
    return impl_->signal_selection_changed();
}
