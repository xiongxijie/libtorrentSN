// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "MakeDialog.h"
#include "GtkCompat.h"
#include "PathButton.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"


#include "tr-transmission.h"
#include "tr-error.h"
#include "tr-values.h"
#include "tr-log.h"

#include <giomm/file.h>
#include <glibmm/convert.h>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>
#include <glibmm/value.h>
#include <glibmm/vectorutils.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scale.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/spinbutton.h>

#if GTKMM_CHECK_VERSION(4, 0, 0)
#include <gtkmm/droptarget.h>
#else
#include <gdkmm/dragcontext.h>
#include <gtkmm/selectiondata.h>
#endif

#include <fmt/core.h>

#include <cstddef> 
#include <cstdint>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <functional>
#include <cstdio>
#include <mutex>
#include <atomic>

using namespace std::literals;
using namespace libtransmission::Values;

#if GTKMM_CHECK_VERSION(4, 0, 0)
using FileListValue = Glib::Value<GSList*>;
using FileListHandler = Glib::SListHandler<Glib::RefPtr<Gio::File>>;
#endif



#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/create_torrent.hpp"
#include "libtorrent/units.hpp"
#include "libtorrent/load_torrent.hpp"
#include "libtorrent/announce_entry.hpp"


using namespace lt;




namespace
{

class MakeProgressDialog : public Gtk::Dialog
{
public:
    MakeProgressDialog(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        lt::create_torrent& create_torrent_ref,
        std::string_view target,
        std::string_view src,
        Glib::RefPtr<Session> const& core
        );
    ~MakeProgressDialog() override;

    TR_DISABLE_COPY_MOVE(MakeProgressDialog)

    static std::unique_ptr<MakeProgressDialog> create(
        std::string_view target,
        std::string_view src,
        lt::create_torrent& create_torrent_ref,
        Glib::RefPtr<Session> const& core);

    [[nodiscard]] bool success() const
    {
        return success_;
    }

private:
    bool onProgressDialogRefresh();

    void onProgressDialogResponse(int response);

    void addTorrent();

    std::future<lt::error_code> make_checksums();

    std::pair<lt::piece_index_t, lt::piece_index_t> checksum_status();

    void cancel_checksums();

private:

    //underlying worker responsible for making this torrent
    lt::create_torrent & create_torrent_ref_;

    std::future<lt::error_code> future_;

    lt::piece_index_t cur_checksum_piece_ = {};

    //the desired new-made torrent path, eg. /home/pal/Downloads/piano.mp3.torrent
    std::string const target_;

    //the target file or dir which torrent generate from,  eg. /home/pal/Music/piano.mp3
    std::string const source_;

    Glib::RefPtr<Session> const core_;
    bool success_ = false;

    sigc::connection progress_tag_;
    Gtk::Label* progress_label_ = nullptr;
    Gtk::ProgressBar* progress_bar_ = nullptr;

    std::mutex mtx_;

    std::shared_ptr<std::atomic<bool>> cancel_flag_;
};


std::unique_ptr<MakeProgressDialog> MakeProgressDialog::create(
    std::string_view target,
    std::string_view src,
    lt::create_torrent& create_torrent_ref,
    Glib::RefPtr<Session> const& core) 
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("MakeProgressDialog.ui"));
    return std::unique_ptr<MakeProgressDialog>(gtr_get_widget_derived<MakeProgressDialog>(
        builder,
        "MakeProgressDialog",
        create_torrent_ref,
        target,
        src,
        core));
}

