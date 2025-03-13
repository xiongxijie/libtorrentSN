
set(TR_NAME "transmission")
add_executable(${TR_NAME}-gtk)



target_sources(${TR_NAME}-gtk
    PRIVATE
    main.cc
    Application.cc
    Application.h
    Actions.cc
    Actions.h
    DetailsDialog.cc
    DetailsDialog.h
    Dialogs.cc
    Dialogs.h
    DynamicPropertyStore.h
    FaviconCache.cc
    FileList.cc
    FileList.h
    FilterBar.cc
    FilterBar.h
    FilterBase.h
    FilterBase.hh
    FilterListModel.h
    FilterListModel.hh
    Flags.h
    FreeSpaceLabel.cc
    FreeSpaceLabel.h
    GtkCompat.h
    HigWorkarea.h
    IconCache.cc
    IconCache.h
    ListModelAdapter.cc
    ListModelAdapter.h
    MainWindow.cc
    MainWindow.h
    MakeDialog.cc
    MakeDialog.h
    MessageLogWindow.cc
    MessageLogWindow.h
    OptionsDialog.cc
    OptionsDialog.h
    PathButton.cc
    PathButton.h
    Percents.cc
    Percents.h
    Prefs.cc
    Prefs.h
    PrefsDialog.cc
    PrefsDialog.h
    RelocateDialog.cc
    RelocateDialog.h
    Session.cc
    Session.h
    SorterBase.h
    SorterBase.hh
    SortListModel.h
    SortListModel.hh
    StatsDialog.cc
    StatsDialog.h
    Torrent.cc
    Torrent.h
    TorrentCellRenderer.cc
    TorrentCellRenderer.h
    TorrentFilter.cc
    TorrentFilter.h
    TorrentSorter.cc
    TorrentSorter.h
    Utils.cc
    Utils.h
    tr-error.cc
    tr-error.h
    tr-error-types.h
    tr-favicon-cache.h
    tr-file.h
    tr-file.cc
    tr-file-posix.cc
    tr-file-capacity.cc
    tr-log.h
    tr-log.cc
    tr-macros.h
    tr-mime-types.h
    tr-platform.h
    tr-platform.cc
    tr-strbuf.h
    tr-values.h
    tr-transmission.h
    tr-utils.h
    tr-utils.cc
    tr-utils-ev.h
    tr-utils-ev.cc
    tr-version.h
    tr-web-utils.h
    tr-web-utils.cc
    tr-web.h
    tr-web.cc
    tr-assert.h
    tr-assert.cc
    tr-net.h
    tr-net.cc
        

)

target_sources(${TR_NAME}-gtk
    PRIVATE
    ui/gtk3/AddTrackerDialog.ui
    ui/gtk3/DetailsDialog.ui
    ui/gtk3/EditTrackersDialog.ui
    ui/gtk3/FilterBar.ui
    ui/gtk3/MainWindow.ui
    ui/gtk3/MakeDialog.ui
    ui/gtk3/MakeProgressDialog.ui
    ui/gtk3/MessageLogWindow.ui
    ui/gtk3/OptionsDialog.ui
    ui/gtk3/PrefsDialog.ui
    ui/gtk3/RelocateDialog.ui
    ui/gtk3/StatsDialog.ui
    ui/gtk3/TorrentUrlChooserDialog.ui
)





source_group(Ui/GTK3
    REGULAR_EXPRESSION [[ui/gtk3/.*\.ui$]])


set(UI_SUBDIR ui/gtk3)

tr_target_glib_resources(${TR_NAME}-gtk
    transmission.gresource.xml
    ${UI_SUBDIR}/transmission-ui.gresource.xml)


#  - Native Language Support ##############################################
#  set(DESKTOP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TR_NAME}-gtk.desktop")
#  tr_gettext_msgfmt(${TR_NAME}-gtk
#      ${DESKTOP_FILE}
#      ${CMAKE_CURRENT_SOURCE_DIR}/transmission-gtk.desktop.in)
#  target_sources(${TR_NAME}-gtk
#      PRIVATE
#          ${DESKTOP_FILE})

#  set(METAINFO_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TR_NAME}-gtk.metainfo.xml")
#  tr_gettext_msgfmt(${TR_NAME}-gtk
#      ${METAINFO_FILE}
#      ${CMAKE_CURRENT_SOURCE_DIR}/transmission-gtk.metainfo.xml.in)
#  target_sources(${TR_NAME}-gtk
#      PRIVATE
#          ${METAINFO_FILE})
############################################################################
    

target_compile_definitions(${TR_NAME}-gtk
    PRIVATE
        TRANSMISSIONLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
        # GETTEXT_PACKAGE="${TR_NAME}-gtk"
        # WITH_UTP
)


# target_compile_options(${TR_NAME}-gtk
#     PRIVATE
#         $<$<BOOL:${ENABLE_WERROR}>:$<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>>
#         $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-exit-time-destructors>)


# include(CheckAtomic)
include(CheckLibraryExists)
# include(CheckSymbolExists)

# if(ENABLE_NLS)
    check_library_exists(intl libintl_gettext "" HAVE_LIBINTL)
    if(HAVE_LIBINTL)
        set(LIBINTL_LIBRARY intl)
    endif()
# endif()

tr_target_compile_definitions_for_headers(${TR_NAME}-gtk
    PRIVATE
        sys/statvfs.h
        xfs/xfs.h
        xlocale.h)

tr_target_compile_definitions_for_functions(${TR_NAME}-gtk
    PRIVATE
        copyfile
        copy_file_range
        fallocate64
        flock
        getmntent
        htonll
        mkdtemp
        ntohll
        posix_fadvise
        posix_fallocate
        pread
        pwrite
        sendfile64
        statvfs
    PUBLIC
        gettext
        ngettext
    REQUIRED_LIBS
        $<$<BOOL:${HAVE_LIBINTL}>:${LIBINTL_LIBRARY}>
) # gettext, ngettext



target_include_directories(${TR_NAME}-gtk
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR})


    target_link_libraries(${TR_NAME}-gtk
    PRIVATE
        gtk_impl
        fmt::fmt-header-only # commonly used in gtk formatting
        small::small #used in tr-log
        FastFloat::fast_float # used in tr-utils
        wildmat # used in tr-utils
        psl::psl # used in tr-web-utils
        libevent::event # used in tr-web
        CURL::libcurl # used in tr-web
        utf8::cpp
        ${TR_NETWORK_LIBRARIES}
    PUBLIC
        torrent-rasterbar
    )


install(
    TARGETS ${TR_NAME}-gtk
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)


set(PUBLIC_ICONS
    hicolor_apps_scalable_transmission.svg
    hicolor_apps_scalable_transmission-devel.svg
    hicolor_apps_symbolic_transmission-symbolic.svg
)

set(ICON_NAME_REGEX "^([^_]+)_([^_]+)_([^_]+)_(.+)$")
foreach(ICON ${PUBLIC_ICONS})
string(REGEX REPLACE ${ICON_NAME_REGEX} "\\1/\\3/\\2" ICON_DIR ${ICON})
string(REGEX REPLACE ${ICON_NAME_REGEX} "\\4" ICON_NAME ${ICON})
install(
    FILES ${SOURCE_ICONS_DIR}/${ICON}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/${ICON_DIR}/
    RENAME ${ICON_NAME})
endforeach()

# Native Language Support ###################################
#if(ENABLE_NLS)
    install(
        FILES ${DESKTOP_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(
        FILES ${METAINFO_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
#else()
#    install(
#        FILES transmission-gtk.desktop.in
#        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
#        RENAME ${TR_NAME}-gtk.desktop)
#endif()
