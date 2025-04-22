
set(TR_NAME "transmission")



#totem
# add_subdirectory(totem)

add_subdirectory(totem-plugins)





# Create the executable target
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
    TotemUri.h
    TotemUri.cc
    TotemGstHelper.h
    TotemGstHelper.c
    MyHdyFlap.h
    MyHdyFlap.cc
    TotemPlaylist.h
    TotemPlayerHeader.h
    TotemPlayerHeader.cc
    TotemPrefsPluginRow.h
    TotemPrefsPluginRow.cc
    TotemPrefsDialog.h
    TotemPrefsDialog.cc
    BitfieldScale.h
    BitfieldScale.cc
    BaconTimeLabel.h
    BaconTimeLabel.cc
    BaconVideoWidgetEnums.h
    BaconVideoWidget.h
    BaconVideoWidget.cc
    TotemWindow.h
    TotemWindow.cc
)


# code in libraries should not depend on code in executable 
# a include header of b, but b is part of executable, cannot link against an executable 

# If the code in lib include some headers in executable, but we cannot let lib link against executable ?
# Use target_include_directories() to expose the executable’s headers to the library:
#     Even though the shared library cannot link to the executable, 
#     you can still allow the shared library to include headers from 
#     the executable’s source directory by setting up the proper include directories.


# When you include a header in a source file (.c or .cpp), the compiler only needs the declarations to compile that file. 
# The linker, however, is responsible for resolving those declarations to actual implementations (in the object files or libraries). 
# If the implementation is not available during the linking phase, the build will fail.




# Splitting and modularize those common code in to library, Mainly for plugins can use(link against) 
add_library(totemplugins SHARED 
    TotemPluginFoo.h
    TotemWindowInterface.h
    TotemWrapper.h 
    TotemWrapper.cc
    totem-plugins/totem-plugins-engine.h
    totem-plugins/totem-plugins-engine.cc
    totem-plugins/totem-plugin.h
    totem-plugins/totem-dirs.h
    totem-plugins/totem-dirs.cc
)




