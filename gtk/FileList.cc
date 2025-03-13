// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "FileList.h"

#include "GtkCompat.h"
#include "HigWorkarea.h" // GUI_PAD, GUI_PAD_BIG
#include "IconCache.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"

#include "tr-utils.h"



#include <giomm/icon.h>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/markup.h>
#include <glibmm/miscutils.h>
#include <glibmm/nodetree.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/cellrendererprogress.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/cellrenderertoggle.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include <fmt/core.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <iostream>

#include "libtorrent/units.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/download_priority.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/file_storage.hpp"




using namespace lt;
using namespace std::literals;

namespace
{

enum : uint16_t
{
    /* these two fields could be any number at all so long as they're not
     * TR_PRI_LOW, TR_PRI_NORMAL, TR_PRI_HIGH, true, or false */
    NOT_SET = 1000,
    MIXED = 1001
};

class FileModelColumns : public Gtk::TreeModelColumnRecord
{
public:
    FileModelColumns() noexcept
    {
        add(index);
        add(icon);
        add(label);
        add(label_esc);
        add(have);
        add(size);
        add(size_str);
        add(prog);
        add(prog_str);
        add(priority);
    }

    /*lt::file_index_t*/
    Gtk::TreeModelColumn<std::int32_t> index;

    /*Icon*/
    Gtk::TreeModelColumn<Glib::RefPtr<Gio::Icon>> icon;

    /*Name Diplay Column*/
    Gtk::TreeModelColumn<Glib::ustring> label;
    Gtk::TreeModelColumn<Glib::ustring> label_esc;

    /*It is Used to Calculate Dir progress, that is: we_have / total_size */
    Gtk::TreeModelColumn<std::int64_t> have;

    /*Size*/
    Gtk::TreeModelColumn<std::int64_t> size;
    Gtk::TreeModelColumn<Glib::ustring> size_str;

    /*Progress Percent value*/
    Gtk::TreeModelColumn<double> prog;
    Gtk::TreeModelColumn<Glib::ustring> prog_str;

    /*Priority*/
    Gtk::TreeModelColumn<std::uint8_t> priority;

};

FileModelColumns const file_cols;

} //anonymous namespace






class FileList::Impl
{
public:
    Impl(
        FileList& widget,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Glib::ustring const& view_name,
        Glib::RefPtr<Session> const& core,
        lt::torrent_handle const& handle_ref,
        lt::add_torrent_params & atp_ref
        );
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

    void set_torrent();

    void set_torrent_when_add(lt::add_torrent_params const& new_atp);

    void reset_torrent();



private:
    void clearData();
    
    void refresh();

    bool getAndSelectEventPath(double view_x, double view_y, Gtk::TreeViewColumn*& col, Gtk::TreeModel::Path& path);

    [[nodiscard]] std::vector<lt::file_index_t> getActiveFilesForPath(Gtk::TreeModel::Path const& path) const;
    [[nodiscard]] std::vector<lt::file_index_t> getSelectedFilesAndDescendants() const;
    [[nodiscard]] std::vector<lt::file_index_t> getSubtree(Gtk::TreeModel::Path const& path) const;

    bool onViewButtonPressed(guint button, TrGdkModifierType state, double view_x, double view_y);
    bool onViewPathToggled(Gtk::TreeViewColumn* col, Gtk::TreeModel::Path const& path);
    void onRowActivated(Gtk::TreeModel::Path const& path, Gtk::TreeViewColumn* col);
    void cell_edited_callback(Glib::ustring const& path_string, Glib::ustring const& newname);
    void on_rename_done(Glib::ustring const& path_string, Glib::ustring const& newname, std::int32_t failedCount);
    void on_rename_done_idle(Glib::ustring const& path_string, Glib::ustring const& newname, std::int32_t failedCount);


private:
    FileList& widget_;

    Glib::RefPtr<Session> const core_;

    //For DetailsDialog
    lt::torrent_handle const& handle_ref_;

    //For OptionsDialog, non-const, so we can modify it
    lt::add_torrent_params & atp_ref_;


    bool is_torrent_added_ = {};

    std::uint32_t uniq_id_= {};


    // GtkWidget* top_ = nullptr; // == widget_
    Gtk::TreeView* view_ = nullptr;
    Glib::RefPtr<Gtk::TreeStore> store_;
    
    sigc::connection timeout_tag_;

    //renaming file task
    std::queue<sigc::connection> rename_done_tags_;


};


/*static field init*/
lt::torrent_handle FileList::empty_handle_ = lt::torrent_handle();

lt::add_torrent_params FileList::empty_atp_ = lt::add_torrent_params();




void FileList::Impl::clearData()
{
    timeout_tag_.disconnect();
}

FileList::Impl::~Impl()
{
    while (!rename_done_tags_.empty())
    {
        rename_done_tags_.front().disconnect();
        rename_done_tags_.pop();
    }

    clearData();
                std::cout << "FileList::Impl::~Impl()" << std::endl;
}





/*####################################################################################################*/
namespace
{

    struct RefreshData
    {
        int sort_column_id;
        bool resort_needed;
        Glib::RefPtr<Torrent> const& Tor;
    };

