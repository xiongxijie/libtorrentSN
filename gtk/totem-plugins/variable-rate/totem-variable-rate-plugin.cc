/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2016 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

// #include "config.h"

// #include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
// #include <gdk/gdkkeysyms.h>
#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>


#ifdef __cplusplus
extern "C" {
#endif
#include "totem-plugin.h"
#ifdef __cplusplus
}
#endif

#include "BaconVideoWidget.h"
#include "TotemWrapper.h"
#include "totem-plugins-engine.h"
#include "TotemPluginFoo.h" // just for MenuPlaceHolderType


#define TOTEM_TYPE_VARIABLE_RATE_PLUGIN		(totem_variable_rate_plugin_get_type ())
#define TOTEM_VARIABLE_RATE_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePlugin))

typedef struct {
	PeasExtensionBase parent;

	TotemWrapper       *totem;

	Glib::RefPtr<Gio::SimpleAction> action;

} TotemVariableRatePlugin;

#define NUM_RATES 6
#define NORMAL_RATE_IDX 1

/* NOTE:
 * Keep in sync with mpris/totem-mpris.c */
static struct {
	float rate;
	const char *label;
	const char *id;
} rates[NUM_RATES] = {
	{ 0.75,   "× 0.75", "0_75" },
	{ 1.0,    "Normal", "normal" },
	{ 1.1,    "× 1.1",  "1_1" },
	{ 1.25,   "× 1.25", "1_25" },
	{ 1.5,    "× 1.5",  "1_5" },
	{ 1.75,   "× 1.75", "1_75" }
};

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePlugin, totem_variable_rate_plugin)

static char *
get_submenu_label_for_index (guint i)
{
	return g_strdup_printf ("Speed: %s", rates[i].label);
}

static void
variable_rate_action_callback (const Glib::VariantBase& parameter, TotemVariableRatePlugin *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	char *label;
	guint i;

	Glib::ustring action_val = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(parameter).get();
	for (i = 0; i < NUM_RATES; i++)
	{
		if (g_strcmp0 (action_val.c_str(), rates[i].id) == 0)
			break;
	}
	g_assert (i < NUM_RATES);

	if (!totem_wrapper_set_rate (pi->totem, rates[i].rate)) {
		printf ("Failed to set rate to %f, resetting \n", rates[i].rate);
		i = NORMAL_RATE_IDX;
		if (!totem_wrapper_set_rate (pi->totem, rates[i].rate))
			printf ("And failed to reset rate as well...\n");
		else
			printf ("resetting success.\n");

	} else {
		printf ("Managed to set rate to %f\n", rates[i].rate);
	}

	// SimpleAction::set_state() is "directly" updates differs from the change_state() methods from Gio::Action who "indirectly" request the change 
	pi->action->set_state(parameter);

	label = get_submenu_label_for_index (i);
	/* FIXME how do we change the section label?
	 * https://gitlab.gnome.org/GNOME/glib/issues/498 */
	g_free (label);

}

/*
This plugin is not built-in, you should enable it in preferences dialog
*/

static void
impl_activate (PeasActivatable *plugin)
{

	
	printf ("(totem-variable-rate-plugin) impl_activate \n");


	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	Glib::RefPtr<Gio::Menu> menu;
	Glib::RefPtr<Gio::MenuItem> item;
	guint i;

	// PeasActivatable:"object" property contains the targetted object for this #PeasActivatable instance
	pi->totem = TOTEM_WRAPPER (g_object_get_data (G_OBJECT (plugin), "object"));



	/* Create the variable rate action */
	pi->action = Gio::SimpleAction::create("variable-rate",
											Glib::VariantType("s"),
											Glib::Variant<Glib::ustring>::create(rates[NORMAL_RATE_IDX].id));
	pi->action->signal_change_state().connect(sigc::bind(sigc::ptr_fun(&variable_rate_action_callback), pi));
	if(pi->action)
		totem_wrapper_add_plugin_action(pi->totem, pi->action);



	/* Create the submenu */
	menu = totem_wrapper_get_menu_section(pi->totem, MenuPlaceHolderType::VARIABLE_RATE_PLACEHOLDER);

	for (i = 0; i < NUM_RATES; i++) {
		char *target;
		target = g_strdup_printf ("app.variable-rate::%s", rates[i].id);
		item = Gio::MenuItem::create(rates[i].label, target);
		g_free (target);
		menu->append_item (item);
	}
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	
	/* Remove the menu */
	totem_wrapper_empty_menu_section (pi->totem, MenuPlaceHolderType::VARIABLE_RATE_PLACEHOLDER);

	/* Reset the rate */
	if (!totem_wrapper_set_rate (pi->totem, 1.0))
		printf ("Failed to reset the playback rate to 1.0 \n");
}