MakeProgressDialog::MakeProgressDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    lt::create_torrent& create_torrent_ref,
    std::string_view target,
    std::string_view src,
    Glib::RefPtr<Session> const& core)
    : Gtk::Dialog(cast_item)
    , create_torrent_ref_(create_torrent_ref)

    , target_(target)

    , source_(src)

    , core_(core)
    , progress_label_(gtr_get_widget<Gtk::Label>(builder, "progress_label"))
    , progress_bar_(gtr_get_widget<Gtk::ProgressBar>(builder, "progress_bar"))
{

    signal_response().connect(sigc::mem_fun(*this, &MakeProgressDialog::onProgressDialogResponse));
    future_ = make_checksums();

    progress_tag_ = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &MakeProgressDialog::onProgressDialogRefresh),
        SECONDARY_WINDOW_REFRESH_INTERVAL_SECONDS);
    onProgressDialogRefresh();
}




/*
    input : /home/pal/Desktop/123
    output : /home/pal/Desktop
*/
std::string branch_path(std::string const& f)
{
	if (f.empty()) return f;

#ifdef TORRENT_WINDOWS
	if (f == "\\\\") return "";
#endif
	if (f == "/") return "";

	auto len = f.size();
	// if the last character is / or \ ignore it
	if (f[len-1] == '/' || f[len-1] == '\\') --len;
	while (len > 0) {
		--len;
		if (f[len] == '/' || f[len] == '\\')
			break;
	}

	if (f[len] == '/' || f[len] == '\\') ++len;
	return std::string(f.c_str(), len);
}




// Generate piece checksums asynchronously
// - Runs in a worker thread because it can be time-consuming.
// - Can be cancelled with `cancelChecksums()` and polled with `checksumStatus()`
std::future<lt::error_code> MakeProgressDialog::make_checksums()
{       
    cancel_flag_ = std::make_shared<std::atomic<bool>>(false); // Initialize the flag

    return std::async(
        std::launch::async,
        [this]()
        {
                                        // printf("in std::async ... \n");
            lt::error_code ec;
            lt::set_piece_hashes_for_gtk(create_torrent_ref_/*create_torrent object*/, 
                branch_path(source_)/*the source dir of the file or dir to make a torrent from*/
		    ,[this] (lt::piece_index_t const p) {
                std::lock_guard<std::mutex> lock(mtx_);
                //the current pieces being calculated, it is an index
                cur_checksum_piece_ = p;
                            // printf("in lamdbda cur_checksum_piece: %d \n", static_cast<int>(cur_checksum_piece_));
    	    } /*callback in on_hash() in create_torrent.cpp*/
            ,ec
            ,cancel_flag_
            );
            
            return ec;
        });

}


// Returns the status of a `makeChecksums()` call:
// The current piece being tested and the total number of pieces in the torrent.
std::pair<lt::piece_index_t, lt::piece_index_t> MakeProgressDialog::checksum_status()
{
      std::lock_guard<std::mutex> lock(mtx_);

    auto const total_piece = create_torrent_ref_.num_pieces();
    return std::make_pair(cur_checksum_piece_, static_cast<lt::piece_index_t>(total_piece));
}

// Tell the `makeChecksums()` worker thread to cleanly exit ASAP.
void MakeProgressDialog::cancel_checksums() 
{
    printf("cancel checksums !\n");
    if (cancel_flag_) 
    {
        *cancel_flag_ = true;  // Signal cancellation
    }
}

//write new-made torrent to local .torrent file,  serialization
bool save_torrent_to_file(std::string_view filename, std::vector<char> const& v)
{
    tr_error tmp_err;/*Temp*/
    tr_file_save(filename, v, &tmp_err);

    if (tmp_err)
    {
        tr_logAddError(fmt::format(
            _("Couldn't save '{path}': {error} ({error_code})"),
            fmt::arg("path", filename),
            fmt::arg("error", tmp_err.message()),
            fmt::arg("error_code", tmp_err.code())));

        return false;/*Failure*/
    }

    return true;/*Success*/
}


