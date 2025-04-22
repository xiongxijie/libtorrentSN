/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Plugin engine for Totem, heavily based on the code from Rhythmbox,
 * which is based heavily on the code from totem.
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *               2006 James Livingston  <jrl@ids.org.au>
 *               2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <glib.h>
#include <libpeas/peas-engine.h>
#include <libpeas/peas-autocleanups.h>


#include "TotemWrapper.h" // For TotemWrapper, proxying MyHdyApplicationWindow using ITotemWindow for us, provide c-style function we prefer



/**
 * TOTEM_GSETTINGS_SCHEMA:
 *
 * The GSettings schema under which all Totem settings are stored.
 * 
 * schema english meaning : a drawing that represents an idea or theory and makes it easier to understand
 * 
 * 
 * GSettings uses compiled schemas (i.e., gschemas.compiled) rather than the original .gschema.xml files. 
 * When you define a schema, such as #define TOTEM_GSETTINGS_SCHEMA "org.gnome.totem", the corresponding schema 
 * is defined in an XML file (org.gnome.totem.gschema.xml) and then compiled into a 
 * system-wide gschemas.compiled file using `glib-compile-schemas`.
 * 
 * 
 * '''example
 * pal@pal-desktop79:~$ glib-compile-schemas -h
 *      Compile all GSettings schema files into a schema cache.
 *      Schema files are required to have the extension .gschema.xml,
 *      and the cache file is called gschemas.compiled.
 * '''
 **/
#define TOTEM_GSETTINGS_SCHEMA "org.gnome.trtotem"








//if this header included by a cpp source file, we should add extern "c" , avoid such undefined reference errors when linking due to c++ name mangling
#ifdef __cplusplus
extern "C" {
#endif



#define TOTEM_TYPE_PLUGINS_ENGINE              (totem_plugins_engine_get_type ())
G_DECLARE_FINAL_TYPE(TotemPluginsEngine, totem_plugins_engine, TOTEM, PLUGINS_ENGINE, PeasEngine)

//// GType			totem_plugins_engine_get_type			(void) G_GNUC_CONST;
/*
cannot directly use C++ types (like MyHdyApplicationWindow, a GTKmm class) in C code 

because C does not understand C++ classes and their type system
*/
TotemPluginsEngine	*totem_plugins_engine_get_default		(TotemWrapper *totem);
// cannot directly call C++ methods on a C++ object pointer from C code
void			totem_plugins_engine_shut_down			(TotemPluginsEngine *self);



#ifdef __cplusplus
}
#endif