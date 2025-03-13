
#include <iostream>
#include <algorithm>
#include "libtorrent/libtorrent.hpp"
#include "libtorrent/aux_/session_impl.hpp"
#include <tuple>
#include <functional>
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/load_torrent.hpp"
#include "libtorrent/error_code.hpp"
#include "setup_transfer.hpp"
#include "settings.hpp"
#include "test_utils.hpp"


using namespace lt;




std::string branch_path(std::string const& f)
{
	if (f.empty()) return f;

#ifdef TORRENT_WINDOWS
	if (f == "\\\\") return "";
#endif
	if (f == "/") return "";

	auto len = f.size();
	// if the last character is / or \ ignore it
	if (f[len-1] == '/' || f[len-1] == '\\') --len;
	while (len > 0) {
		--len;
		if (f[len] == '/' || f[len] == '\\')
			break;
	}

	if (f[len] == '/' || f[len] == '\\') ++len;
	return std::string(f.c_str(), len);
}



int main(int argc, char* argv[]) try
{
	if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " <magnet-url>" << std::endl;
		return 1;
	}
	
	auto str = branch_path("/home/pal/Desktop/aa4.torrent");
	std::cout << "str is " << str << std::endl;

	
	// lt::add_torrent_params atp = lt::load_torrent_file("/home/pal/Desktop/123/fc2ppv4530999.torrent");
	// std:: cout << atp.save_path << std::endl;
	// if(atp.save_path.empty())
	// {
	// 	std::cout << "Save path is empty" << std::endl;
	// } 


		

	// wait for the user to end
	char a;
	int ret = std::scanf("%c\n", &a);
	(void)ret; // ignore
	return 0;

	
}

catch (std::exception const& e) {
	std::cerr << "ERROR: " << e.what() << "\n";
}