bool MakeProgressDialog::onProgressDialogRefresh()
{
    auto const is_done = !future_.valid() || future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;

    if (is_done)
    {
        //if finished, we can write to file
        std::vector<char> new_made;
	    lt::bencode(std::back_inserter(new_made), create_torrent_ref_.generate());
                        
                        std::cout << "Make checksum Finished, write to local file !" << std::endl;

        save_torrent_to_file(target_ ,new_made);
        progress_tag_.disconnect();
    }

    // progress value
    auto percent_done = 1.0;
    lt::piece_index_t piece_index{};
    auto const [current, total] = checksum_status();

    if (!is_done)
    {
        percent_done = static_cast<double>(current) / total;
        piece_index = current;
    }

    // progress text
    auto str = std::string{};
    auto is_success = false;
    auto const base = Glib::path_get_basename(target_.c_str() );
    if (!is_done)
    {
        str = fmt::format(_("Creating '{path}'   ({current_pieces_idx}/{total_pieces_num})")
        , fmt::arg("path", base)
        , fmt::arg("current_pieces_idx", static_cast<std::int32_t>(current))
        , fmt::arg("total_pieces_num", static_cast<std::int32_t>(total))
        
        );
    }
    else
    {
        lt::error_code ec = future_.get();
        //success , no error
        if (!ec)
        {
            str = fmt::format(_("Created '{path}'"), fmt::arg("path", base));
            is_success = true;
        }
        //got error
        else
        {
            str = fmt::format(
                _("Couldn't create '{path}': {error} ({error_code})"),
                fmt::arg("path", base),
                fmt::arg("error", ec.message()),
                fmt::arg("error_code", ec.value()));
        }
    }

    gtr_label_set_text(*progress_label_, str);

    /* progress bar */
    if (piece_index == 0)
    {
        str.clear();
    }
    else
    {
        /* how much data we've scanned through to generate checksums */
        str = fmt::format(
            _("Scanned {file_size}"),
            fmt::arg("file_size", tr_strlsize(static_cast<uint64_t>(piece_index) * create_torrent_ref_.piece_length())));
    }

    progress_bar_->set_fraction(percent_done);
    progress_bar_->set_text(str);

    /* buttons */
    set_response_sensitive(TR_GTK_RESPONSE_TYPE(CANCEL), !is_done);
    set_response_sensitive(TR_GTK_RESPONSE_TYPE(CLOSE), is_done);
    set_response_sensitive(TR_GTK_RESPONSE_TYPE(ACCEPT), is_done && is_success);

    success_ = is_success;

    return true;//keep firing the timer
}


MakeProgressDialog::~MakeProgressDialog()
{
    progress_tag_.disconnect();
                std::cout  << "MakeProgressDialog::~MakeProgressDialog() " << std::endl;
}

/*
    addTorrent to session just after the making torrent,
    use the freshly-made torrent file 
*/
void MakeProgressDialog::addTorrent()
{
	lt::add_torrent_params atp = lt::load_torrent_file(target_);

    const char* parent_dir_raw = branch_path(target_).c_str(); 
    core_->add_impl(std::move(atp), target_.c_str(), parent_dir_raw);

}

void MakeProgressDialog::onProgressDialogResponse(int response)
{
        std::cout << "MakeProgressDialog::onProgressDialogResponse, response int is " << static_cast<int>(response) << std::endl;
    switch (response)
    {
    case TR_GTK_RESPONSE_TYPE(CANCEL):
    case TR_GTK_RESPONSE_TYPE(DELETE_EVENT)://triggered by directly click the small 'X' right-top of MakeProgressDialog
        cancel_checksums();
        close();//close the MakeProgressDialog, will automatically trigger ResponseType::DELETE_EVENT if it has not been triggered before
        break;

    //just add it to session
    case TR_GTK_RESPONSE_TYPE(ACCEPT):
        addTorrent();
        [[fallthrough]];

    case TR_GTK_RESPONSE_TYPE(CLOSE):
                printf("CLOSE MakeProgressDialog\n");
        close();//close the MakeProgressDialog, will automatically trigger ResponseType::DELETE_EVENT if it has not been triggered before
        break;

    default:
        g_assert(0 && "unhandled response");//shouldn't reach here
    }
}





} // anonymous namespace






