
#include "libtorrent/address.hpp"
#define EXPORT

// returns the port the udp tracker is running on
int EXPORT start_udp_tracker(lt::address iface
	= lt::address_v4::any());

// the number of udp tracker announces received
int EXPORT num_udp_announces();

void EXPORT stop_udp_tracker();