    bool refreshFilesForeach(
        Glib::RefPtr<Gtk::TreeStore> const& store,
        Gtk::TreeModel::iterator const& iter,
        RefreshData& refresh_data,
        lt::add_torrent_params const& atp)
    {
            
            // std::printf("enter refreshFilesForeach\n");

        bool const is_file = iter->children().empty();

        auto const old_size = iter->get_value(file_cols.size);
        auto const old_have = iter->get_value(file_cols.have);
        auto const old_progress = iter->get_value(file_cols.prog);
        auto const old_priority = iter->get_value(file_cols.priority);

        auto new_have = decltype(old_have){};
        auto new_priority = int{};
        auto new_progress = double{};
        auto new_size = decltype(old_size){};

        //single file
        if (is_file)
        {
            //lt::file_index_t
            auto const index = iter->get_value(file_cols.index);
    
            //on OptionsDialog, we dont have Torrnet yet, so priorty is set to download_priority::default just for initalization purpose
            if(!refresh_data.Tor)
            {
                auto const& atp_fp = atp.file_priorities;
                auto const& ti = atp.ti;
                auto const& fs = ti->files();

                /*it is set to zero , this is used to calculate dir progress*/
                new_have = 0;

                /*total size */
                new_size = fs.file_size(index);

                /*it is set to zero, since we haven't start download*/
                new_progress = 0.0;
                
                new_priority = static_cast<int>(atp_fp[index]);

            }
            //On DetailsDialog, Torrent RefPtr has already exists
            else
            {
                auto const& Tor = refresh_data.Tor;
                auto const& handle = Tor->get_handle_ref();
                auto const& ti = handle.torrent_file(); 
                auto const file_prio = handle.get_file_priorities();
                std::vector<std::int64_t>& progress_vec = Tor->get_file_progress_vec_ref();
                        // if(progress_vec.empty()==false)
                        // {
                        // std::cout << "progress_vec is not EMPTY" << std::endl;

                        // }
                /*size we have downloaded, this especially used to calculate dir progress*/
                if(!progress_vec.empty())
                {
                    new_have = progress_vec[index];
                }
                else
                {
                    new_have = 0;
                }

                /*total size*/
                new_size = ti->files().file_size(index);


                /*percent done*/
                new_progress = ti->files().file_size(index) > 0 ? 
                        static_cast<double>(1000 * new_have / ti->files().file_size(index)) : 1000;
                new_progress = static_cast<double>(new_progress/10.0);

                new_priority = static_cast<int>(file_prio[index]);
            }
        }
        //dir 
        else
        {
            new_priority = NOT_SET;

            /* since gtk_tree_model_foreach() is depth-first, we can
            * get the `sub' info by walking the immediate children */
            for (auto const& child : iter->children())
            {
                auto const child_have = child[file_cols.have];
                auto const child_size = child[file_cols.size];
                auto const child_priority = child[file_cols.priority];
                bool child_chosen = (child_priority != lt::dont_download);

                //sum up all child which has been enabled
                if (child_chosen)
                {
                    /*used to calculate dir's progress percent value*/
                    new_have += child_have;

                    /*total size of file enabled within this directory*/
                    new_size += child_size;
                }

                if (new_priority == NOT_SET)
                {
                    new_priority = child_priority;
                }
                else if (new_priority != child_priority)
                {
                    new_priority = MIXED;
                }
            }

            /* dir total progres = total_have / total_size */
            new_progress = new_size != 0 ? static_cast<double>(100.0 * new_have / new_size) : 100.0;
        }

        new_progress = std::clamp(new_progress, 0.0, 100.0);

        if (new_priority != old_priority)
        {
            /* Changing a value in the sort column can trigger a resort
            * which breaks this foreach () call. (See #3529)
            * As a workaround: if that's about to happen, temporarily disable
            * sorting until we finish walking the tree. */
            if (!refresh_data.resort_needed &&        
                    refresh_data.sort_column_id == file_cols.priority.index() && new_priority != old_priority)
            {
                refresh_data.resort_needed = true;
                store->set_sort_column(GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, TR_GTK_SORT_TYPE(ASCENDING));
            }
        }

        /*Size Column*/
        if (new_size != old_size)
        {
            //update data for calculate
            (*iter)[file_cols.size] = new_size;
            //update display for size
            (*iter)[file_cols.size_str] = tr_strlsize(new_size);
        }

        /*Recorded to calculate `Progress = Have/Size`*/
        if (new_have != old_have)
        {
            //update data for calculate
            (*iter)[file_cols.have] = new_have;
        }

        /*Have Column (xx% Percent)*/
        if(new_progress != old_progress)
        {
            //update data for calculate
            (*iter)[file_cols.prog] = new_progress;
            //update display for progress
            (*iter)[file_cols.prog_str] = fmt::format("{:.2f}%", new_progress);
        }

        /*Priority Column*/
        if (new_priority != old_priority)
        {
            (*iter)[file_cols.priority] = new_priority;
        }

        return false; /* keep walking */
    }





    void gtr_tree_model_foreach_postorder(Glib::RefPtr<Gtk::TreeModel> const& model, Gtk::TreeModel::SlotForeachIter const& func)
    {
        // std::cout << "\n\n````````````````" << std::endl;

        auto items = std::stack<Gtk::TreeModel::iterator>();
        if (auto const root_child_it = model->children().begin(); root_child_it)
        {
            items.push(root_child_it);
        }

        while (!items.empty())
        {
            while (items.top())
            {
                if (auto const child_it = items.top()->children().begin(); child_it)
                {

                        // auto child = *child_it;
                        // std::cout << "push >> " << child->get_value(file_cols.label)  << "<<" << std::endl;

                    items.push(child_it);
                }
                else
                {
                            // std::cout << "exec ---" << std::endl;
                    //for leaf, aka.file
                    func(items.top()++);
                }
            }

            items.pop();

            if (!items.empty())
            {
                            // std::cout << "exec2 ---" << std::endl;

                //for dir
                func(items.top()++);
            }
        }
    }


    //used for debug
    void traverse_children(const Gtk::TreeModel::Row& parent_row) 
    {
        // Get an iterator to the child rows
        Gtk::TreeModel::iterator child_iter = parent_row.children().begin();

        // Traverse through the child nodes
        for (auto row_iter = child_iter; row_iter != parent_row.children().end(); ++row_iter) {
            // Access the row data
            auto row = *row_iter;
            std::cout << "Index: " << row->get_value(file_cols.index) << ", Label: " << row->get_value(file_cols.label)  << std::endl;
        
            // Add more as needed...

            // Recursively traverse child rows
            traverse_children(row);
        }
    }


    void traverse_store(const Glib::RefPtr<Gtk::TreeStore>& store) 
    {
        // Get an iterator to the root nodes
        Gtk::TreeModel::iterator root_iter = store->children().begin();

        // Traverse through the root nodes
        for (auto row_iter = root_iter; row_iter != store->children().end(); ++row_iter) {
            // Access the row data
            auto row = *row_iter;
            std::cout << "Index: " << row->get_value(file_cols.index) << ", Label: " << row->get_value(file_cols.label) << std::endl;

            // Add more as needed...

            // Traverse child rows
            traverse_children(row);
        }
    }


} // anonymous namespace




void FileList::Impl::refresh()
{
    
    if(!handle_ref_.is_valid() && !atp_ref_.info_hashes.has_v1() )
    {
        widget_.clear();
    }
    else
    {
        //get sort column id
        Gtk::SortType order = TR_GTK_SORT_TYPE(ASCENDING);
        int sort_column_id = 0;
        store_->get_sort_column_id(sort_column_id, order);


            // std::printf("sort_column_id is %d \n", sort_column_id);


        //get Torrent RefPtr for get file progress
        Glib::RefPtr<Torrent>const& Tor =core_->find_Torrent(uniq_id_);


            // std::cout << "\n``````````````" << std::endl;
            // traverse_store(store_);
            // std::cout << "``````````````\n" << std::endl;



        RefreshData refresh_data{ sort_column_id, false, Tor};
        gtr_tree_model_foreach_postorder(
            store_,
            [this, &refresh_data](Gtk::TreeModel::iterator const& iter)
            { 
                    // auto child = *iter;
                    // std::cout << ">>>>>>>>>>>>> " << child->get_value(file_cols.label)  << std::endl;


                return refreshFilesForeach(store_, iter, refresh_data, atp_ref_); 
            }
        );

        if (refresh_data.resort_needed)
        {
            store_->set_sort_column(sort_column_id, order);
        }
    }



}






