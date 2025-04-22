/*
 * progress bar also diplaying libtorrent::bitfield
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


#include "libtorrent/bitfield.hpp"
using namespace lt;








class BitfieldScaleExtraInit : public Glib::ExtraClassInit
{
public:
    BitfieldScaleExtraInit();

private:
    static void class_init(void* klass, void* user_data);
    static void instance_init(GTypeInstance* instance, void* klass);
};







class BitfieldScale : public BitfieldScaleExtraInit
,public Gtk::Scale
{

public:
    BitfieldScale();
    BitfieldScale(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder);
    ~BitfieldScale() override;

    TR_DISABLE_COPY_MOVE(BitfieldScale)


    void update_bitfield(bitfield const& bitfield_ref);

    void set_num_pieces(int total);

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;

};


