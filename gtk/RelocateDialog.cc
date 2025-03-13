// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "RelocateDialog.h"
#include "GtkCompat.h"
#include "PathButton.h"
#include "Prefs.h" 
#include "Session.h"
#include "Utils.h"

#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/ustring.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/messagedialog.h>

#include <fmt/core.h>

#include <memory>
#include <string>
#include <cstdio>

#include "libtorrent/storage_defs.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/session_handle.hpp"


using namespace lt;
namespace
{

/*new save_path, an absolute path*/
std::string targetLocation;

}


class RelocateDialog::Impl
{
public:
    Impl(
        RelocateDialog& dialog,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Glib::RefPtr<Session> const& core,
        std::vector<std::uint32_t> const& uniq_ids);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

private:
    void onResponse(int response);

    bool onTimer();

    void movingStorageForOne();


private:
    RelocateDialog& dialog_;
    Glib::RefPtr<Session> const core_;
    std::vector<std::uint32_t> uniq_ids_;

    // int done_ = 0;

    /*whether move the source file if any*/
    bool do_move_ = false;
    //timer to check execution results periodically
    sigc::connection timer_;
    std::unique_ptr<Gtk::MessageDialog> message_dialog_;
    PathButton* chooser_ = nullptr;
    Gtk::CheckButton* move_radio_ = nullptr;



};



RelocateDialog::Impl::~Impl()
{
    timer_.disconnect();

                std::cout << "RelocateDialog::Impl::~Impl() " << std::endl;
}



//move storage for one  torrent, (we can multi-select Torrents and pop up RelocateDialog for them)
void RelocateDialog::Impl::movingStorageForOne()
{
    lt::torrent_handle const& handle = core_->find_torrent(uniq_ids_.back());

    if (handle.is_valid())
    {
        if(do_move_)
        {   
            handle.move_storage(targetLocation, lt::move_flags_t::always_replace_files);
            /*exec results notified by move_storage_alert*/
        }
        else
        {
            handle.move_storage(targetLocation, lt::move_flags_t::reset_save_path);
            /*exec results notified by move_storage_alert*/

        }

    }

    uniq_ids_.pop_back();

    /*refresh text of current Torrent in turn */
    message_dialog_->set_message(
        fmt::format(_("Moving '{torrent_name}'"), fmt::arg("torrent_name", handle.name())),
        true);
}





/* every once in awhile, check to see if the move is done.
 * if so, delete the dialog 
 */
bool RelocateDialog::Impl::onTimer()
{
    //we only care about errors or exceptions
    //post the error info of the move-storage execution results
    lt::error_code ec_copy =  core_->FetchMoveFileErr();
                                        printf("current torrent FetchMoveFileErr() from Session\n");
    /**got error, pop up dialog */
    if (ec_copy)
    {
        auto d = std::make_shared<Gtk::MessageDialog>(
            *message_dialog_,
            //print error message such as storage error
            fmt::format(_("Move Failed:'{err_msg}'"), fmt::arg("err_msg", ec_copy.message())),
         
            // _("Failed move storage"),

            false,
            TR_GTK_MESSAGE_TYPE(ERROR),
            TR_GTK_BUTTONS_TYPE(CLOSE),
            true);

        d->signal_response().connect(
            [this, d](int /*response*/) mutable
            {
                d.reset();
                message_dialog_.reset();
                dialog_.close();
            });

        d->show();
        
                                        printf("current torrent clear_move_file_err() from Session\n");

        //clear the current torrent's error 
        core_->clear_move_file_err();
        return false;
    }
    /*no error*/
    else
    {
        //keep move storage for one uniq ids
        if (!uniq_ids_.empty())
        {
            movingStorageForOne();
        }
        //we're done all, just close the RelocateDialog
        else
        {
            message_dialog_.reset();
            dialog_.close();
            return false;//stop the timer, over
        }
    }

    return true;//keep firing the timer, so keep calling this handler
}




void RelocateDialog::Impl::onResponse(int response)
{
    if (response == TR_GTK_RESPONSE_TYPE(APPLY))
    {
                    
        auto const location = chooser_->get_filename();

        //get RadioButton checked option
        //(a).Do moveing the file to that path
        //(b).the file is already in that path, No move, just modify save_path
        do_move_ = move_radio_->get_active();

        /* pop up a dialog saying that the work is in progress */
        message_dialog_ = std::make_unique<Gtk::MessageDialog>(
            dialog_,
            Glib::ustring(),
            false,
            TR_GTK_MESSAGE_TYPE(INFO),
            TR_GTK_BUTTONS_TYPE(CLOSE),
            true);
        message_dialog_->set_secondary_text(_("This may take a moment…"));
        message_dialog_->set_response_sensitive(TR_GTK_RESPONSE_TYPE(CLOSE), false);
        message_dialog_->show();

        /* remember this location for the next torrent */
        targetLocation = location;

        /* remember this location so that it can be the default dir next time */
        gtr_save_recent_dir('r', core_, location);

        /* start the move and periodically check its status */
        // done_ = TR_LOC_DONE;
        timer_ = Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &Impl::onTimer), 1);
        onTimer();
    }
    else
    {
        dialog_.close();
    }
}



RelocateDialog::RelocateDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    std::vector<std::uint32_t> const& uniq_ids)
    : Gtk::Dialog(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core, uniq_ids))
{
    set_transient_for(parent);
}



RelocateDialog::~RelocateDialog() /*= default;*/
{
    std::cout << "RelocateDialog::~RelocateDialog() " << std::endl;
}


std::unique_ptr<RelocateDialog> RelocateDialog::create(
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    std::vector<std::uint32_t> const& uniq_ids)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("RelocateDialog.ui"));
    return std::unique_ptr<RelocateDialog>(
        gtr_get_widget_derived<RelocateDialog>(builder, "RelocateDialog", parent, core, uniq_ids));


}



RelocateDialog::Impl::Impl(
    RelocateDialog& dialog,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Session> const& core,
    std::vector<std::uint32_t> const& uniq_ids)
    : dialog_(dialog)
    , core_(core)
    , uniq_ids_(uniq_ids)
    , chooser_(gtr_get_widget_derived<PathButton>(builder, "new_location_button"))
    , move_radio_(gtr_get_widget<Gtk::CheckButton>(builder, "move_data_radio"))
{

    dialog_.set_default_response(TR_GTK_RESPONSE_TYPE(CANCEL));
    dialog_.signal_response().connect(sigc::mem_fun(*this, &Impl::onResponse));



    /*init path chooser*/
    auto recent_dirs = gtr_get_recent_dirs( core_->get_sett() , 'r');
    if (recent_dirs.empty())
    {
        /* default to download dir */
        auto down_dir = core_->get_sett().get_str(settings_pack::TR_KEY_download_dir);
        chooser_->set_filename(down_dir);
    }
    else
    {
        /* set last used as target */
        chooser_->set_filename(recent_dirs.front());
        recent_dirs.pop_front();

        /* add remaining as shortcut */
        chooser_->set_shortcut_folders(recent_dirs);
    }

}





