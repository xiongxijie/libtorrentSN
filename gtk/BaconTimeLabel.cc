
#include "Utils.h"
#include "BaconTimeLabel.h"



#include <iostream>
#include <cstdint>  // Required for std::int64_t
#include <string>
#include <cmath>
#include <iomanip>
#include <sstream>




namespace {


enum class TotemTimeFlag {
    NONE         = 0,
    MSECS        = 1 << 0,  // Show milliseconds
    REMAINING    = 1 << 1,  // Show as remaining time (negative)
    FORCE_HOUR   = 1 << 2   // Force hour display even when <1 hour
};


constexpr TotemTimeFlag operator|(TotemTimeFlag a, TotemTimeFlag b) {
    return static_cast<TotemTimeFlag>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr TotemTimeFlag operator&(TotemTimeFlag a, TotemTimeFlag b) {
    return static_cast<TotemTimeFlag>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr TotemTimeFlag& operator|=(TotemTimeFlag& a, TotemTimeFlag b) {
    a = a | b;
    return a;
}

constexpr TotemTimeFlag& operator&=(TotemTimeFlag& a, TotemTimeFlag b) {
    a = a & b;
    return a;
}

//some helper functions used in this BaconTimeLabel


std::string totem_time_to_string(int64_t msecs, TotemTimeFlag flags) {
    // Handle negative case (unknown time)
    if (msecs < 0) {
        return "--:--";
    }

    // Calculate time components
    const int msec = msecs % 1000;
    int64_t seconds_total;

    if ((flags & TotemTimeFlag::MSECS) == TotemTimeFlag::MSECS) {
        // Keep full precision when showing milliseconds
        seconds_total = msecs / 1000;
    } else {
        // Round seconds based on flags
        const double seconds = static_cast<double>(msecs) / 1000.0;
        seconds_total = static_cast<int64_t>(
            (flags & TotemTimeFlag::REMAINING) != TotemTimeFlag::NONE ? 
            std::ceil(seconds) : std::round(seconds));
    }

    // Break down into hours, minutes, seconds
    const int sec = seconds_total % 60;
    const int min = (seconds_total / 60) % 60;
    const int hour = seconds_total / 3600;

    // Format the output string
    std::ostringstream oss;
    oss << std::setfill('0');

    const bool show_hours = (hour > 0) || 
                           ((flags & TotemTimeFlag::FORCE_HOUR) != TotemTimeFlag::NONE);
    const bool is_remaining = (flags & TotemTimeFlag::REMAINING) != TotemTimeFlag::NONE;
    const bool show_msecs = (flags & TotemTimeFlag::MSECS) != TotemTimeFlag::NONE;

    if (is_remaining) {
        oss << "-";
    }

    if (show_hours) {
        oss << hour << ":"
            << std::setw(2) << min << ":"
            << std::setw(2) << sec;
    } else {
        oss << min << ":"
            << std::setw(2) << sec;
    }

    if (show_msecs) {
        oss << "." << std::setw(3) << msec;
    }

    return oss.str();
}



}//anonymous namespace







class BaconTimeLabel::Impl
{
    public:
        Impl(BaconTimeLabel& widget, bool is_remaining);
        ~Impl();

        TR_DISABLE_COPY_MOVE(Impl)

        void set_time(std::int64_t _time, std::int64_t length);
        void set_show_msecs (bool show_msecs);
        void set_remaining(bool remaining);
        void reset();

    private:

        void update_label_text();

    private:
        BaconTimeLabel& widget_;
        
        Gtk::Label *time_label_ = nullptr;

        std::int64_t time_;
        std::int64_t length_;

	    bool remaining_;
	    bool show_msecs_ = false;
};

















BaconTimeLabelExtraInit::BaconTimeLabelExtraInit()
    : ExtraClassInit(&BaconTimeLabelExtraInit::class_init, nullptr, &BaconTimeLabelExtraInit::instance_init)
{
}


void BaconTimeLabelExtraInit::class_init(void* klass, void* /*user_data*/)
{
    // printf ("(BaconTimeLabelExtraInit::class_init)\n");

    // auto* const widget_klass = GTK_WIDGET_CLASS(klass);

}


void BaconTimeLabelExtraInit::instance_init(GTypeInstance* instance, void* /*klass*/)
{
    // printf ("(BaconTimeLabelExtraInit::instance_init)\n");

}











BaconTimeLabel::BaconTimeLabel()
    : Glib::ObjectBase(typeid(BaconTimeLabel))
{
}









BaconTimeLabel::BaconTimeLabel(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& /*builder*/,
    bool is_remaining
)
    : Glib::ObjectBase(typeid(BaconTimeLabel))
    , Gtk::Bin(cast_item)
    , impl_(std::make_unique<Impl>(*this, is_remaining))
{
    std::cout << "BaconTimeLabel::BaconTimeLabel ctor" << std::endl;
}










// Constructor that loads its own .ui file using its own Gtk::Builder
BaconTimeLabel::Impl::Impl(BaconTimeLabel& widget, bool is_remaining) 
: widget_(widget),
time_label_(Gtk::make_managed<Gtk::Label>())
{
    
    std::string init_time_string = totem_time_to_string (0, TotemTimeFlag::NONE);
    time_label_->set_text(init_time_string);

    /*set font style of label*/
    // no niclude pangomm headers , gtkmm already knows them
    Pango::AttrList attrs = {};
    if(attrs)
    {
        // std::cout << " Pango::AttrList is valid" << std::endl;
        Pango::Attribute attr = Pango::Attribute::create_attr_font_features("tnum=1");
        attrs.insert(attr);
        time_label_->set_attributes(attrs);
    }
    else
    {
        std::cout << " Pango::AttrList is NOT valid" << std::endl;

    }
    

    time_ = 0;
	length_ = -1;
	remaining_ = is_remaining;

	time_label_->show();
    widget_.add(*time_label_);
}







BaconTimeLabel::~BaconTimeLabel() 
{
    std::cout << "BaconTimeLabel::~BaconTimeLabel() " << std::endl;
}











BaconTimeLabel::Impl::~Impl()
{
}



void BaconTimeLabel::Impl::update_label_text ()
{
	std::string label_str;
	std::int64_t _time, length;
	TotemTimeFlag flags;

	_time = time_;
	length = length_;

	flags = remaining_ ? TotemTimeFlag::REMAINING : TotemTimeFlag::NONE;
	if (length > 60 * 60 * 1000)
		flags |= TotemTimeFlag::FORCE_HOUR;
	flags |= show_msecs_ ? TotemTimeFlag::MSECS : TotemTimeFlag::NONE;

	if (length <= 0 || _time > length)
		label_str = totem_time_to_string (remaining_ ? -1 : _time, flags);
	else
		label_str = totem_time_to_string (remaining_ ? length - _time : _time, flags);

    time_label_->set_text(label_str);
}




void BaconTimeLabel::set_time(std::int64_t _time, std::int64_t length)
{
    impl_->set_time(_time, length);
}

void BaconTimeLabel::Impl::set_time(std::int64_t _time, std::int64_t length)
{
	if (length_ == -1 && length == -1)
		return;

	if (!show_msecs_) 
    {
		if (_time / 1000 == time_ / 1000 &&
		    length / 1000 == length_ / 1000)
			return;
	}

	time_ = _time;
	length_ = length;

	update_label_text();
}
        
     
        
        
/*PUBLIC METHODS*/
void BaconTimeLabel::reset ()
{
    impl_->reset();
}
void BaconTimeLabel::Impl::reset()
{
	set_show_msecs (FALSE);
	set_time (0, 0);
}




void BaconTimeLabel::set_remaining (bool remaining)
{
    impl_->set_remaining(remaining);
}
void BaconTimeLabel::Impl::set_remaining (bool remaining)
{
	remaining_ = remaining;
	update_label_text();
}



void BaconTimeLabel::set_show_msecs (bool show_msecs)
{
    impl_->set_show_msecs(show_msecs);
}
void BaconTimeLabel::Impl::set_show_msecs (bool show_msecs)
{
	if (show_msecs != show_msecs_) 
    {
		show_msecs_ = show_msecs;
		update_label_text();
	}
}