/*#####################################################################################################*/
namespace
{
    struct build_data
    {
        Gtk::Widget* w = nullptr;
    
        // Glib::RefPtr<Torrent>& Tor;

        // lt::torrent_handle handle;

        Gtk::TreeStore::iterator iter;

        Glib::RefPtr<Gtk::TreeStore> store;
    };


    //fileList Row
    struct row_struct
    {
        //file size 
        std::uint64_t length = 0;
        //file name
        Glib::ustring name;
        //file index, this not include directory
        lt::file_index_t index = {};
    };


    using FileRowNode = Glib::NodeTree<row_struct>;


    void buildTree(FileRowNode& node, build_data& build, std::vector<lt::download_priority_t> prios)
    {

        auto const& child_data = node.data();
        bool const isLeaf = node.child_count() == 0;


        auto const mime_type = isLeaf ? tr_get_mime_type_for_filename(child_data.name.raw()) : DirectoryMimeType;
        auto const icon = gtr_get_mime_type_icon(mime_type);
        auto name_esc = Glib::Markup::escape_text(child_data.name);

    
        auto priority = lt::default_priority;
        if(!prios.empty())
        {
            priority = isLeaf ? prios[child_data.index/*lt::file_index_t*/] : lt::default_priority /*4*/;
        }

                // std::cout << child_data.index << " " << child_data.length << " " << child_data.name << std::endl;
            
        auto const child_iter = build.store->prepend(build.iter->children());
        /* file_index_t */
        (*child_iter)[file_cols.index] = child_data.index;
        /* file size */
        (*child_iter)[file_cols.size] = child_data.length;
        (*child_iter)[file_cols.size_str] = tr_strlsize(child_data.length);
        /* name for the node , either dirname or filename*/
        (*child_iter)[file_cols.label] = child_data.name;
        /*tooltip name*/
        (*child_iter)[file_cols.label_esc] = name_esc;
        /*priority*/
        (*child_iter)[file_cols.priority] = priority;
        /*icon*/
        (*child_iter)[file_cols.icon] = icon;

        //if it is dir
        if (!isLeaf)
        {
            build_data b = build;
            b.iter = child_iter;
            node.foreach ([&b, &prios](auto& child_node) { buildTree(child_node, b, prios); }, TR_GLIB_NODE_TREE_TRAVERSE_FLAGS(FileRowNode, ALL));
        }
    }

} //anonymous namespace



struct PairHash
{
    template<typename T1, typename T2>
    auto operator()(std::pair<T1, T2> const& pair) const
    {
        // std::cout << "pairhash:" << pair.second << std::endl;
        
        auto s = std::hash<T1>{}(pair.first) ^ std::hash<T2>{}(pair.second);
        // std::cout << "pair hash:"<< s << std::endl;
        return s;
    }
};


/*Used for OptionsDialog, 
eg. you changed torrent file in OptionsDialog, so new_atp passed in,modifying atp_ref_field
*/
void FileList::set_torrent_when_add(lt::add_torrent_params const& new_atp)
{
    impl_->set_torrent_when_add(new_atp);
}

void FileList::Impl::set_torrent_when_add(lt::add_torrent_params const& new_atp)
{
    if (store_ != nullptr && !store_->children().empty())
    {
        return;
    }
    
    
    /*update atp*/
    if(new_atp.info_hashes != atp_ref_.info_hashes || new_atp.save_path != atp_ref_.save_path)
    {
        //modify atp
        atp_ref_ = new_atp;
    }
            
            std::printf("FileList::Impl::set_torrent_when_add \n");
            // std::cout << "new_atp.save path is " << new_atp.save_path << std::endl;
            // std::cout << "atp_ref_.save path is " << atp_ref_.save_path << std::endl;


    /* unset the old fields */
    clearData();


    /* instantiate the model */
    store_ = Gtk::TreeStore::create(file_cols);


    //fetch ti in atp
    if(atp_ref_.ti)
    {
        auto const& ti = atp_ref_.ti;
        auto const& fs = ti->files();

        auto root = FileRowNode{};
                
        //name of the torrent for single file torrent, it is filename
        //for multi-file torrent, it is dirname
        if(fs.num_files() > 1)
        {
            auto& root_data = root.data();
            root_data.name = ti->name().c_str();
            root_data.index = -1;
            root_data.length = 0;
        }

        /*dont use string_view as the pair's second here, cauze string_view dont own resources ... */
        auto nodes = std::unordered_map<std::pair<FileRowNode* /*parent ptr*/, std::string /*token*/>, FileRowNode*, PairHash>{};

        /*std::string must outlive the std::string_view obj. so defined outside of the for loop*/
        std::string path_str{};
        /*
            reminder: for single-file torrent, below it will loop only once
        */
        for(lt::file_index_t const i : fs.file_range())
        {
            auto* parent = &root;     

                // std::cout << path_str_raw << std::endl;
                
                // std::printf(" : %s \n", path_str_raw.c_str());

            path_str = fs.file_path(i);
            std::string_view path = path_str ;
            std::string_view token = std::string_view{ };
            while (tr_strv_sep(&path, &token, '/'))
            {
                // std::printf("[%p, ", parent);

                // std::cout << token  << "]" << std::endl;

                // since string_view dont own resources, so when its underlying std::string out of scope.
                // try to print  string_view will print weird content, so conver it to a string
                auto*& node = nodes[std::make_pair(parent, std::string(token))];

                //add if do not have
                if (node == nullptr)
                {
                    //if `path` is empty, means it is a file (aka. a leaf in tree)
                    auto const is_leaf = std::empty(path);

                    node = parent->prepend_data({});
                    auto& node_data = node->data();

                    // if `path` is empty, `token` respond to file name
                    // if `path` is non-empty, `token` is like this,  eg. {Britain/Terasse} 
                    node_data.name = std::string{ token };

                    //file has its file_index_t , while child dir dont have
                    node_data.index = is_leaf ? /*file_index_t*/i : static_cast<lt::file_index_t>(-1);

                    auto const file_sz = fs.file_size(i);
                    node_data.length = is_leaf ? file_sz : 0;
                }

                parent = node;

            }
        }


        // for (const auto& [key, value] : nodes) 
        // {
        //     std::cout << "Key: " << key.second << std::endl;
        // }


        // now, add them to the model
        struct build_data build;
        build.w = &widget_;
        build.store = store_;


        /*resize the file_priorities in atp, cuz they are not allocated */
        auto & prios = atp_ref_.file_priorities;
    	prios.resize(atp_ref_.ti->num_files(), lt::default_priority);
        root.foreach (
            [&build, &prios](auto& child_node) 
            {   
                buildTree(child_node, build, prios); 
            },
            TR_GLIB_NODE_TREE_TRAVERSE_FLAGS(FileRowNode, ALL)
        );        
    }

    
    refresh();
    /*refresh at fixed time interval*/
    timeout_tag_ = Glib::signal_timeout().connect_seconds(
        [this]() { refresh(); return true; },
        SECONDARY_WINDOW_REFRESH_INTERVAL_SECONDS);

    view_->set_model(store_);

    /* set default sort by label */
    store_->set_sort_column(file_cols.label, TR_GTK_SORT_TYPE(ASCENDING));

    view_->expand_row(Gtk::TreeModel::Path("0"), false);

}




