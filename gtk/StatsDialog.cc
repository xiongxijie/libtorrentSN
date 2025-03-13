// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "StatsDialog.h"

#include "GtkCompat.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"

#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/ustring.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>

#include <fmt/core.h>

#include <memory>

static auto constexpr TR_RESPONSE_RESET = 1;

class StatsDialog::Impl
{
public:
    Impl(StatsDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

private:
    bool updateStats();
    void dialogResponse(int response);

private:
    StatsDialog& dialog_;
    Glib::RefPtr<Session> const core_;

    Gtk::Label* one_up_lb_;
    Gtk::Label* one_down_lb_;
    Gtk::Label* one_time_lb_;

    Gtk::Label* all_up_lb_;
    Gtk::Label* all_down_lb_;
    Gtk::Label* all_time_lb_;
    Gtk::Label* all_sessions_lb_;

    sigc::connection update_stats_tag_;
};



namespace
{

void setLabel(Gtk::Label* l, Glib::ustring const& str)
{
    gtr_label_set_text(*l, str);
}


auto startedTimesText(std::uint64_t n)
{
    return fmt::format(ngettext("Started {count:L} time", "Started {count:L} times", n), fmt::arg("count", n));
}

} //anonymous namespace



bool StatsDialog::Impl::updateStats()
{
    std::uint64_t up{},dn{},sec{},times{},cur_sec{};
    core_->all_time_accum_stats(up, dn, sec, times,cur_sec);
    setLabel(all_sessions_lb_, startedTimesText(times));
    setLabel(all_up_lb_, tr_strlsize(up));
    setLabel(all_down_lb_, tr_strlsize(dn));
    setLabel(all_time_lb_, tr_format_time(sec));
    setLabel(one_time_lb_, tr_format_time(cur_sec));

    std::int64_t cur_up{},cur_dn{};
    core_->only_current_stats(cur_up, cur_dn);
    
            std::cout << "cur_up is " << cur_up << ", cur_dn is " << cur_dn << std::endl;

    setLabel(one_up_lb_, tr_strlsize(cur_up));
    setLabel(one_down_lb_, tr_strlsize(cur_dn));
    
    return true;//keep fireing timer
}



StatsDialog::Impl::~Impl()
{
        std::cout << "StatsDialog::Impl::~Impl()" << std::endl;
    update_stats_tag_.disconnect();
}



void StatsDialog::Impl::dialogResponse(int response)
{
    //reset statistics
    if (response == TR_RESPONSE_RESET)
    {
        auto w = std::make_shared<Gtk::MessageDialog>(
            dialog_,
            _("Reset your statistics?"),
            false,
            TR_GTK_MESSAGE_TYPE(QUESTION),
            TR_GTK_BUTTONS_TYPE(NONE),
            true);
        w->add_button(_("_Cancel"), TR_GTK_RESPONSE_TYPE(CANCEL));
        w->add_button(_("_Reset"), TR_RESPONSE_RESET);
        w->set_secondary_text(
            _("These statistics are for your information only. "
              "Resetting them doesn't affect the statistics logged by your BitTorrent trackers."));
        w->signal_response().connect(
            [this, w](int inner_response) mutable
            {
                if (inner_response == TR_RESPONSE_RESET)
                {
                    core_->all_time_accum_clear();
                    updateStats();
                }
                w.reset();
            });
        w->show();
    }
    //close StatsDialog
    if (response == TR_GTK_RESPONSE_TYPE(CLOSE))
    {
        //trigger dialog to be deleted
        dialog_.close();
    }

}


StatsDialog::StatsDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core)
    : Gtk::Dialog(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core))
{
    set_transient_for(parent);
}


StatsDialog::~StatsDialog() /*= default;*/
{
    std::cout << " StatsDialog::~StatsDialog() " << std::endl;
}

std::unique_ptr<StatsDialog> StatsDialog::create(Gtk::Window& parent, Glib::RefPtr<Session> const& core)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("StatsDialog.ui"));
    return std::unique_ptr<StatsDialog>(gtr_get_widget_derived<StatsDialog>(builder, "StatsDialog", parent, core));
}


StatsDialog::Impl::Impl(StatsDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core)
    : dialog_(dialog)
    , core_(core)
    , one_up_lb_(gtr_get_widget<Gtk::Label>(builder, "current_uploaded_value_label"))
    , one_down_lb_(gtr_get_widget<Gtk::Label>(builder, "current_downloaded_value_label"))
    , one_time_lb_(gtr_get_widget<Gtk::Label>(builder, "current_duration_value_label"))
    , all_up_lb_(gtr_get_widget<Gtk::Label>(builder, "total_uploaded_value_label"))
    , all_down_lb_(gtr_get_widget<Gtk::Label>(builder, "total_downloaded_value_label"))
    , all_time_lb_(gtr_get_widget<Gtk::Label>(builder, "total_duration_value_label"))
    , all_sessions_lb_(gtr_get_widget<Gtk::Label>(builder, "start_count_label"))

{
    dialog_.set_default_response(TR_GTK_RESPONSE_TYPE(CLOSE));
    dialog_.signal_response().connect(sigc::mem_fun(*this, &Impl::dialogResponse));
    updateStats();
    update_stats_tag_ = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &Impl::updateStats),
        SECONDARY_WINDOW_REFRESH_INTERVAL_SECONDS);
}




