// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "OptionsDialog.h"
#include "FileList.h"
#include "FreeSpaceLabel.h"
#include "GtkCompat.h"
#include "PathButton.h"
#include "Prefs.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"

#include "tr-transmission.h"
#include "tr-file.h" /* tr_sys_path_is_same() */

#include <giomm/file.h>
#include <glibmm/i18n.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/spinbutton.h>

#include <memory>
#include <utility>
#include <iostream>


#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/settings_pack.hpp"
#include "libtorrent/load_torrent.hpp"
#include "libtorrent/magnet_uri.hpp"
using namespace lt;

using namespace std::string_view_literals;

/****
*****
****/

namespace
{

auto const ShowOptionsDialogChoice = "show_options_dialog"sv; // TODO(C++20): Use ""s



} //anonymous namespace







class OptionsDialog::Impl
{

public:
    Impl(
    OptionsDialog& dialog,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Session> const& core,
    lt::add_torrent_params && atp,
    std::string dottor_filename,
    std::string download_dir
    );

    ~Impl() /*= default*/;

    TR_DISABLE_COPY_MOVE(Impl)


private:
    void sourceChanged(PathButton* b);
    void downloadDirChanged(PathButton* b);
    void updatePerHasMeta();
    void addResponseCB(int response);


private:


    OptionsDialog& dialog_;
    Glib::RefPtr<Session> const& core_;
    



    //.torrent file path
    std::string tor_path_;
    
    std::string downloadDir_;

    Gtk::CheckButton* run_check_ = nullptr;
    Gtk::CheckButton* trash_check_ = nullptr;
    FreeSpaceLabel* freespace_label_ = nullptr;
    Gtk::SpinButton* priority_spin_ = nullptr;

    lt::add_torrent_params atp_regular_;

    FileList* file_list_ = nullptr;

    lt::settings_pack sett_;

    // sigc::connection priority_spin_tag_;

    bool isSrcChanged_ = {};

};



/**/
// void OptionsDialog::Impl::removeOldTorrent()
// {
//     // if (tor_ != nullptr)
//     // {
//         file_list_->clear();

//     // }
// }



void OptionsDialog::Impl::addResponseCB(int response)
{
    bool add_from_magnet = true;
    if(atp_regular_.ti)
    {
        add_from_magnet = false;
    }
    
    if (response == TR_GTK_RESPONSE_TYPE(ACCEPT))
    {
        int prio_val = priority_spin_->get_value_as_int();
        bool start_added = run_check_->get_active();
        bool trash_origin = trash_check_->get_active();


        if(!start_added)
        {
            // stop_when_ready will stop the torrent immediately when it's done
            // checking.
            atp_regular_.flags |= lt::torrent_flags::stop_when_ready;
        }

                                // printf("OptionsDialog::atp_regular before async_add_torrent\n");
                                // for(auto i : atp_regular_.file_priorities)
                                // {
                                //     std::printf("%d,", static_cast<int>(i));
                                // }
                                // std::printf("\n");

        auto& ses =core_->get_session();
        /*add torrent using the field `atp_regular_` */
        std::string mag_link = lt::make_magnet_uri(atp_regular_);

                    // std::cout << "maglink is " << mag_link << std::endl;

        ses.async_add_torrent(std::move(atp_regular_));
  

        
        if(!add_from_magnet)
        {            
            core_->fetchTorHandleCB(
            static_cast<tr_addtor_cb_t>(
                //dont't pass `this` in capture of lambda, 
                // If the object is destroyed before the lambda is executed 
                // (e.g., in asynchronous or deferred execution),
                //  dereferencing the pointer will result in undefined behavior

                [mag_link, prio_val, start_added, trash_origin, tor_path_copy=tor_path_ /*, this*/](lt::torrent_handle const& handle, Glib::RefPtr<Torrent> const& Tor, bool& add_or_load)
            {
                    std::cout << "hello from OptionsDialo::addResponseCB " << std::endl;
                    std::cout << "maglink is " << mag_link << std::endl;
    
                add_or_load = true;
                handle.set_priority(prio_val);
                // if(!start_added)
                // {
                //     /*
                //     pause has two procedures
                //         1.unset auto-managed
                //         2.pause()
                //     */
                //     handle.unset_flags(lt::torrent_flags::auto_managed);
                //     handle.pause(torrent_handle::graceful_pause);
                // }
                if(trash_origin)
                {   
                    gtr_file_trash_or_remove(tor_path_copy, nullptr/*tr_error* error*/);
                }

                return mag_link;
            }
            )
            );
        }
            
        else
        {
            core_->fetchTorHandleCB(
            static_cast<tr_addtor_cb_t>(
                // dont't pass `this` in capture of lambda, 
                // If the object is destroyed before the lambda is executed 
                // (e.g., in asynchronous or deferred execution),
                //  dereferencing the pointer will result in undefined behavior
            [mag_link, prio_val, start_added /*,this*/](lt::torrent_handle const& handle,  Glib::RefPtr<Torrent> const& Tor, bool& add_or_load)
            {

                        std::cout << "hello from OptionsDialo::addResponseCB ,magnet-uri Branch" << std::endl;
                        std::cout << "maglink is " << mag_link << std::endl;

                add_or_load = true;
                handle.set_priority(prio_val);
                // if(!start_added)
                // {
                //     /*
                //     pause has two procedures
                //         1.unset auto-managed
                //         2.pause()
                //     */
                //     handle.unset_flags(lt::torrent_flags::auto_managed);
                //     handle.pause(torrent_handle::graceful_pause);
                // }
                
                return mag_link;
            }
            )
            );
        }
        //save rencent download dir, so you can use the next time
        gtr_save_recent_dir('d', core_, downloadDir_);     
    }
    else if (response == TR_GTK_RESPONSE_TYPE(CANCEL))
    {
        //nothing intersesting need to be done here
    }

    dialog_.close();
}