/*Used for DetailsDialog*/
void FileList::set_torrent()
{
    impl_->set_torrent();
}

void FileList::Impl::set_torrent()
{
    if (!handle_ref_.is_valid() || (store_ != nullptr && !store_->children().empty()))
    {
        return;
    }

    /* unset the old fields */
    clearData();

    /* instantiate the model */
    store_ = Gtk::TreeStore::create(file_cols);
    
    /* populate the model */

    if( handle_ref_.is_valid())
    {
        auto const& ti = handle_ref_.torrent_file();
        auto const& fs = ti->files();

        //accord to libtorrent::file_storage build a GNode tree of the files
        auto root = FileRowNode{};

        if(fs.num_files() > 1)
        {
            //name of the torrent for single file torrent, it is filename
            //for multi-file torrent, it is torrent root dirname
            auto& root_data = root.data();
            root_data.name = ti->name().c_str();
            root_data.index = -1;
            root_data.length = 0;
        }


        auto nodes = std::unordered_map<std::pair<FileRowNode* /*parent*/, std::string /*token*/>, FileRowNode*, PairHash>{};

        /*std::string must outlive the std::string_view obj. so defined outside of the for loop*/
        std::string path_str{};


        /*
            reminder: for single-file torrent, below it will loop only once
        */
        for(lt::file_index_t const i : fs.file_range())
        {
            auto* parent = &root;     
            path_str = fs.file_path(i);


            //do not use r-value `fs.file_path(i).c_str();`  here cuz the string_view will  print weird content, so use a varible instead, 
            std::string_view path = path_str ;
            std::string_view token = std::string_view{ };
       
            while (tr_strv_sep(&path, &token, '/'))
            {
                /*  
                    output of a four-file-torrent eg:

                 foramt: [path]```    [token]
                    Canada/Obama
                    {Obama} ```     {Canada}
                    {} ```          {Obama}
                    ------------------
                    
                    Canada/Britain/Terasse
                    {Britain/Terasse} ```   {Canada}
                    {Terasse} ```           {Britain}
                    {} ```                  {Terasse}
                    ------------------
                    
                    Canada/Britain/Sunak
                    {Britain/Sunak} ```     {Canada}
                    {Sunak} ```             {Britain}
                    {} ```                  {Sunak}
                    ------------------
                    
                    Canada/Geer
                    {Geer} ```              {Canada}
                    {} ```                  {Geer}
                    ------------------

                    tree view:
                            Canada
                           /   |   \
                        Obama Geer Britain(dir)
                                         /   \
                                       Terase Sunak

                  Reminder: when `path` is empty, the  corresponding token must be the file name, (not dirname 
                            and when 'path' is non-empty, the toekn is the dir's name

                */
                auto*& node = nodes[std::make_pair(parent, std::string(token))];
                //add if not have
                if (node == nullptr)
                {
                    //if `path` is empty, means it is a file (aka. a leaf in tree)
                    auto const is_leaf = std::empty(path);

                    node = parent->prepend_data({});
                    auto& node_data = node->data();

                    // if `path` is empty, `token` respond to file name
                    // if `path` is non-empty, `token` is like this,  eg. {Britain/Terasse} 
                    node_data.name = std::string{ token };

                    //file has its file_index_t , while child dir dont have
                    node_data.index = is_leaf ? /*file_index_t*/i : static_cast<lt::file_index_t>(-1);

                    auto const file_sz = fs.file_size(i);
                    node_data.length = is_leaf ? file_sz : 0;
                }
                parent = node;
            }
        }

        // now, add them to the model
        struct build_data build;
        build.w = &widget_;
        build.store = store_;

        /*query file priorities using torrent handle*/
        std::vector<lt::download_priority_t> prios = handle_ref_.get_file_priorities();   
        root.foreach (
            [&build, &prios](auto& child_node) 
            { 
                buildTree(child_node, build, prios); 
            },
            TR_GLIB_NODE_TREE_TRAVERSE_FLAGS(FileRowNode, ALL)
        );

    }

    refresh();
    /*refresh at fixed time interval*/
    timeout_tag_ = Glib::signal_timeout().connect_seconds(
        [this]() { refresh(); return true; },
        SECONDARY_WINDOW_REFRESH_INTERVAL_SECONDS);


    view_->set_model(store_);

    /* set default sort by label */
    store_->set_sort_column(file_cols.label, TR_GTK_SORT_TYPE(ASCENDING));

    view_->expand_row(Gtk::TreeModel::Path("0"), false);
    // view_->expand_all();
}





void FileList::clear()
{
    impl_->reset_torrent();
}


void FileList::Impl::reset_torrent()
{
    clearData();

    store_ = Gtk::TreeStore::create(file_cols);
    view_->set_model(store_);
}




namespace
{

    void renderPriority(Gtk::CellRenderer* renderer, Gtk::TreeModel::const_iterator const& iter)
    {
        auto* const text_renderer = dynamic_cast<Gtk::CellRendererText*>(renderer);
        g_assert(text_renderer != nullptr);
        if (text_renderer == nullptr)
        {
            return;
        }

        Glib::ustring text;

        switch (auto const priority = iter->get_value(file_cols.priority); priority)
        {
            case lt::dont_download:
                text = _("Dont Dn");
                break;

            case lt::top_priority:
                text = _("Highest");
                break;

            case lt::default_priority:
                text = _("Normal");
                break;

            case lt::low_priority:
                text = _("Low");
                break;

            default:
                text = _("Mixed");
                break;
        }

        text_renderer->property_text() = text;
    }


