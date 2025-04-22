
#include "MyHdyFlap.h"

#include <iostream>




class MyHdyFlap::Impl
{

public:
    Impl(MyHdyFlap& widget);
    ~Impl();


    TR_DISABLE_COPY_MOVE(Impl)


    void set_fold_policy(bool is_fullscreen);
    void set_reveal_flap(bool visible);

private:
    MyHdyFlap& widget_;

};












MyHdyFlap::MyHdyFlap(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& /*builder*/
)
: Gtk::Widget(cast_item) 
 ,impl_(std::make_unique<Impl>(*this))
{
    std::cout << "MyHdyFlap::MyHdyFlap ctor" << std::endl;
}




MyHdyFlap::~MyHdyFlap() 
{
    std::cout << "MyHdyFlap::~MyHdyFlap() " << std::endl;
}





MyHdyFlap::Impl::Impl(MyHdyFlap& widget) 
:   widget_(widget)
{

}





MyHdyFlap::Impl::~Impl()
{
}








void MyHdyFlap::set_fold_policy(bool is_fullscreen)
{
    impl_->set_fold_policy(is_fullscreen);
}

void MyHdyFlap::Impl::set_fold_policy(bool is_fullscreen)
{

    GtkWidget* cwidget = widget_.gobj();
    if (cwidget)
    {
        // Cast GtkWidget* to HdyFlap* and call the C function
        HdyFlap* hdy_flap_obj = HDY_FLAP(cwidget);
        hdy_flap_set_fold_policy (hdy_flap_obj, is_fullscreen ?
            HDY_FLAP_FOLD_POLICY_ALWAYS : HDY_FLAP_FOLD_POLICY_NEVER);
    }
}





void MyHdyFlap::set_reveal_flap(bool visible)
{
    impl_->set_reveal_flap(visible);
}

void MyHdyFlap::Impl::set_reveal_flap(bool visible)
{
    GtkWidget* cwidget = widget_.gobj();
    if (cwidget)
    {
        // Cast GtkWidget* to HdyFlap* and call the C function
        HdyFlap* hdy_flap_obj = HDY_FLAP(cwidget);
        hdy_flap_set_reveal_flap (hdy_flap_obj, visible);

    }
}