//update FileList
void OptionsDialog::Impl::updatePerHasMeta()
{
    
    /*if add from magnetlink, obviously there is not torrent source file, so trash original checkbox is non-sense ,disable it*/
    bool has_metadata = false;
    if(atp_regular_.ti)
    {
        has_metadata = true;
    }
        
    
    trash_check_->set_sensitive(has_metadata);

    //remember to set save_path !
    atp_regular_.save_path = downloadDir_;

    /*if no metadata ,we disable file list*/
    if (!has_metadata)
    {
            // std::printf("OptionsDialog::Impl::updatePerHasMeta, no metadata \n");

        file_list_->clear();
        file_list_->set_sensitive(false);
        trash_check_->set_active(false);
    }
    else
    {
            // std::printf("OptionsDialog::Impl::updatePerHasMeta, hav metadata \n");
     
        file_list_->set_sensitive(true);
        //update fileList ,since we not have torrent handle or uniq id , use atp.ti instead 
        file_list_->set_torrent_when_add(atp_regular_);
    }
}



/*callback of *source* torrent file chooser */
void OptionsDialog::Impl::sourceChanged(PathButton* b)
{
    /*get source torrent chosen `full-path`*/
    std::string const chosen_path = b->get_filename();

    /*torrent file path is not empty*/
    if (!chosen_path.empty() && isSrcChanged_)
    {
        lt::add_torrent_params params = lt::load_torrent_file(chosen_path);

        if(params.ti && params.info_hashes == atp_regular_.info_hashes)
        {
            gtr_add_torrent_error_dialog(*b, params.ti->name(), tor_path_);
        }
        else
        {
            //update target atp
            atp_regular_ = params;
            tor_path_ = chosen_path;
            file_list_->clear();
        }

    }   

    updatePerHasMeta();

    if(isSrcChanged_ == false)
    {
        isSrcChanged_ = true;
    }

}



/*callback of destination_chooser */
void OptionsDialog::Impl::downloadDirChanged(PathButton* b)
{
    /*get chosen `full-path`*/
    auto const chosen_path = b->get_filename();
            // std::cout << "downloadDirChanged " << chosen_path << std::endl;
    if (!chosen_path.empty() && (downloadDir_.empty() || !tr_sys_path_is_same(chosen_path, downloadDir_)))
    {
        downloadDir_ = chosen_path;
        updatePerHasMeta();

        freespace_label_->set_dir(downloadDir_);
    }
}



namespace
{

template<typename FileChooserT>
void addTorrentFilters(FileChooserT* chooser)
{
    auto filter = Gtk::FileFilter::create();
    filter->set_name(_("Torrent files"));
    filter->add_pattern("*.torrent");
    chooser->add_filter(filter);

    filter = Gtk::FileFilter::create();
    filter->set_name(_("All files"));
    filter->add_pattern("*");
    chooser->add_filter(filter);
}

} //anonymous namespace