    bool getSubtreeForeach(
        Gtk::TreeModel::Path const& path,
        Gtk::TreeModel::iterator const& iter,
        Gtk::TreeModel::Path const& subtree_path,
        std::vector<lt::file_index_t>& indexBuf)
    {
        if (bool const is_file = iter->children().empty(); is_file)
        {
            if (path == subtree_path || path.is_descendant(subtree_path))
            {
                indexBuf.push_back(iter->get_value(file_cols.index));
            }
        }

        return false; /* keep walking */
    }

    /*query save_path from torrent_status*/
    std::string build_filename(Glib::RefPtr<Torrent> const& Tor, Gtk::TreeModel::iterator const& iter)
    {
        std::vector<std::string> tokens;
        for (auto child = iter; child; child = child->parent())
        {
            tokens.push_back(child->get_value(file_cols.label));
        }

        auto const& st = Tor->get_status_ref();
        if(!st.handle.is_valid())
        {
            return {};
        }
        /*concatenate tokens to form path*/
        tokens.emplace_back(st.save_path);
        std::reverse(tokens.begin(), tokens.end());
        return Glib::build_filename(tokens);
    }

    std::optional<std::string> get_filename_to_open(Glib::RefPtr<Torrent> const& Tor, Gtk::TreeModel::iterator const& iter)
    {
        auto file = Gio::File::create_for_path(build_filename(Tor, iter));
  
        // if the selected file is complete, use it
        if (iter->get_value(file_cols.prog) == 100.0 && file->query_exists())
        {
            return file->get_path();
        }

        //if not, use nearest existing ancestor dir instead
        for (;;)
        {
            file = file->get_parent();

            if (!file)
            {
                return {};
            }

            if (file->query_exists())
            {
                return file->get_path();
            }
        }
    }


    bool getSelectedFilesForeach(
        Gtk::TreeModel::iterator const& iter,
        Glib::RefPtr<Gtk::TreeSelection> const& sel,
        std::vector<lt::file_index_t>& indexBuf)
    {
        if (bool const is_file = iter->children().empty(); is_file)
        {
            /* active means: if it's selected or any ancestor is selected */
            bool is_active = sel->is_selected(iter);

            if (!is_active)
            {
                for (auto walk = iter->parent(); !is_active && walk; walk = walk->parent())
                {
                    is_active = sel->is_selected(walk);
                }
            }

            if (is_active)
            {
                indexBuf.push_back(iter->get_value(file_cols.index));
            }
        }

        return false; /* keep walking */
    }


} //anonymous namespace






//################################################################################################################

std::vector<lt::file_index_t> FileList::Impl::getSelectedFilesAndDescendants() const
{
    auto const sel = view_->get_selection();
    std::vector<lt::file_index_t> indexBuf;
    store_->foreach_iter([&sel, &indexBuf](Gtk::TreeModel::iterator const& iter)
                         { return getSelectedFilesForeach(iter, sel, indexBuf); });
    return indexBuf;
}


std::vector<lt::file_index_t> FileList::Impl::getSubtree(Gtk::TreeModel::Path const& subtree_path) const
{
    std::vector<lt::file_index_t> indexBuf;
    store_->foreach ([&subtree_path, &indexBuf](Gtk::TreeModel::Path const& path, Gtk::TreeModel::iterator const& iter)
                     { return getSubtreeForeach(path, iter, subtree_path, indexBuf); });
    return indexBuf;
}




/***********************Double click open folder of file***********************/
void FileList::Impl::onRowActivated(Gtk::TreeModel::Path const& path, Gtk::TreeViewColumn* /*col*/)
{
                // std::cout << "enter onRowActivated ";
                // for (const auto& index : path) {
                //     std::cout << index << " ";
                // }
                // std::cout << std::endl;

    auto const iter = store_->get_iter(path);
    if (!iter)
    {
                // std::cout << "Gtk::TreeModel::Path: notfound !" << std::endl;

        return;
    }
                // else
                // {
                //     std::cout << "Gtk::TreeModel::Path: found it !!" << std::endl;
                // }


    //For Details Dialog, we got torrent handle_ref 
    if (handle_ref_.is_valid())
    {
        //get Torrent obj for get torrent status, so we can get save_path
        Glib::RefPtr<Torrent>const& Tor = core_->find_Torrent(uniq_id_);

        if (auto const filename = get_filename_to_open(Tor, iter); filename)
        {
            gtr_open_file(*filename);
        }
    }
    //For OptionsDialog, we only got atp_ref_ which has it save_path already set in OptionsDialog
    else
    {
                // auto const savepath = atp_ref_.save_path;
                // std::cout << "Open file at path: " << savepath << std::endl;
        
        if(auto const savepath = atp_ref_.save_path; !savepath.empty())
        {
            gtr_open_file(savepath);
        }
    }
}







/********************* Press Priority Column to Switch Priority ***********************/

std::vector<lt::file_index_t> FileList::Impl::getActiveFilesForPath(Gtk::TreeModel::Path const& path) const
{
    if (view_->get_selection()->is_selected(path))
    {
        /* clicked in a selected row... use the current selection */
        return getSelectedFilesAndDescendants();
    }

    /* clicked OUTSIDE of the selected row... just use the clicked row */
    return getSubtree(path);
}


// 'col' and 'path' are assumed not to be nullptr.
bool FileList::Impl::getAndSelectEventPath(double view_x, double view_y, Gtk::TreeViewColumn*& col, Gtk::TreeModel::Path& path)
{
    int cell_x = 0;
    int cell_y = 0;

    if (view_->get_path_at_pos(view_x, view_y, path, col, cell_x, cell_y))
    {
        if (auto const sel = view_->get_selection(); !sel->is_selected(path))
        {
            sel->unselect_all();
            sel->select(path);
        }

        return true;
    }

    return false;
}

bool FileList::Impl::onViewButtonPressed(guint button, TrGdkModifierType state, double view_x, double view_y)
{
    Gtk::TreeViewColumn* col = nullptr;
    Gtk::TreeModel::Path path;
    bool handled = false;

    if (button == GDK_BUTTON_PRIMARY &&
        (state & (TR_GDK_MODIFIED_TYPE(SHIFT_MASK) | TR_GDK_MODIFIED_TYPE(CONTROL_MASK))) == TrGdkModifierType{} &&
        getAndSelectEventPath(view_x, view_y, col, path))
    {
        handled = onViewPathToggled(col, path);
    }

    return handled;
}