class MakeDialog::Impl
{
public:
    Impl(MakeDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);
    ~Impl() /*= default*/;

    TR_DISABLE_COPY_MOVE(Impl)

private:
    void onSourceToggled(Gtk::CheckButton* tb, PathButton* chooser);
    void onChooserChosen(PathButton* chooser);
    void onResponse(int response);

#if GTKMM_CHECK_VERSION(4, 0, 0)
    bool on_drag_data_received(Glib::ValueBase const& value, double x, double y);
#else
    void on_drag_data_received(
        Glib::RefPtr<Gdk::DragContext> const& drag_context,
        int x,
        int y,
        Gtk::SelectionData const& selection_data,
        guint info,
        guint time_);
#endif

    bool set_dropped_source_path(std::string const& filename);

    void updatePiecesLabel();

    void setFilename(std::string_view filename);

    // void configurePieceSizeScale(uint32_t piece_size);

    void onPieceSizeUpdated();


private:

    MakeDialog& dialog_;
    Glib::RefPtr<Session> const core_;
    Gtk::CheckButton* file_radio_ = nullptr;
    PathButton* file_chooser_ = nullptr;
    Gtk::CheckButton* folder_radio_ = nullptr;
    PathButton* folder_chooser_ = nullptr;
    Gtk::Label* pieces_lb_ = nullptr;
    PathButton* destination_chooser_ = nullptr;
    Gtk::CheckButton* comment_check_ = nullptr;
    Gtk::Entry* comment_entry_ = nullptr;
    Gtk::CheckButton* createdby_check_ = nullptr;
    Gtk::Entry* createdby_entry_ = nullptr;
    Gtk::SpinButton* piece_size_spin_ = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> announce_text_buffer_;
    


    std::unique_ptr<MakeProgressDialog> progress_dialog_;
    sigc::connection piece_size_spin_tag_;
    int piece_size_ = 16*1024;//16Kib

    std::string file_or_dir_path_;
    std::optional<lt::create_torrent> create_torrent_;
    lt::file_storage file_storage_;

    // Gtk::Scale* piece_size_scale_ = nullptr;
    // Gtk::CheckButton* private_check_ = nullptr;
    // Gtk::CheckButton* source_check_ = nullptr;
    // Gtk::Entry* source_entry_ = nullptr;
    // std::optional<tr_metainfo_builder> builder_;

};



namespace
{

    std::vector<lt::announce_entry> parse_trackers_txt(std::string_view text)
    {   
        std::vector<lt::announce_entry> aes_copy;
        
        auto current_tier = std::uint8_t{ 0 };
        auto line = std::string_view{};

        //In EditTracker Entry : *000 url following a blank line means next tier, no blank line means same tier 000*
        /* 
        In Linux, you typically don't encounter '\r' (carriage return) unless the file originated from a system like Windows, which uses "\r\n" for line endings.
            Linux/Unix: Uses \n (newline) as the line ending character.
            Windows: Uses \r\n (carriage return + newline) as the line ending.
        */
        while (tr_strv_sep(&text, &line, '\n'))
        {
                        // std::cout << "line : " << line << std::endl;
            

            if (tr_strv_ends_with(line, '\r'))
            {
                line = line.substr(0, std::size(line) - 1); 
            }

            //remove leading or trailing space
            line = tr_strv_strip(line);

            //if this line is empty, means we increase the current_tier
            if (std::empty(line))
            {
                ++current_tier;
            
            }
            //otherwise, insert this line of same tier
            else
            {
                lt::announce_entry ae{};
                ae.url = std::string(line);
                ae.tier = current_tier;
                aes_copy.push_back(ae);
            }    
        }

        return aes_copy;


    }



} //anonymous namespace




