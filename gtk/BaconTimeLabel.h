/*
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */


#pragma once


#include "tr-macros.h"

#include <glibmm.h>
#include <gtkmm.h>
#include <memory>
#include <glibmm/extraclassinit.h>


#include <pangomm.h>//for adjust font style for the underlying Gtk::Label




class BaconTimeLabelExtraInit : public Glib::ExtraClassInit
{
public:
    BaconTimeLabelExtraInit();

private:
    static void class_init(void* klass, void* user_data);
    static void instance_init(GTypeInstance* instance, void* klass);
};





class BaconTimeLabel : 
public BaconTimeLabelExtraInit
,public Gtk::Bin
{

public:
    BaconTimeLabel();
    BaconTimeLabel(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, bool is_remaining);
    ~BaconTimeLabel() override;

    TR_DISABLE_COPY_MOVE(BaconTimeLabel)

    void set_time(std::int64_t _time, std::int64_t length);
    void reset ();
    void set_remaining (bool remaining);
    void set_show_msecs (bool show_msecs);

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;

};







