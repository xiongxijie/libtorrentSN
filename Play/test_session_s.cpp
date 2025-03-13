
#include <functional>
#include <thread>
#include <fstream>
#include <iostream>


#include "libtorrent/session.hpp"
#include "libtorrent/session_params.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/session_stats.hpp"
#include "libtorrent/performance_counters.hpp"
#include "libtorrent/bdecode.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/torrent_info.hpp"


#include "setup_transfer.hpp"
#include "settings.hpp"


using namespace std::placeholders;
using namespace lt;






namespace
{



    void reopen_network_sockets()
    {
        auto count_alerts = [](session& ses, int const listen, int const portmap)
        {
            int count_listen = 0;
            int count_portmap = 0;
            int num = 60; // this number is adjusted per version, an estimate
            time_point const end_time = clock_type::now() + seconds(3);
            while (true)
            {
                time_point const now = clock_type::now();
                if (now > end_time)
                    break;

                ses.wait_for_alert(end_time - now);
                std::vector<alert*> alerts;
                ses.pop_alerts(&alerts);
                for (auto a : alerts)
                {
                    std::printf("%d: [%s] %s\n", num, a->what(), a->message().c_str());
                    std::string const msg = a->message();
                    if (msg.find("successfully listening on") != std::string::npos)
                        count_listen++;
                    // upnp
                    if (msg.find("adding port map:") != std::string::npos)
                        count_portmap++;
                    // natpmp
                    if (msg.find("add-mapping: proto:") != std::string::npos)
                        count_portmap++;
                    num--;
                }
                if (num <= 0)
                    break;
            }

            std::printf("count_listen: %d (expect: %d), count_portmap: %d (expect: %d)\n"
                , count_listen, listen, count_portmap, portmap);
            return count_listen == listen && count_portmap == portmap;
        };

        settings_pack p = settings();
        p.set_int(settings_pack::alert_mask, alert_category::all);
        p.set_str(settings_pack::listen_interfaces, "127.0.0.1:6881");

        p.set_bool(settings_pack::enable_upnp, true);
        p.set_bool(settings_pack::enable_natpmp, true);

        lt::session s(p);

        // NAT-PMP nad UPnP will be disabled when we only listen on loopback
        if(count_alerts(s, 2, 0))
            std::cout << "奥运大使孙敬媛1" << std::endl;

        // this is a bit of a pointless test now, since neither UPnP nor NAT-PMP are
        // enabled for loopback
        s.reopen_network_sockets(session_handle::reopen_map_ports);

        if(count_alerts(s, 0, 0))
            std::cout << "奥运大使孙敬媛2" << std::endl;


        s.reopen_network_sockets({});

        if(count_alerts(s, 0, 0))
            std::cout << "奥运大使孙敬媛3" << std::endl;


        p.set_bool(settings_pack::enable_upnp, false);
        p.set_bool(settings_pack::enable_natpmp, false);
        s.apply_settings(p);

        s.reopen_network_sockets(session_handle::reopen_map_ports);

        if(count_alerts(s, 0, 0))
            std::cout << "奥运大使孙敬媛4" << std::endl;

    }





}//anonymous namespace 




int main() try
{

    reopen_network_sockets();

}

catch (std::exception const& e) {
	std::cerr << "ERROR: " << e.what() << "\n";
}