#btdemux plugin for Gstreamer also need cooperate with our transmission Session with ibtorrent underlying core
# libgstbt.so is in our project dir, need to tell GStreamer to lookup it in there
add_library(gstbt SHARED 
    # gstbt/gst_bt_type.c  #may be no used
    # gstbt/gst_bt_type.h  #may be no used
    gstbt/gst_bt.cpp
    gstbt/gst_bt.h
    gstbt/gst_bt_demux.cpp
    gstbt/gst_bt_demux.hpp
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

# not used for the build process itself but for organizing files when viewed inside an IDE that supports CMake
# if you are using the Visual Studio generator, CMake will generate a .sln (solution) file that contains virtual
# folders corresponding to the groups defined by source_group.
source_group(Ui/GTK3
    REGULAR_EXPRESSION [[ui/gtk3/.*\.ui$]])


set(UI_SUBDIR ui/gtk3)


# [glib-compile-resources] compile the `xxx.gresource.xml` XML to a binary bundle(.gresource) that you can load with g_resource_load()
# but it is more common (our choice) to create a source file and header to link directly into your application
# underlying use [glib-compile-resources] on gresource.xml file, to compiled resource file (UI XML, images, CSS,etc..) into the application
# resources are loaded from memory instead of the filesystem, access is faster and more efficient.
tr_target_glib_resources(${TR_NAME}-gtk
    transmission.gresource.xml
    ${UI_SUBDIR}/transmission-ui.gresource.xml) #used to init transmission Actions (popup menu ..etc..)



    
#totem ui , must manually specify the path or it cant find 
target_sources(${TR_NAME}-gtk
PRIVATE
    totem/data/totem.ui
    totem/data/totem-player-toolbar.ui
    totem/data/totem-preferences-dialog.ui
    totem/data/totem-preferences-plugin-row.ui
    totem/data/playlist.ui
    totem-plugins/properties/properties.ui # used for the [properties window] plugin, which has its own ui file
    totem/src/backend/bacon-video-widget.ui
)

source_group(Totem/data
    REGULAR_EXPRESSION [[totem/data/.*\.ui$]])

set(TOTEM_UI_SUBDIR totem/data)
set(TOTEM_PROP_PLUGIN_UI totem-plugins/properties)

tr_target_glib_resources(${TR_NAME}-gtk
    ${TOTEM_UI_SUBDIR}/totem.gresource.xml #general totem ui file 
    ${TOTEM_PROP_PLUGIN_UI}/properties.gresource.xml #only for totem [properties window] plugin
)
    

#manually instead of using tr_target_glib_resources since it will generate hash file name on .c and .h ,hard to include
find_program(OUR_GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)
if(NOT OUR_GLIB_COMPILE_RESOURCES)
    message(FATAL_ERROR "Could not find glib-compile-resources. Please install glib-compile-resources or make sure it's in your PATH.")
else()
    message(STATUS "\nFound glib-compile-resources at ${OUR_GLIB_COMPILE_RESOURCES}\n")
endif()

execute_process(
    COMMAND ${OUR_GLIB_COMPILE_RESOURCES}
            --target "${CMAKE_CURRENT_BINARY_DIR}/bacon-video-widget-resources.h"
            --sourcedir "${CMAKE_SOURCE_DIR}/gtk/totem/src/backend/" 
            --generate-header
            --c-name "_bvw"
            "${CMAKE_SOURCE_DIR}/gtk/totem/src/backend/bacon-video-widget.gresource.xml"
            
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE RESULT_BACON_RESOURCE_H
    ERROR_VARIABLE ERROR_BACON_RESOURCE_H
    OUTPUT_VARIABLE OUTPUT_BACON_RESOURCE_H
)
# Check if the process was successful
if(NOT ${RESULT_BACON_RESOURCE_H} EQUAL 0)
    message(FATAL_ERROR "Error generating bacon-video-widget.gresource.h file:\n${ERROR_BACON_RESOURCE_H}")
else()
    message(STATUS "Successfully generated bacon-video-widget.gresource.h")
endif()

execute_process(
    COMMAND ${OUR_GLIB_COMPILE_RESOURCES}
            --target "${CMAKE_CURRENT_BINARY_DIR}/bacon-video-widget-resources.c"
            --sourcedir "${CMAKE_SOURCE_DIR}/gtk/totem/src/backend/" 
            --generate-source
            --c-name "_bvw"
            "${CMAKE_SOURCE_DIR}/gtk/totem/src/backend/bacon-video-widget.gresource.xml"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE RESULT_BACON_RESOURCE_C
    ERROR_VARIABLE ERROR_BACON_RESOURCE_C
    OUTPUT_VARIABLE OUTPUT_BACON_RESOURCE_C
)
# Check if the process was successful
if(NOT ${RESULT_BACON_RESOURCE_C} EQUAL 0)
    message(FATAL_ERROR "Error generating bacon-video-widget.gresource.c file:\n${ERROR_BACON_RESOURCE_C}")
else()
    message(STATUS "Successfully generated bacon-video-widget.gresource.c")
endif()


target_sources(${TR_NAME}-gtk PRIVATE 
                        ${CMAKE_CURRENT_BINARY_DIR}/bacon-video-widget-resources.c
                        ${CMAKE_CURRENT_BINARY_DIR}/bacon-video-widget-resources.h)





#########################Totem's GSettings schemas or mkEnums###############################
# For use GSettings in totem, we should first copy it from data dir to 
# build path then call glib-compile-gschemas to compile the given .gschema.xml file
# Create the directory for the schemas in the build directory.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gtk/gschemas)


#generate Totem's enums using (glib-mkenums)
# Find glib-mkenums
find_program(GLIB_MKENUMS NAMES glib-mkenums REQUIRED)
    
if(NOT GLIB_MKENUMS)
    message(FATAL_ERROR "Could not find glib-mkenums. Please install glib-mkenums or make sure it's in your PATH.")
else()
    message(STATUS "\nFound glib-mkenums at ${GLIB_MKENUMS}\n")
endif()

# List of header files containing the enums
set(totem_enum_headers
    ${CMAKE_SOURCE_DIR}/gtk/totem/src/backend/bacon-video-widget.h
)




# Output paths
set(TOTEM_ENUMS_OUTPUT ${CMAKE_BINARY_DIR}/gtk/gschemas/org.gnome.trtotem.enums.xml)
set(TOTEM_ENUMS_HEADER_OUTPUT ${CMAKE_BINARY_DIR}/gtk/gschemas/bacon-video-widget-enums.h)
set(TOTEM_ENUMS_SOURCE_OUTPUT ${CMAKE_BINARY_DIR}/gtk/gschemas/bacon-video-widget-enums.c)




# it seems useless to generate enum.xml , but it actually is used for gschemas.xml, (.xml reads .xml) 
execute_process(
    COMMAND ${GLIB_MKENUMS}
            --comments "<!-- @comment@ -->"
            --fhead "<schemalist>"
            --vhead "  <@type@ id=\"org.gnome.trtotem.@EnumName@\">"
            --vprod "    <value nick=\"@valuenick@\" value=\"@valuenum@\"/>"
            --vtail "  </@type@>"
            --ftail "</schemalist>"
            --output ${TOTEM_ENUMS_OUTPUT}
            ${totem_enum_headers}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE RESULT_ENUMHEADER
    ERROR_VARIABLE ERROR_OUTPUT_HEADER
    OUTPUT_VARIABLE OUTPUT_HEADER
)
# Check if the process was successful
if(NOT ${MKENUM_RESULT} EQUAL 0)
    message(FATAL_ERROR "Error generating enums.xml file:\n${ERROR_OUTPUT}")
else()
    message(STATUS "Successfully generated enums.xml at ${TOTEM_ENUMS_OUTPUT}")
endif()



#header and source file (.h ,.c)
execute_process(
    COMMAND ${GLIB_MKENUMS}
            --fhead "\n\n#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n\n"
            --eprod "\n\nGType @enum_name@_get_type (void);\n#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n\n"
            --ftail "G_END_DECLS\n\n\n"
            --output ${TOTEM_ENUMS_HEADER_OUTPUT}
            ${totem_enum_headers}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE RESULT_ENUMHEADER
    ERROR_VARIABLE ERROR_OUTPUT_HEADER
    OUTPUT_VARIABLE OUTPUT_HEADER
)
# Check if the process was successful
if(NOT ${RESULT_ENUMHEADER} EQUAL 0)
    message(FATAL_ERROR "Error generating enums header file:\n${ERROR_OUTPUT_HEADER}")
else()
    message(STATUS "Successfully generated enums header at ${TOTEM_ENUMS_HEADER_OUTPUT}")
endif()

# generate the source file
execute_process(
    COMMAND ${GLIB_MKENUMS}
        --fhead "#include \"bacon-video-widget-enums.h\"\n\n#include \"BaconVideoWidgetEnums.h\"\n\n#define C_ENUM(v) ((gint) v)\n#define C_FLAGS(v) ((guint) v)\n\n"
        --eprod "\n\nGType\n@enum_name@_get_type (void)\n{\nstatic gsize gtype_id = 0;\nstatic const GEnumValue values[] = {\n"
        --vprod "\n{ C_ENUM(@VALUENAME@), \"@VALUENAME@\", \"@valuenick@\" },"
        --vtail "\n{ 0, NULL, NULL }\n};\nif (g_once_init_enter (&gtype_id)) {\n GType new_type = g_enum_register_static (g_intern_static_string (\"@EnumName@\"), values);\ng_once_init_leave (&gtype_id, new_type);\n}\nreturn (GType) gtype_id;\n}\n\n"
        --output ${TOTEM_ENUMS_SOURCE_OUTPUT}
            ${totem_enum_headers}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE RESULT_ENUMSOURCE
    ERROR_VARIABLE ERROR_OUTPUT_SOURCE
    OUTPUT_VARIABLE OUTPUT_SOURCE
)
# Check if the process was successful
if(NOT ${RESULT_ENUMSOURCE} EQUAL 0)
    message(FATAL_ERROR "Error generating enums source file:\n${ERROR_OUTPUT_SOURCE}")
else()
    message(STATUS "Successfully generated enums source at ${TOTEM_ENUMS_SOURCE_OUTPUT}")
endif()



target_sources(${TR_NAME}-gtk PRIVATE 
                        ${CMAKE_CURRENT_BINARY_DIR}/gschemas/bacon-video-widget-enums.c
                        ${CMAKE_CURRENT_BINARY_DIR}/gschemas/bacon-video-widget-enums.h)



#Only need to install the .enums.xml, not the header, the header need in build dir

# Optionally, in the future: Add installation step for the generated XML file
# This will only be activated when needed by enabling installation in CMake.
# install(FILES ${TOTEM_ENUMS_XML}
        # DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas
        # OPTIONAL)  # Optional for now, you can remove this in the future

# Optionally, display the generated XML file path
# message(STATUS "Enums generated at: ${TOTEM_ENUMS_XML}")






# Copy the schema file (without any template processing) into the build directory,
# renaming it to remove the '.in' suffix.
configure_file(
  ${CMAKE_SOURCE_DIR}/gtk/totem/data/org.gnome.trtotem.gschema.xml.in
  ${CMAKE_BINARY_DIR}/gtk/gschemas/org.gnome.trtotem.gschema.xml
  COPYONLY
)


# Locate the glib-compile-schemas executable.
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)