void MakeDialog::Impl::onResponse(int response)
{
    //click close just do nothing and exit the MakeDialog
    if (response == TR_GTK_RESPONSE_TYPE(CLOSE) || response == TR_GTK_RESPONSE_TYPE(DELETE_EVENT))
    {
        dialog_.close();
        return;
    }

    /*if create_torrent obj is empty, which means we do not choose any file for folder to make the torrent with, so we just do nothing and return*/
    if (response != TR_GTK_RESPONSE_TYPE(ACCEPT) || !file_storage_.is_valid() || file_or_dir_path_.empty())
    {
        return;
    }
                        

    lt::create_flags_t flags = {};
    flags |= lt::create_torrent::v1_only;
    // initial create_torrent obj wrapped in std::optional, so we use .emplace() method here
    create_torrent_.emplace(file_storage_, piece_size_, flags);

    if(create_torrent_.has_value() == false)
    {
        return;
    }


    // destination path
    auto const dir = destination_chooser_->get_filename();
    //--for single file it's the file's name
    //--for folder(multiple files), it's the folder's name
    auto const base = Glib::path_get_basename( file_or_dir_path_.c_str() /*builder_->top()*/);
    //full path of the output torrent  file
    std::string target = fmt::format("{:s}/{:s}.torrent", dir, base);

                            std::cout << "MakeDialog, OnResponse-> target path: " << target <<
                            " source path: " << file_or_dir_path_ << std::endl;


    // set trackers
    auto trackers_vec = parse_trackers_txt(announce_text_buffer_->get_text(false).raw().c_str());
    for(auto& ae : trackers_vec)
    {
        create_torrent_->add_tracker(ae.url, ae.tier);
    }


    // set comment
    if (comment_check_->get_active())
    {
        create_torrent_->set_comment(comment_entry_->get_text().raw().c_str());
        // builder_->set_comment(comment_entry_->get_text().raw());
    }

    // set created by who
    if (createdby_check_->get_active())
    {
        create_torrent_->set_creator(createdby_entry_->get_text().raw().c_str());
        // builder_->set_comment(comment_entry_->get_text().raw());
    }



    // build the .torrent
    progress_dialog_ = MakeProgressDialog::create(target.c_str(), file_or_dir_path_.c_str(),*create_torrent_, core_);
    progress_dialog_->set_transient_for(dialog_);
    gtr_window_on_close(
        *progress_dialog_,
        [this]()
        {
            auto const success = progress_dialog_->success();
                                std::cout << "Destruct MakeProgressDialog on handler" << std::endl;
            progress_dialog_.reset();
            //success ,we also close the MakeDialog 
            if (success)
            {
                dialog_.close();
            }
        });
    progress_dialog_->show();
}



void MakeDialog::Impl::updatePiecesLabel()
{
    auto gstr = Glib::ustring();
        

                    // std::cout << "in updatePiecesLabel " << std::endl;


    if (!file_storage_.is_valid())
    {
        gstr += _("No source selected");
        // piece_size_scale_->set_visible(false);
    }
    else
    {          
        auto file_cnt = file_storage_.is_valid() ? file_storage_.num_files() : 0;
        auto total_sz = file_storage_.is_valid() ? file_storage_.total_size() : 0;

        //eg. 9.70GB in 1 file(591984 BitTorrent pieces @ 16KiB)
        gstr += fmt::format(
            ngettext("{total_size} in {file_count:L} file", "{total_size} in {file_count:L} files", file_cnt),
            fmt::arg("total_size", tr_strlsize(total_sz)),
            fmt::arg("file_count", file_cnt));
        gstr += ' ';
        gstr += fmt::format(
            ngettext(
                "({piece_count} BitTorrent piece @ {piece_size})",
                "({piece_count} BitTorrent pieces @ {piece_size})",
                file_storage_.num_pieces()),
            fmt::arg("piece_count", file_storage_.num_pieces()),
            fmt::arg("piece_size", Memory{ file_storage_.piece_length(), Memory::Units::Bytes }.to_string()));
    }

    pieces_lb_->set_text(gstr);
}