/*press priorirty column to switch priority , for dir this may affect its descentdants*/
bool FileList::Impl::onViewPathToggled(Gtk::TreeViewColumn* col, Gtk::TreeModel::Path const& path)
{

    if (col == nullptr || path.empty())
    {
        return false;
    }

    bool handled = false;

    auto const cid = col->get_sort_column_id();

    if (cid == file_cols.priority.index() )
    {
        /*for single-file,it is the file_index_t, for dir, it is the collection of file_index_t of all files in contains*/
        auto const indexBuf = getActiveFilesForPath(path);

        auto const iter = store_->get_iter(path);

        if (cid == file_cols.priority.index())
        {
            auto const old_priority = iter->get_value(file_cols.priority);
            auto new_priority = lt::default_priority;
            /*round-robin*/
            switch (old_priority)
            {
                case lt::default_priority:
                    new_priority = lt::top_priority;
                    break;
                case lt::top_priority:
                    new_priority = lt::low_priority;
                    break;
                case lt::low_priority:
                    new_priority = lt::dont_download;
                    break;
                default:
                    new_priority = lt::default_priority;
                    break;
            }

            //use torrent_handle -- For DetailsDialog
            if(handle_ref_.is_valid())
            {
                for(auto file_idx : indexBuf)
                {
                    handle_ref_.file_priority(file_idx, new_priority);
                }
            }
            //use atp -- For OptionsDialog
            else
            {
                for(auto file_idx : indexBuf)
                {
                    if(static_cast<std::int32_t>(file_idx) == -1)
                    {
                        continue;
                    }
                    atp_ref_.file_priorities[file_idx] = new_priority;
                            // std::printf("%d,", int(file_idx));

                }
                            // std::printf("\n");


                        // std::printf("atp after modify priority in FileList");
                        // for(auto i : atp_ref_.file_priorities)
                        // {
                        //     std::printf("%d,", i);
                        // }
                        // std::printf("\n");

            }
        
        }

        refresh();
        handled = true;
    }

    return handled;
}






/*******************************************RENAME**********************************************/
namespace
{
    /*example:
           -----we modified is a leaf node ,aka. file
                original Canada/Britain/Sunak
                modify `Sunak` to `Cameron`
                after this function: Canada/Britain/Cameron

            -----we modified is a parent node, aka. dir
                original Canada/Britain/Sunak
                modify `Britain` to `India`
                after this function: Canada/India/Sunak

    args:
        is_dir_case : set to true means that we are handling modifying directory's name, so we need to build renamed path for each child file within

        for non-recursive case:
            newname is token, such as "xxx"
        for recursive case:
            newname is not token, it is a path , contaning dir separator, such as "xxx/yyy/zzz"
    */
    Glib::ustring build_renamed_path(const Gtk::TreeModel::iterator& iter, const Glib::ustring& newname, bool is_dir_case/*what we modify is a dir*/)
    {
        // Make a copy of iter to avoid modifying the original
        Gtk::TreeModel::iterator iter_copy = iter;

        Glib::ustring tgtpath;
        //for what we modified is leaf file
        if(!is_dir_case)
        {
            tgtpath.insert(0, newname);
        }
        //for what we modified is dir
        else
        {           
            auto const leafname = iter_copy->get_value(file_cols.label);
            tgtpath.insert(0, leafname);
            tgtpath.insert(0, 1, G_DIR_SEPARATOR);


            //if it is recursive case
            if(newname.find(G_DIR_SEPARATOR))
            {
                tgtpath.insert(0, newname);
                //return directly
                return tgtpath;
            }
            else
            {
                //insert its *nearest* parent dir's new name, and the parent's parent ... etc. are the same with single file case
                tgtpath.insert(0, newname);

                //dont forget to update iter_copy
                iter_copy = iter_copy->parent();
                if(!iter_copy)
                {
                    return tgtpath;
                }
            }
        }


        for (;;)
        {
            tgtpath.insert(0, 1, G_DIR_SEPARATOR);
            auto parent_iter = iter_copy->parent();
            if (!parent_iter)
            {  
                break;
            }
         
            tgtpath.insert(0, parent_iter->get_value(file_cols.label));
            iter_copy = parent_iter;
        }
        
        tgtpath.erase(0, 1); // Remove leading separator
        return tgtpath;
    }


    void gather_files(const Gtk::TreeModel::iterator& dir_iter,
                                const Glib::ustring& newname,
                                std::vector<std::pair<std::int32_t, Glib::ustring>>& files_to_rename)
    {
        // Ensure the iterator is valid
        if (!dir_iter)
        {
            std::cout << "Invalid directory iterator" << std::endl;
            return;
        }


                // Gtk::TreeModel::iterator child_iter_2 = dir_iter->children().begin();
                // for (auto row_iter = child_iter_2; row_iter != dir_iter->children().end(); ++row_iter) 
                // {
                
                //     auto row = *row_iter;
                //     std::cout << "Index: " << row.get_value(file_cols.index) << ", Label: " << row.get_value(file_cols.label)  << std::endl;
                // }
                // return;


        Gtk::TreeModel::iterator child_iter = dir_iter->children().begin();
        // Gather child files or dirs within the current directory
        for (auto row_iter = child_iter; row_iter != dir_iter->children().end(); ++row_iter)
        {
                    // Ensure the child iterator is valid
                    // if (!row_iter || row_iter == children_iter.end())
                    // {
                    //     std::cerr << "Invalid or end child iterator" << std::endl;
                    //     return;
                    // }

                    // auto const str = row_iter->get_value(file_cols.label);
                    // std::cout << "str: " << str << std::endl;
                    
            if (row_iter->children().empty())
            {
                // It's a file
                std::int32_t file_idx = row_iter->get_value(file_cols.index);
                Glib::ustring new_file_path = build_renamed_path(row_iter, newname, true);
                files_to_rename.emplace_back(file_idx, new_file_path);
            }
            else
            {
                // It's a directory, recurse into it
                Glib::ustring new_dir_path = build_renamed_path(row_iter, newname, true);
                gather_files(row_iter, new_dir_path, files_to_rename);
            }
        }
    }


}//anonymous namespace



struct rename_data
{
    Glib::ustring newname;
    Glib::ustring path_string;
    gpointer impl = nullptr;
};

void FileList::Impl::on_rename_done(Glib::ustring const& path_string, Glib::ustring const& newname, std::int32_t failedCount)
{
    rename_done_tags_.push(Glib::signal_idle().connect(
        [this, path_string, newname, failedCount]()
        {
            rename_done_tags_.pop();
            on_rename_done_idle(path_string, newname, failedCount);
            return false;
        })
    );
}

