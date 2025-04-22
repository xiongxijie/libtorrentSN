#include "BitfieldScale.h"
#include <iostream>
#include <glib.h>



class BitfieldScale::Impl
{
public:
    Impl(BitfieldScale& widget);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

    void update_bitfield(bitfield const& bitfield_ref);

    void set_num_pieces(int total);

private:
    bool on_draw_bitfield_scale(const Cairo::RefPtr<Cairo::Context>& cr);

private:
    BitfieldScale& widget_;

    libtorrent::bitfield our_own_copy_ = {};

    int total_num_segments_ = -1;

    GMutex mutex_;
};









BitfieldScaleExtraInit::BitfieldScaleExtraInit()
    : ExtraClassInit(&BitfieldScaleExtraInit::class_init, nullptr, &BitfieldScaleExtraInit::instance_init)
{
}

void BitfieldScaleExtraInit::class_init(void* klass, void* /*user_data*/)
{
    // printf("(BitfieldScaleExtraInit::class_init)\n");
}

void BitfieldScaleExtraInit::instance_init(GTypeInstance* instance, void* /*klass*/)
{
    // printf("(BitfieldScaleExtraInit::instance_init)\n");
}







BitfieldScale::BitfieldScale()
    : Glib::ObjectBase(typeid(BitfieldScale))
{
    // printf("BitfieldScale::BitfieldScale()\n");

}







BitfieldScale::BitfieldScale(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& /*builder*/
)
: Gtk::Scale(cast_item) 
 ,impl_(std::make_unique<Impl>(*this))
{
    std::cout << "BitfieldScale::BitfieldScale ctor" << std::endl;
}


BitfieldScale::~BitfieldScale() 
{
    std::cout << "BitfieldScale::~BitfieldScale() " << std::endl;
}


BitfieldScale::Impl::Impl(BitfieldScale& widget) 
:   widget_(widget)
{
    g_mutex_init(&mutex_);

    widget_.signal_draw().connect(sigc::mem_fun(*this, &BitfieldScale::Impl::on_draw_bitfield_scale));
}


BitfieldScale::Impl::~Impl()
{
    g_mutex_clear(&mutex_);

}









bool BitfieldScale::Impl::on_draw_bitfield_scale(const Cairo::RefPtr<Cairo::Context>& cr)
{
    widget_.on_draw(cr);

    if(our_own_copy_.empty())
    {
        return true;
    }

    int i;
    // Get widget size
    Gtk::Allocation allocation = widget_.get_allocation();
    int width = allocation.get_width();
    int height = allocation.get_height();
    double segment_width;


g_mutex_lock(&mutex_);/****************************************************************************/

    if (total_num_segments_ > 0)
    {
        //the little segment represent a block
        segment_width = (double) width / total_num_segments_;

    }
     
    //Iterate each piece (Sequentially)
    for (i=0; i < total_num_segments_; i++) 
    {
        //we hold this piece, fill with color
        if(our_own_copy_[i])
        {
            cr->set_source_rgb(0.0, 1.0, 0.0); // Green color  

            cr->rectangle(i *segment_width, 
                0, 
                segment_width, 
                height);

            cr->fill();
        }
        else{


        }

    }


g_mutex_unlock(&mutex_);/****************************************************************************/

    return true;  // Indicate that the drawing is handled
}








void BitfieldScale::set_num_pieces(int total)
{
    impl_->set_num_pieces(total);
}
void BitfieldScale::Impl::set_num_pieces(int total)
{
    if(total > 0)
    {
        total_num_segments_ = total;
    }
}




void BitfieldScale::update_bitfield (bitfield const& bitfield_ref)
{
    impl_->update_bitfield(bitfield_ref);
}

//make a ref's copy
void BitfieldScale::Impl::update_bitfield (bitfield const& bitfield_ref)
{
g_mutex_lock(&mutex_);/****************************************************************************/

    our_own_copy_ = bitfield_ref;

g_mutex_unlock(&mutex_);/****************************************************************************/

}