// void MakeDialog::Impl::configurePieceSizeScale(uint32_t piece_size)
// {
//     // the below lower & upper bounds would allow piece size selection between approx 1KiB - 64MiB
//     auto adjustment = Gtk::Adjustment::create(log2(piece_size), 10, 26, 1.0, 1.0);
//     piece_size_scale_->set_adjustment(adjustment);
//     piece_size_scale_->set_visible(true);
// }


void MakeDialog::Impl::setFilename(std::string_view filename)
{
    create_torrent_.reset();

    // Create a new file_storage object each time
    lt::file_storage file_storage_tmp;

    if (!filename.empty())
    {                               
	    lt::create_flags_t flags = {};
        //only make v1 torrent 
        //(a) add single file
        //(b) add a folder
        flags |= lt::create_torrent::v1_only;
        //initialize the file_storage object
	    lt::add_files(file_storage_tmp, std::string(filename), flags);
        file_storage_tmp.set_piece_length(piece_size_);
        //after set piece length, num pieces need manually calculated
        file_storage_tmp.set_num_pieces(lt::aux::calc_num_pieces(file_storage_tmp));
            
    }

    // Swap the temporary with file_storage_ to avoid unnecessary copies
    file_storage_.swap(file_storage_tmp);

    updatePiecesLabel();
}




void MakeDialog::Impl::onChooserChosen(PathButton* chooser)
{
    file_or_dir_path_ = chooser->get_filename();
    setFilename(chooser->get_filename());
}

//choose a single file or dir (which contains one or multiple files or dirs )
void MakeDialog::Impl::onSourceToggled(Gtk::CheckButton* tb, PathButton* chooser)
{
    if (tb->get_active())
    {
        onChooserChosen(chooser);
    }
}



bool MakeDialog::Impl::set_dropped_source_path(std::string const& filename)
{
    if (Glib::file_test(filename, TR_GLIB_FILE_TEST(IS_DIR)))
    {
        /* a directory was dragged onto the dialog... */
        folder_radio_->set_active(true);
        folder_chooser_->set_filename(filename);
        return true;
    }

    if (Glib::file_test(filename, TR_GLIB_FILE_TEST(IS_REGULAR)))
    {
        /* a file was dragged on to the dialog... */
        file_radio_->set_active(true);
        file_chooser_->set_filename(filename);
        return true;
    }

    return false;
}

#if GTKMM_CHECK_VERSION(4, 0, 0)

bool MakeDialog::Impl::on_drag_data_received(Glib::ValueBase const& value, double /*x*/, double /*y*/)
{
    if (G_VALUE_HOLDS(value.gobj(), GDK_TYPE_FILE_LIST))
    {
        FileListValue files_value;
        files_value.init(value.gobj());
        if (auto const files = FileListHandler::slist_to_vector(files_value.get(), Glib::OwnershipType::OWNERSHIP_NONE);
            !files.empty())
        {
            return set_dropped_source_path(files.front()->get_path());
        }
    }

    return false;
}

#else

void MakeDialog::Impl::on_drag_data_received(
    Glib::RefPtr<Gdk::DragContext> const& drag_context,
    int /*x*/,
    int /*y*/,
    Gtk::SelectionData const& selection_data,
    guint /*info*/,
    guint time_)
{
    bool success = false;

    if (auto const uris = selection_data.get_uris(); !uris.empty())
    {
        success = set_dropped_source_path(Glib::filename_from_uri(uris.front()));
    }

    drag_context->drag_finish(success, false, time_);
}

#endif




/***********
 ****CTOR****
 ************
 ************/

MakeDialog::MakeDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core)
    : Gtk::Dialog(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core))
{
    set_transient_for(parent);
}

MakeDialog::~MakeDialog() /*= default;*/
{
    std::cout << "MakeDialog::~MakeDialog()  " << std::endl;
}
MakeDialog::Impl::~Impl() /*= default;*/
{
    std::cout << "MakeDialog::Impl::~Impl()  " << std::endl;
}

