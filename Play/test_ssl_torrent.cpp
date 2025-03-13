
#include "libtorrent/libtorrent.hpp"
#include "libtorrent/aux_/session_impl.hpp"
#include <tuple>
#include <functional>
#include "libtorrent/add_torrent_params.hpp"
#include "setup_transfer.hpp"
#include "settings.hpp"
#include "test_utils.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <cstdint>
#include <cstdlib>
#ifdef TORRENT_UTP_LOG_ENABLE
#include "libtorrent/aux_/utp_stream.hpp"
#endif

using namespace lt;


namespace
{

	void test(char * file, char *mode, int seeder_port=0)
	{




		char listen_iface[100];

		lt::add_torrent_params atp;
		settings_pack p = settings();

		bool use_utp = false;
		p.set_bool(settings_pack::enable_incoming_utp, use_utp);
		p.set_bool(settings_pack::enable_outgoing_utp, use_utp);
		p.set_bool(settings_pack::enable_incoming_tcp, !use_utp);
		p.set_bool(settings_pack::enable_outgoing_tcp, !use_utp);


		if(!strcmp(mode ,"leecher"))
		{
			printf("YOU ARE DOWNLOADER !!! \n");
			int port = 1024 + rand() % 50000;
			std::snprintf(listen_iface, sizeof(listen_iface), "0.0.0.0:%d,0.0.0.0:%ds", port + 20, port);
		}
		else
		{
			printf("YOU ARE SEEDER !!! \n");
			int port = 512 + rand() % 49789;
			std::snprintf(listen_iface, sizeof(listen_iface), "0.0.0.0:%ds", port);
			atp.flags |= torrent_flags::seed_mode;
		}
		p.set_str(settings_pack::listen_interfaces, listen_iface);


		// p.set_int(settings_pack::alert_mask, ~0);
		lt::session ses(p);
		std::vector<alert*> alerts;
	

		
		atp.flags &= ~torrent_flags::paused;
		atp.flags &= ~torrent_flags::auto_managed;
		atp.save_path = ".";
		atp.ti = std::make_shared<lt::torrent_info>(file);
		auto h = ses.add_torrent(atp);

		wait_for_listen(ses, "ses");

		h.set_ssl_certificate("./ssl/peer_certificate.pem", "./ssl/peer_private_key.pem", "./ssl/dhparams.pem", "test");

		ses.listen_port();

		if(!strcmp(mode ,"seeder"))
			wait_for_alert(ses, torrent_finished_alert::alert_type, "seeder");

		if(!strcmp(mode ,"leecher"))
			wait_for_downloading(ses, "downloader");

		if(seeder_port != 0)
		{
			printf("````h.connect_peer(127.0.0.1:%d)`````\n\n", seeder_port);
			error_code ec;
			h.connect_peer(tcp::endpoint(make_address("127.0.0.1", ec), std::uint16_t(seeder_port)));
		}


		for (;;)
		{
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
					// else if(auto at = lt::alert_cast<lt::portmap_alert>(a))
					// {
					// 	std::cout << at->message() << std::endl;

					// }
					// else if(auto at = lt::alert_cast<lt::portmap_log_alert>(a))
					// {
					// 	std::cout << at->message() << std::endl;

					// }
					// else if(auto at = lt::alert_cast<lt::torrent_need_cert_alert>(a))
					// {
					// 	std::cout << at->message() << std::endl;
					// 	h.set_ssl_certificate("./ssl/peer_certificate.pem", "./ssl/peer_private_key.pem", "./ssl/dhparams.pem", "test");
					// 	std::cout << "TORRENT SET CERT FINISHED" << std::endl;
					// 	if(seeder_port != 0)
					// 	{
					// 		printf("````h.connect_peer(129.0.0.1:%d)`````\n\n", seeder_port);
					// 		error_code ec;
					// 		h.connect_peer(tcp::endpoint(make_address("127.0.0.1", ec)
					// 		, std::uint16_t(seeder_port)));
					// 	}
					// }
					// else if(auto at = lt::alert_cast<lt::torrent_finished_alert>(a))
					// {
					// 	return;
					// }
				}
				
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}

}






int main(int argc, char* argv[]) try
{

	if(argc<4)
		test(argv[1], argv[2]);
	else
		test(argv[1], argv[2], atoi(argv[3]));

}

catch (std::exception const& e) {
	std::cerr << "ERROR: " << e.what() << "\n";
}