if(GLIB_COMPILE_SCHEMAS)
# runs immediately during the CMake configuration phase, 
# !! if totem cannot find gsettings, call export GSETTINGS_SCHEMA_DIR=/media/pal/E/Downloads/libtorrent-SN/build/gtk/gschemas
# OR it will report "GLib-GIO-ERROR **: 02:18:14.229: Settings schema 'org.gnome.trtotem' is not installed"

  execute_process(
    COMMAND ${GLIB_COMPILE_SCHEMAS} ${CMAKE_BINARY_DIR}/gtk/gschemas #--targetdir=/usr/share/glib-2.0/schemas
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  # Check if the command was successful
  if(result EQUAL 0)
    message(STATUS "GSettings schema used by totem compilation successful.")
  else()
    message(FATAL_ERROR "GSettings schema compilation failed with error code ${result}.")
  endif()
else()
  message(FATAL_ERROR "glib-compile-schemas not found.")
endif()


# message(STATUS "In gtk dir CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")
# -- In gtk dir CMAKE_BINARY_DIR = /media/pal/E/Downloads/libtorrent-SN/build


# message(STATUS "In gtk dir CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
# -- In gtk dir CMAKE_SOURCE_DIR = /media/pal/E/Downloads/libtorrent-SN/







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




# define macro "TOTEM_PLUGIN_DIR" in totemplugin lib, those who link against it will have access to this macro
# set(plugins_cflags "-DTOTEM_PLUGIN_DIR=\"/media/pal/E/Downloads/libtorrent-SN/build/gtk/totem-plugins\"")
# target_compile_definitions(${plugin_name} PUBLIC ${plugins_cflags})

target_compile_definitions(gstbt PRIVATE 
    HAVE_GST_1
    PACKAGE="gst-bt"
)




# Expose the include directories for totemplugins to other targets
# any target that links against totemplugins will automatically gain the include directories exposed by totemplugins
target_include_directories(totemplugins
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/totem-plugins
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/totem-plugins

)   

#mark as PUBLIC let Totem can include the header btdemux.h
target_include_directories(gstbt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/gstbt
)   



# message(STATUS "In gtk dir CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(${TR_NAME}-gtk
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR} # maybe useful such as add include path for include the bacon-video-widget-enums.h (who generated in build dir, not availabel in source code file)
        ${CMAKE_CURRENT_BINARY_DIR}/gschemas
        ${CMAKE_CURRENT_SOURCE_DIR}
        # ${CMAKE_CURRENT_SOURCE_DIR}/totem/src
)   

# message(STATUS "In gtk dir CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
# -- In gtk dir CMAKE_CURRENT_SOURCE_DIR = /media/pal/E/Downloads/libtorrent-SN/gtk


set (gtk_dir_source_dir ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TR_NAME}-gtk
    PRIVATE
        gtk_impl
        totemplugins
        gstbt 
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
        torrent-rasterbar # our own libtorrent with source code
)




target_link_libraries(totemplugins
    PRIVATE
    gtk_impl
)



target_link_libraries(gstbt
    PRIVATE
    gtk_impl # for gstreamer, so we can write the gst-bt plugin
    torrent-rasterbar #for use some types in libtorrent
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