/*******************
*******CTOR*********
********************/



std::unique_ptr<OptionsDialog> OptionsDialog::create(
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    lt::add_torrent_params && atp,
    std::string dottor_filename,
    std::string download_dir
    )
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("OptionsDialog.ui"));
    return std::unique_ptr<OptionsDialog>(
        gtr_get_widget_derived<OptionsDialog>(builder, "OptionsDialog", parent, core, std::move(atp), dottor_filename, download_dir)
    );
}


OptionsDialog::OptionsDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    lt::add_torrent_params && atp,
    std::string dottor_filename,
    std::string download_dir
    )
    : Gtk::Dialog(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core, std::move(atp), dottor_filename, download_dir))
{
    set_transient_for(parent);
}

OptionsDialog::~OptionsDialog() /*= default;*/
{

    std::cout << "OptionsDialog::~OptionsDialog()" << std::endl;
}
OptionsDialog::Impl::~Impl()
{

    std::cout << "OptionsDialog::Impl::~Impl()" << std::endl;
}

OptionsDialog::Impl::Impl(
    OptionsDialog& dialog,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Session> const& core,
    lt::add_torrent_params && atp,
    std::string dottor_filename,
    std::string download_dir
    ): dialog_(dialog)
    , core_(core)
    , tor_path_(dottor_filename)
    , downloadDir_(download_dir)
    , run_check_(gtr_get_widget<Gtk::CheckButton>(builder, "start_check"))
    , trash_check_(gtr_get_widget<Gtk::CheckButton>(builder, "trash_check"))
    , freespace_label_(gtr_get_widget_derived<FreeSpaceLabel>(builder, "free_space_label", core_, downloadDir_))
    , priority_spin_(gtr_get_widget<Gtk::SpinButton>(builder, "priority_spin"))
    , atp_regular_(std::move(atp))
    , file_list_(gtr_get_widget_derived<FileList>(builder, "files_view_scroll", "files_view", core_, atp_regular_))
    , sett_(core->get_sett())

{

                        //std::cout << "whoo , tor_path_ is " << tor_path_ << std::endl;


    dialog_.set_default_response(TR_GTK_RESPONSE_TYPE(ACCEPT));
    dialog.signal_response().connect(sigc::mem_fun(*this, &Impl::addResponseCB));


    /*re-choose torrent file to add*/
    auto* source_chooser = gtr_get_widget_derived<PathButton>(builder, "source_button");
    addTorrentFilters(source_chooser);
    source_chooser->signal_selection_changed().connect([this, source_chooser]() { sourceChanged(source_chooser); });



    /*Destination download dir*/
            // std::cout << downloadDir_ << std::endl;
    auto* destination_chooser = gtr_get_widget_derived<PathButton>(builder, "destination_button");
    destination_chooser->set_filename(downloadDir_);
    destination_chooser->set_shortcut_folders(gtr_get_recent_dirs(sett_, 'd'));
    destination_chooser->signal_selection_changed().connect([this, destination_chooser]()
                                                            { downloadDirChanged(destination_chooser); });


    /*torren priority*/
    priority_spin_->set_adjustment(Gtk::Adjustment::create(0, 0, 255, 1));

    // priority_spin_tag_ = priority_spin_->signal_value_changed().connect(
    //     [this]() { 
    //         handle.set_priority(priority_spin_->get_value_as_int());
    // });



    /*load *Global* settings to init GUI checkbox ...*/
    auto const start_after_add = sett_.get_bool(settings_pack::TR_KEY_start_added_torrents);
    run_check_->set_active(start_after_add);

    auto const trash_after_add = sett_.get_bool(settings_pack::TR_KEY_trash_original_torrent_files);
    trash_check_->set_active(trash_after_add);


    /*has .torrent file aka the metadata*/
    if (!tor_path_.empty())
    {
            // std::printf("tor_path_ is NOT empty\n");

        source_chooser->set_filename(tor_path_);
    }
    /*has not the metadata yet, it is launched from a magnet link*/
    else
    {
            // std::printf("tor_path_ is empty\n");

        sourceChanged(source_chooser);
    }


    dialog_.get_widget_for_response(TR_GTK_RESPONSE_TYPE(ACCEPT))->grab_focus();
}













/****
**************************************TorrentFileChooserDialog**********************************************
****/

