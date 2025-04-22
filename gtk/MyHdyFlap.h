
#include "tr-macros.h"


#include <glibmm.h>
#include <gtkmm.h>
#include <libhandy-1/handy.h>
#include <memory>






class MyHdyFlap : public Gtk::Widget
{

public:
    MyHdyFlap(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder);
    ~MyHdyFlap() override;

    TR_DISABLE_COPY_MOVE(MyHdyFlap)


    void set_fold_policy(bool is_fullscreen);

    void set_reveal_flap(bool visible);

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;

};



