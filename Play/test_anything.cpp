
#include "libtorrent/libtorrent.hpp"
#include "libtorrent/aux_/session_impl.hpp"
#include <tuple>
#include <functional>
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/torrent_info.hpp"

#include "setup_transfer.hpp"
#include "settings.hpp"
#include "test_utils.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <csignal>
#ifdef TORRENT_UTP_LOG_ENABLE
#include "libtorrent/aux_/utp_stream.hpp"
// #pragma message "TORRENT_UTP_LOG_ENABLE " __FILE__ 
#endif

using namespace lt;


namespace
{

	// set when we're exiting
	std::atomic<bool> shut_down{false};

	void sighandler(int) { 
		std::cout << "sighandler \n\n";
		shut_down = true; }


	void test(const char * file)
	{

		lt::add_torrent_params atp; //from torrent
		settings_pack p = settings();


		// p.set_int(settings_pack::alert_mask, ~0);
		lt::session ses(p);
		std::vector<alert*> alerts;
	
		//from magnet links
		// lt::add_torrent_params atp = lt::parse_magnet_uri(file);
		
	
		atp.flags &= ~torrent_flags::paused;
		atp.flags &= ~torrent_flags::auto_managed;
		atp.save_path = ".";
		atp.ti = std::make_shared<lt::torrent_info>(std::string(file));// from torrent

		auto h = ses.add_torrent(atp);

		

		// set when we're exiting
		for (;;)
		{	

			
				if (shut_down) {
					std::cout << "Exit by SIGINT... \n";
					shut_down = false;
					break;
				}

			

				std::vector<lt::alert*> alerts;
				ses.pop_alerts(&alerts);
				for (lt::alert const* a : alerts) 
				{
					if (auto at = lt::alert_cast<lt::log_alert>(a)) 
					{
						std::cout << at->message() << std::endl;
					}
					else if(auto at = lt::alert_cast<lt::torrent_log_alert>(a))
					{
						std::cout << at->message() << std::endl;

					}
					else if(auto at = lt::alert_cast<lt::peer_log_alert>(a))
					{
						std::cout << at->message() << std::endl;
					}
					else if(auto at = lt::alert_cast<lt::lsd_peer_alert>(a))
					{
						std::cout << at->message() << std::endl;
					}
					else if(auto at = lt::alert_cast<lt::metadata_received_alert>(a))
					{
						std::cout << at->message() << std::endl;
					}
					else if(auto at = lt::alert_cast<lt::metadata_failed_alert>(a))
					{
						std::cout << at->message() << std::endl;
					}
					// else if(auto at = lt::alert_cast<lt::portmap_alert>(a))
					// {
					// 	std::cout << at->message() << std::endl;

					// }
					// else if(auto at = lt::alert_cast<lt::portmap_log_alert>(a))
					// {
					// 	std::cout << at->message() << std::endl;

					// }
					// else if(auto at = lt::alert_cast<lt::torrent_finished_alert>(a))
					// {
					// 	return;
					// }
					else
					{
						std::cout << a->message() << std::endl;
					}
				}
				
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	
	
	}


}






int main(int argc, char* argv[]) try
{
		printf("\n\n\n===================\n\n\n");
		
		if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " <magnet-url>/<torrent-file>" << std::endl;
		return 1;
		}
		printf(">>%s\n\n",argv[1]);
		std::signal(SIGINT, &sighandler);

		test(argv[1]);

		std::cout << "\ndone, shutting down" << std::endl;




}

catch (std::exception const& e) {
	std::cerr << "ERROR: " << e.what() << "\n";
}