void TorrentFileChooserDialog::onOpenDialogResponse(int response, Glib::RefPtr<Session> const& core)
{
    if (response == TR_GTK_RESPONSE_TYPE(ACCEPT))
    {
        auto sett = core->get_sett();
        bool const do_prompt = get_choice(std::string(ShowOptionsDialogChoice)) == "true";
        
        auto const files = IF_GTKMM4(get_files2, get_files)();
        g_assert(!files.empty());

        // remember the path you previously open a torrent !!this setting can be serialzed 
        if (auto const folder = IF_GTKMM4(get_current_folder, get_current_folder_file)(); folder != nullptr)
        {
            sett.set_str(settings_pack::TR_KEY_open_dialog_dir, std::string(folder->get_path()));
        
        }
        else if (auto const parent = files.front()->get_parent(); parent != nullptr)
        {
            sett.set_str(settings_pack::TR_KEY_open_dialog_dir, std::string(parent->get_path()));
        }

        core->add_files(files, do_prompt);
    }
}




std::unique_ptr<TorrentFileChooserDialog> TorrentFileChooserDialog::create(
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core)
{
    return std::unique_ptr<TorrentFileChooserDialog>(new TorrentFileChooserDialog(parent, core));
}

TorrentFileChooserDialog::TorrentFileChooserDialog(Gtk::Window& parent, Glib::RefPtr<Session> const& core)
    : Gtk::FileChooserNative(_("Open a Torrent"), parent, TR_GTK_FILE_CHOOSER_ACTION(OPEN), _("_Open"), _("_Cancel"))
{
    set_modal(true);

    set_select_multiple(true);
    addTorrentFilters(this);
    signal_response().connect([this, core](int response) { onOpenDialogResponse(response, core); });
    auto sett = core->get_sett();

    if (auto const folder =  sett.get_str(settings_pack::TR_KEY_open_dialog_dir); !folder.empty())
    {
        IF_GTKMM4(set_current_folder, set_current_folder_file)(Gio::File::create_for_path(folder));
    }

    add_choice(std::string(ShowOptionsDialogChoice), _("Show options dialog"));
    /*load settings to init checkbox*/
    set_choice(std::string(ShowOptionsDialogChoice), sett.get_bool(settings_pack::TR_KEY_show_options_window)==true ? "true" : "false");

}








/***
********************************TorrentUrlChooserDialog*****************************************
***/
void TorrentUrlChooserDialog::onOpenURLResponse(int response, Gtk::Entry const& entry, Glib::RefPtr<Session> const& core)
{
    if (response == TR_GTK_RESPONSE_TYPE(CANCEL))
    {
        close();
    }
    else if (response == TR_GTK_RESPONSE_TYPE(ACCEPT))
    {
        auto const url = gtr_str_strip(entry.get_text());

        if (url.empty())
        {
            return;
        }

        if (core->add_from_url(url))
        {
            //add success, meaning that magnet-url is valid, we can close TorrentUrlChooserDialog now
            close();
        }
        else
        {
            gtr_unrecognized_url_dialog(*this, url);
        }
    }
}

std::unique_ptr<TorrentUrlChooserDialog> TorrentUrlChooserDialog::create(Gtk::Window& parent, Glib::RefPtr<Session> const& core)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("TorrentUrlChooserDialog.ui"));
    return std::unique_ptr<TorrentUrlChooserDialog>(
        gtr_get_widget_derived<TorrentUrlChooserDialog>(builder, "TorrentUrlChooserDialog", parent, core));
}

TorrentUrlChooserDialog::TorrentUrlChooserDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core)
    : Gtk::Dialog(cast_item)
{
    set_transient_for(parent);

    auto* const e = gtr_get_widget<Gtk::Entry>(builder, "url_entry");
    //auto sniffing the clipboard magnet-url
    gtr_paste_clipboard_url_into_entry(*e);

    signal_response().connect([this, e, core](int response) { onOpenURLResponse(response, *e, core); });

    if (e->get_text_length() == 0)
    {
        e->grab_focus();
    }
    else
    {
        get_widget_for_response(TR_GTK_RESPONSE_TYPE(ACCEPT))->grab_focus();
    }
}


TorrentUrlChooserDialog::~TorrentUrlChooserDialog()
{
    std::cout << "TorrentUrlChooserDialog::~TorrentUrlChooserDialog() " << std::endl;
}