void FileList::Impl::on_rename_done_idle(Glib::ustring const& path_string, Glib::ustring const& newname, std::int32_t failedCount)
{
    /*
        when rename a dir cell, its child file's name remain intact,they just moved to new dir, so no need to update diplay
        just update the dir cell's diplay text in GUI
    */
    if (failedCount == 0)
    {
        if (auto const iter = store_->get_iter(path_string); iter)
        {
            bool const isLeaf = iter->children().empty();
            auto const mime_type = isLeaf ? tr_get_mime_type_for_filename(newname.raw()) : DirectoryMimeType;
            auto const icon = gtr_get_mime_type_icon(mime_type);
            //update cell display
            (*iter)[file_cols.label] = newname;
            (*iter)[file_cols.icon] = icon;

            //TEMPORARY DISABLED  NOT SURE ITS FUNCITONALITY
            // if (!iter->parent())
            // {
            //     core_->torrent_changed(uniq_id_);
            // }
        }
    }
    //some rename operations failed, such as when your new name contains '/', it is invalid filename,we are assumsed to be failed to edit this cell 
    else
    {
        auto w = std::make_shared<Gtk::MessageDialog>(
            gtr_widget_get_window(widget_),
            fmt::format(
                _("Failed rename '{old_path}' as '{path}' "),
                fmt::arg("old_path", path_string),
                fmt::arg("path", newname)),
            false,
            TR_GTK_MESSAGE_TYPE(ERROR),
            TR_GTK_BUTTONS_TYPE(CLOSE),
            true);
        w->set_secondary_text(_("Please correct the errors and try again."));
        w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
        w->show();
    }
}

void FileList::Impl::cell_edited_callback(Glib::ustring const& path_string, Glib::ustring const& newname)
{

            // std::cout << "path_string is " << path_string << std::endl;
            // std::cout << "newname is " << newname << std::endl;
             
    if (!handle_ref_.is_valid() && !atp_ref_.info_hashes.has_v1())
    {
        return;
    }

    auto iter = store_->get_iter(path_string);
    if (!iter)
    {
        return;
    }


        // auto row = *iter;
        // std::cout << "Index: " << row->get_value(file_cols.index) << ", Label: " << row->get_value(file_cols.label)  << std::endl;
        // std::cout << "```````" << std::endl;
        // traverse_children(*iter);
        // std::cout << "```````" << std::endl;

        // Gtk::TreeModel::iterator child_iter = iter->children().begin();
        // for (auto row_iter = child_iter; row_iter != iter->children().end(); ++row_iter) 
        // {           
        //     auto row = *row_iter;
        //     std::cout << "Index: " << row.get_value(file_cols.index) << ", Label: " << row.get_value(file_cols.label)  << std::endl;
        // }

        
    /* if this cell row is a dir, rename a dir will lead to rename on all its child contained, recursively */
    auto const is_dir = (!iter->children().empty());
    std::int32_t tgt_file_idx = -1;
    //dir has no file_index_t
    if(!is_dir){
        tgt_file_idx = iter->get_value(file_cols.index);
    }

    std::vector<std::pair<std::int32_t, Glib::ustring>> files_to_rename;
    if(is_dir) 
    {
        // Directory case: Recursively gather all files and directories
        gather_files(iter, newname, files_to_rename);
    } 
    else 
    {
        // Single file case
        Glib::ustring tgtpath = build_renamed_path(iter, newname, false);
        files_to_rename.emplace_back(tgt_file_idx, tgtpath);
    }

            // for (const auto& [index, tgtpath] : files_to_rename) 
            // {
            //     std::cout << "index: " << index << ", tgtpath: " << tgtpath  << std::endl;
            // }


    /*
    prototype:

        void torrent_handle::rename_file(file_index_t index, std::string name);

    args: 

        'index'---- specify which file to be renamed

        'name' ---- the relative path based on the *root dir* 

    explain:

        *  if save path of the torrent is '/home/pal/Downloads/MyFile/Dir1/content.txt'
            (here, the root dir of the torrent is `MyFile`)
            the 'name' arg is 'MyFile/Dir2/Content.txt'
            Dir2 in `MyFile` root-dir will be created and the file will be moved to this dirs

        * if  save path of the torrent is '/home/pal/Downloads/MyFile/content.txt'
            (here, the root dir of the torrent is `MyFile`)
            the 'name' arg is '/MyFile/Content.txt'
            parent dir is unchanged, just the file will be renamed

        * also, there also might be some torrents which only has one single file, no root dir,
                so the case is:
            the save path of the single-file torrent is '/home/pal/Downloads/singlefile.txt'
            (here, no root dir for this torrent, we just the file directly)
            when the 'name' arg is 'Singlefile.txt'
            the file will be renamed  

    */
    // Rename all collected files
    for (const auto& [index, tgtpath] : files_to_rename) 
    {
        if(index == -1)
        {
            continue;
        }

        //used for DetailsDialog
        if (handle_ref_.is_valid()) 
        {
            handle_ref_.rename_file(static_cast<lt::file_index_t>(index), tgtpath.raw());
        } 
        //used for OptionsDialog
        else 
        {
                    // std::cout << "pass arg to renamed_files" << tgtpath << std::endl;
            atp_ref_.renamed_files[static_cast<lt::file_index_t>(index)] = tgtpath.raw();

        }
    }


                    //for(const auto& [key, value] : atp_ref_.renamed_files){
                    //    std::cout << key << ": " << value << std::endl;
                    //}


    bool use_atp = (!handle_ref_.is_valid() && atp_ref_.info_hashes.has_v1());
    bool use_handle = (handle_ref_.is_valid() && !atp_ref_.info_hashes.has_v1());
    if(use_atp)
    {
        //atp no need to wait for alert, assumed to success
        on_rename_done(path_string, newname, 0);
        return;
    }
    else if(use_handle)
    {
        // Track the number of pending operations
        int pending_operations = files_to_rename.size();
        int failed_operations = {};
        if (pending_operations == 0) 
        {
            // No files to rename, do nothing
            return;
        }

        auto rename_data = std::make_unique<struct rename_data>();
        rename_data->newname = newname;
        //may Open multiple OptionsDialog or DetailsDialog of each torrent, 
        // so there are multiple FileList objects, need *this* pointer to differentiate which is which
        rename_data->impl = this;
        rename_data->path_string = path_string;

        /*callback for feedback, to decide whether rename has filed or not,
            if success, update display
            if failed, we popup dialog

            for dir, multiple rename task should be performed,
            when each task alert, we only care whether its error_code is set, meaning it failed to rename
            this means the dir's rename operation failed  
            only when all its child file or dir renamed success, can the dir regarded as rename success
        */
        core_->FetchTorRenameResult(
            static_cast<rename_done_func>(
                [pending_operations, failed_operations](gpointer data, lt::error_code ec) mutable
                {
                    pending_operations--;
                    if (ec)
                    {
                        failed_operations++;
                    }
                    if (pending_operations == 0) {
                        // All operations are complete
                        auto const data_grave = std::unique_ptr<struct rename_data>(static_cast<struct rename_data*>(data));
                        static_cast<Impl*>(data_grave->impl)->on_rename_done(data_grave->path_string, data_grave->newname, failed_operations);
                    }
                }),
            rename_data.release()
        );
    }


}