std::unique_ptr<MakeDialog> MakeDialog::create(Gtk::Window& parent, Glib::RefPtr<Session> const& core)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("MakeDialog.ui"));
    return std::unique_ptr<MakeDialog>(gtr_get_widget_derived<MakeDialog>(builder, "MakeDialog", parent, core));
}

MakeDialog::Impl::Impl(MakeDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core)
    : dialog_(dialog)
    , core_(core)
    
    , file_radio_(gtr_get_widget<Gtk::CheckButton>(builder, "source_file_radio"))
    , file_chooser_(gtr_get_widget_derived<PathButton>(builder, "source_file_button"))
    
    , folder_radio_(gtr_get_widget<Gtk::CheckButton>(builder, "source_folder_radio"))
    , folder_chooser_(gtr_get_widget_derived<PathButton>(builder, "source_folder_button"))
    
    //file size and piece size or piece num
    , pieces_lb_(gtr_get_widget<Gtk::Label>(builder, "source_size_label"))
    
    //save new-made .torrent file to this localtion
    , destination_chooser_(gtr_get_widget_derived<PathButton>(builder, "destination_button"))
    
    //comment
    , comment_check_(gtr_get_widget<Gtk::CheckButton>(builder, "comment_check"))
    , comment_entry_(gtr_get_widget<Gtk::Entry>(builder, "comment_entry"))

    //created by
    , createdby_check_(gtr_get_widget<Gtk::CheckButton>(builder, "createdby_check"))
    , createdby_entry_(gtr_get_widget<Gtk::Entry>(builder, "createdby_entry"))
    
    //piece size
    , piece_size_spin_(gtr_get_widget<Gtk::SpinButton>(builder, "piecesz_spin_button"))

    //trackers
    , announce_text_buffer_(gtr_get_widget<Gtk::TextView>(builder, "trackers_view")->get_buffer())




{
    dialog_.signal_response().connect(sigc::mem_fun(*this, &Impl::onResponse));

    destination_chooser_->set_filename(Glib::get_user_special_dir(TR_GLIB_USER_DIRECTORY(DESKTOP)));

    //choose a folder as target to make torrent
    folder_radio_->signal_toggled().connect([this]() { onSourceToggled(folder_radio_, folder_chooser_); });
    folder_chooser_->signal_selection_changed().connect([this]() { onChooserChosen(folder_chooser_); });

    //choose single-file as target to make torrent
    file_radio_->signal_toggled().connect([this]() { onSourceToggled(file_radio_, file_chooser_); });
    file_chooser_->signal_selection_changed().connect([this]() { onChooserChosen(file_chooser_); });

    //number of piece to make this torrent
    pieces_lb_->set_markup(fmt::format("<i>{:s}</i>", _("No source selected")));


    //piece size spin button, you can choose the piece-size you want, delta is 1024 byte (aka, 1Kib) , default piece size is 16KiB=16384
    piece_size_spin_->set_adjustment(Gtk::Adjustment::create(16*1024/*16Kib=16384*/, 0, 1024*1024*2/*2MiB=2097152*/, 1024 /*1Kib*/));
    piece_size_spin_tag_ = piece_size_spin_->signal_value_changed().connect(
    [this]() { 
        piece_size_ = piece_size_spin_->get_value_as_int();

        if(file_storage_.is_valid())
        {
            file_storage_.set_piece_length(piece_size_);
            //after set piece length, num pieces need manually calculated
            file_storage_.set_num_pieces(lt::aux::calc_num_pieces(file_storage_));
        }

        updatePiecesLabel();
    });
 


#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto drop_controller = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY);
    drop_controller->signal_drop().connect(sigc::mem_fun(*this, &Impl::on_drag_data_received), false);
    dialog_.add_controller(drop_controller);
#else
    dialog_.drag_dest_set(Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
    dialog_.drag_dest_add_uri_targets();
    dialog_.signal_drag_data_received().connect(sigc::mem_fun(*this, &Impl::on_drag_data_received));
#endif
}