/******************
 ******CTOR********
 ******************/
//Ctor For DetailsDialog
FileList::FileList(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::ustring const& view_name,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle const& handle_ref
    )
    : Gtk::ScrolledWindow(cast_item)  //Calls the base class constructor
    , impl_(std::make_unique<Impl>(*this, builder, view_name, core, handle_ref, empty_atp_))
{
}


//Ctor For OptionsDialog
FileList::FileList(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::ustring const& view_name,
    Glib::RefPtr<Session> const& core,
    lt::add_torrent_params & atp_ref
    )
    : Gtk::ScrolledWindow(cast_item)  //Calls the base class constructor
    , impl_(std::make_unique<Impl>(*this, builder, view_name, core, empty_handle_, atp_ref))
{
}


FileList::Impl::Impl(
    FileList& widget,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::ustring const& view_name,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle const& handle_ref,
    lt::add_torrent_params & atp_ref
    )
    : widget_(widget)
    , core_(core)
    , view_(gtr_get_widget<Gtk::TreeView>(builder, view_name))
    , handle_ref_(handle_ref)
    , atp_ref_(atp_ref)
{
 

    uniq_id_ = handle_ref_.id();


    /* create the view */
    view_->signal_row_activated().connect(sigc::mem_fun(*this, &Impl::onRowActivated));


    setup_item_view_button_event_handling(
        *view_,
        [this](guint button, TrGdkModifierType state, double view_x, double view_y, bool /*context_menu_requested*/)
        { return onViewButtonPressed(button, state, view_x, view_y); },
        [this](double view_x, double view_y) 
        { return on_item_view_button_released(*view_, view_x, view_y); });


    auto pango_font_description = view_->create_pango_context()->get_font_description();
    if (auto const new_size = pango_font_description.get_size() * 0.8; pango_font_description.get_size_is_absolute())
    {
        pango_font_description.set_absolute_size(new_size);
    }
    else
    {
        pango_font_description.set_size(new_size);
    }

    /* set up view */
    auto const sel = view_->get_selection();
    sel->set_mode(TR_GTK_SELECTION_MODE(MULTIPLE));
    view_->expand_all();
    view_->set_search_column(file_cols.label);

    {
        /* add file column */
        auto* col = Gtk::make_managed<Gtk::TreeViewColumn>();
        col->set_expand(true);
        col->set_title(_("Name"));
        col->set_resizable(true);
        auto* icon_rend = Gtk::make_managed<Gtk::CellRendererPixbuf>();
        col->pack_start(*icon_rend, false);
        col->add_attribute(icon_rend->property_gicon(), file_cols.icon);
#if GTKMM_CHECK_VERSION(4, 0, 0)
        icon_rend->property_icon_size() = Gtk::IconSize::NORMAL;
#else
        icon_rend->property_stock_size() = Gtk::ICON_SIZE_MENU;
#endif

        /* add text renderer */
        auto* text_rend = Gtk::make_managed<Gtk::CellRendererText>();
        text_rend->property_editable() = true;
        text_rend->property_ellipsize() = TR_PANGO_ELLIPSIZE_MODE(END);
        text_rend->property_font_desc() = pango_font_description;
        //register cell edit callback
        text_rend->signal_edited().connect(sigc::mem_fun(*this, &Impl::cell_edited_callback));
        col->pack_start(*text_rend, true);
        col->add_attribute(text_rend->property_text(), file_cols.label);
        col->set_sort_column(file_cols.label);
        view_->append_column(*col);
    }



    {
        /* add "size" column */
        auto* rend = Gtk::make_managed<Gtk::CellRendererText>();
        rend->property_alignment() = TR_PANGO_ALIGNMENT(RIGHT);
        rend->property_font_desc() = pango_font_description;
        rend->property_xpad() = GUI_PAD;
        rend->property_xalign() = 1.0F;
        rend->property_yalign() = 0.5F;
        auto* col = Gtk::make_managed<Gtk::TreeViewColumn>(_("Size"), *rend);
        col->set_sizing(TR_GTK_TREE_VIEW_COLUMN_SIZING(GROW_ONLY));
        col->set_sort_column(file_cols.size);
        col->add_attribute(rend->property_text(), file_cols.size_str);
        view_->append_column(*col);
    }




    {
        /* add "progress" column */
        auto const* title = _("Have");
        int width = 0;
        int height = 0;
        view_->create_pango_layout(title)->get_pixel_size(width, height);
        width += 30; /* room for the sort indicator */
        auto* rend = Gtk::make_managed<Gtk::CellRendererProgress>();
        auto* col = Gtk::make_managed<Gtk::TreeViewColumn>(title, *rend);
        col->add_attribute(rend->property_text(), file_cols.prog_str);
        col->add_attribute(rend->property_value(), file_cols.prog);
        col->set_fixed_width(width);
        col->set_sizing(TR_GTK_TREE_VIEW_COLUMN_SIZING(FIXED));
        col->set_sort_column(file_cols.prog);
        view_->append_column(*col);
    }




    {
        /* add priority column */
        auto const* title = _("Priority");
        int width = 0;
        int height = 0;
        view_->create_pango_layout(title)->get_pixel_size(width, height);
        width += 30; /* room for the sort indicator */
        auto* rend = Gtk::make_managed<Gtk::CellRendererText>();
        rend->property_xalign() = 0.5F;
        rend->property_yalign() = 0.5F;
        auto* col = Gtk::make_managed<Gtk::TreeViewColumn>(title, *rend);
        col->set_fixed_width(width);
        col->set_sizing(TR_GTK_TREE_VIEW_COLUMN_SIZING(FIXED));
        col->set_sort_column(file_cols.priority);
        col->set_cell_data_func(*rend, sigc::ptr_fun(&renderPriority));
        view_->append_column(*col);
    }



    /* add tooltip to tree */
    view_->set_tooltip_column(file_cols.label_esc.index());


}



FileList::~FileList() /*= default;*/
{
    std::cout << "FileList::~FileList() " << std::endl;
}