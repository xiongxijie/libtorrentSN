#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <functional>
#include "libtorrent/libtorrent.hpp"
#include <boost/range/combine.hpp>
#include "test_utils.hpp"
#include "settings.hpp"
#include "setup_transfer.hpp"
#include "libtorrent/aux_/file_view_pool.hpp"


using namespace lt;
using namespace lt::aux;
using lt::mmap_storage;


constexpr int piece_size = 16 * 1024 * 16;
constexpr int half = piece_size / 2;
void delete_dirs(std::string path)
{
	path = complete(path);
	error_code ec;
	remove_all(path, ec);
	if (ec && ec != boost::system::errc::no_such_file_or_directory)
	{
		std::printf("remove_all \"%s\": %s\n"
			, path.c_str(), ec.message().c_str());
	}
	if(!exists(path)) std::cout <<"\nIn delete_dirs: dirs deleted Successfully\n" << std::endl;

}

std::vector<char> new_piece(std::size_t const size)
{
	std::vector<char> ret(size);
	aux::random_bytes(ret);
	return ret;
}



void run_until(io_context& ios, bool const& done)
{
	while (!done)
	{
		ios.restart();
		ios.run_one();
		std::cout << time_now_string() << " done: " << done << std::endl;
	}
}

void print_error(char const* call, int ret, storage_error const& ec)
{
	std::printf("%s: %s() returned: %d error: \"%s\" in file: %d operation: %s\n"
		, time_now_string().c_str(), call, ret, ec.ec.message().c_str()
		, static_cast<int>(ec.file()), operation_name(ec.operation));
}


void on_check_resume_data(lt::status_t const status, storage_error const& error, bool* done, bool* oversized)
{
	std::cout << time_now_string() << " on_check_resume_data ret: "
		<< static_cast<int>(status);
	if ((status & lt::status_t::oversized_file) != status_t{})
	{
		std::cout << " oversized file(s) - ";
		*oversized = true;
	}
	else
	{
		*oversized = false;
	}

	switch (status & ~lt::status_t::mask)
	{
		case lt::status_t::no_error:
			std::cout << " success" << std::endl;
			break;
		case lt::status_t::fatal_disk_error:
			std::cout << " disk error: " << error.ec.message()
				<< " file: " << error.file() << std::endl;
			break;
		case lt::status_t::need_full_check:
			std::cout << " need full check" << std::endl;
			break;
		case lt::status_t::file_exist:
			std::cout << " file exist" << std::endl;
			break;
		case lt::status_t::mask:
		case lt::status_t::oversized_file:
			break;
	}
	std::cout << std::endl;
	*done = true;
}


void on_piece_checked(piece_index_t, sha1_hash const&
	, storage_error const& error, bool* done)
{
	std::cout << time_now_string() << " on_piece_checked err: "
		<< error.ec.message() << '\n';
	*done = true;
}



using lt::operator""_bit;
using check_files_flag_t = lt::flags::bitfield_flag<std::uint64_t, struct check_files_flag_type_tag>;
constexpr check_files_flag_t sparse = 0_bit;
constexpr check_files_flag_t test_oversized = 1_bit;
constexpr check_files_flag_t zero_prio = 2_bit;




int write(std::shared_ptr<mmap_storage> s
	, aux::session_settings const& sett
	, span<char> buf
	, piece_index_t const piece, int const offset
	, aux::open_mode_t const mode
	, storage_error& error)
{
	return s->write(sett, buf, piece, offset, mode, disk_job_flags_t{}, error);
}
int read(std::shared_ptr<mmap_storage> s
	, aux::session_settings const& sett
	, span<char> buf
	, piece_index_t piece
	, int const offset
	, aux::open_mode_t mode
	, storage_error& ec)
{
	return s->read(sett, buf, piece, offset, mode, disk_job_flags_t{}, ec);
}


void release_files(std::shared_ptr<mmap_storage> s, storage_error& ec)
{
	s->release_files(ec);
}


void fill_pattern(span<char> buf)
{
	int counter = 0;
	for (char& v : buf)
	{
		v = char(counter & 0xff);
		++counter;
	}
}


bool check_pattern(std::vector<char> const& buf, int counter)
{
	unsigned char const* p = reinterpret_cast<unsigned char const*>(buf.data());
	for (int k = 0; k < int(buf.size()); ++k)
	{
		if (p[k] != (counter & 0xff)) return false;
		++counter;
	}
	return true;
}


file_storage make_fs()
{
	file_storage fs;
	fs.add_file(combine_path("readwrite", "1"), 3);
	fs.add_file(combine_path("readwrite", "2"), 9);
	fs.add_file(combine_path("readwrite", "3"), 81);
	fs.add_file(combine_path("readwrite", "4"), 6561);
	fs.set_piece_length(0x1000);
	fs.set_num_pieces(aux::calc_num_pieces(fs));
	return fs;
}

struct test_fileop
{
	explicit test_fileop(int stripe_size) : m_stripe_size(stripe_size) {}

	int operator()(file_index_t const file_index, std::int64_t const file_offset
		, span<char> buf, storage_error&)
	{
		std::size_t offset = size_t(file_offset);
		if (file_index >= m_file_data.end_index())
		{
			m_file_data.resize(static_cast<int>(file_index) + 1);
		}

		std::size_t const write_size = std::size_t(std::min(m_stripe_size, int(buf.size())));
		buf = buf.first(int(write_size));

		std::vector<char>& file = m_file_data[file_index];

		if (offset + write_size > file.size())
			file.resize(offset + write_size);

		std::memcpy(&file[offset], buf.data(), write_size);
		return int(write_size);
	}

	int m_stripe_size;
	aux::vector<std::vector<char>, file_index_t> m_file_data;
};


struct test_read_fileop
{
	// EOF after size bytes read
	explicit test_read_fileop(int size) : m_size(size), m_counter(0) {}

	int operator()(file_index_t, std::int64_t /*file_offset*/
		, span<char> buf, storage_error&)
	{
		if (buf.size() > m_size)
			buf = buf.first(m_size);
		for (char& v : buf)
		{
			v = char(m_counter & 0xff);
			++m_counter;
		}
		m_size -= int(buf.size());
		return int(buf.size());
	}

	int m_size;
	int m_counter;
};

struct test_error_fileop
{
	// EOF after size bytes read
	explicit test_error_fileop(file_index_t error_file)
		: m_error_file(error_file) {}

	int operator()(file_index_t const file_index, std::int64_t /*file_offset*/
		, span<char> buf, storage_error& ec)
	{
		if (m_error_file == file_index)
		{
			ec.file(file_index);
			ec.ec.assign(boost::system::errc::permission_denied
				, boost::system::generic_category());
			ec.operation = operation_t::file_read;
			return 0;
		}
		return int(buf.size());
	}

	file_index_t m_error_file;
};

std::shared_ptr<torrent_info> setup_torrent_info(file_storage& fs
	, std::vector<char>& buf)
{
	fs.add_file(combine_path("temp_storage", "test1.tmp"), 0x8000);
	fs.add_file(combine_path("temp_storage",  "test2.tmp"), 0x8000);
	fs.add_file(combine_path("temp_storage",  "test3.tmp"), 0x0000);
	// fs.add_file(combine_path("temp_storage", combine_path("folder2", "test3.tmp")), 0);
	// fs.add_file(combine_path("temp_storage", combine_path("_folder3", "test4.tmp")), 0);
	// fs.add_file(combine_path("temp_storage", combine_path("_folder3", combine_path("subfolder", "test5.tmp"))), 0x8000);
	lt::create_torrent t(fs, 0x4000);

	
	std::cout << fs.num_pieces() << std::endl;
	sha1_hash h = hasher(std::vector<char>(0x4000, 0)).final();
	std::cout << "sha1_hash is >> " << h.to_string().length() << std::endl;
	for (piece_index_t i(0); i < 4_piece; ++i) t.set_hash(i, h);

	bencode(std::back_inserter(buf), t.generate());


	std::string const test_path = current_working_directory();
	ofstream(combine_path(test_path, "SSNI123").c_str())
	.write(buf.data(), std::streamsize(buf.size()));


	error_code ec;

	auto info = std::make_shared<torrent_info>(buf, ec, from_span);

	if (ec)
	{
		std::printf("torrent_info constructor failed: %s\n"
			, ec.message().c_str());
		throw system_error(ec);
	}

	return info;
}



std::shared_ptr<mmap_storage> setup_torrent(file_storage& fs
	, aux::file_view_pool& fp
	, std::vector<char>& buf
	, std::string const& test_path
	, aux::session_settings& set)
{
	std::shared_ptr<torrent_info> info = setup_torrent_info(fs, buf);

	aux::vector<download_priority_t, file_index_t> priorities;
	sha1_hash info_hash;
	storage_params p{
		fs,
		nullptr,
		test_path,
		storage_mode_allocate,
		priorities,
		info_hash
	};
	auto s = std::make_shared<mmap_storage>(p, fp);
	// allocate the files and create the directories
	storage_error se;
	s->initialize(set, se);
	if (se)
	{
		//TEST_ERROR(se.ec.message().c_str());
		std::printf("mmap_storage::initialize %s: %d\n"
			, se.ec.message().c_str(), static_cast<int>(se.file()));
		throw system_error(se.ec);
	}

	return s;
}
namespace
{
	void sync(lt::io_context& ioc, int& outstanding)
	{
		while (outstanding > 0)
		{
			ioc.run_one();
			ioc.restart();
		}
	}

	struct write_handler
	{
		write_handler(int& outstanding) : m_out(&outstanding) {}
		void operator()(lt::storage_error const& ec) const
		{
			--(*m_out);
			if (ec) std::cout << "async_write failed " << ec.ec.message() << '\n';
			//TEST_CHECK(!ec);
		}
		int* m_out;
	};

	struct read_handler
	{
		read_handler(int& outstanding, lt::span<char const> expected) : m_out(&outstanding), m_exp(expected) {}
		void operator()(lt::disk_buffer_holder h, lt::storage_error const& ec) const
		{
			--(*m_out);
			if (ec) std::cout << "async_read failed " << ec.ec.message() << '\n';
			//TEST_CHECK(!ec);
			// TEST_CHECK(m_exp == lt::span<char const>(h.data(), h.size()));
			std::string const test_path = current_working_directory();
			ofstream(combine_path(test_path, "SONE154").c_str())
			.write(h.data(), std::streamsize(h.size()));
		}
		int* m_out;
		lt::span<char const> m_exp;
	};


	void both_sides_from_store_buffer(lt::disk_interface* disk_io, lt::storage_holder const& t, lt::io_context& ioc, int& outstanding)
	{
		std::vector<char> write_buffer(lt::default_block_size * 2);
		aux::random_bytes(write_buffer);

		lt::peer_request const req0{0_piece, 0, lt::default_block_size};
		lt::peer_request const req1{0_piece, lt::default_block_size, lt::default_block_size};

		// this is the unaligned read request
		lt::peer_request const req2{0_piece, lt::default_block_size / 2, lt::default_block_size};

		std::vector<char> const expected_buffer(write_buffer.begin() + req2.start
			, write_buffer.begin() + req2.start + req2.length);

		++outstanding;
		disk_io->async_write(t, req0, write_buffer.data(), {}, write_handler(outstanding));
		++outstanding;
		disk_io->async_write(t, req1, write_buffer.data() + lt::default_block_size, {}, write_handler(outstanding));
		++outstanding;
		disk_io->async_read(t, req2, read_handler(outstanding, expected_buffer));
		disk_io->submit_jobs();
		sync(ioc, outstanding);
	}



	void first_side_from_store_buffer(lt::disk_interface* disk_io, lt::storage_holder const& t, lt::io_context& ioc, int& outstanding)
	{
		std::vector<char> write_buffer(lt::default_block_size * 2);
		aux::random_bytes(write_buffer);

		lt::peer_request const req0{0_piece, 0, lt::default_block_size};
		lt::peer_request const req1{0_piece, lt::default_block_size, lt::default_block_size};

		// this is the unaligned read request
		lt::peer_request const req2{0_piece, lt::default_block_size / 2, lt::default_block_size};

		std::vector<char> const expected_buffer(write_buffer.begin() + req2.start
			, write_buffer.begin() + req2.start + req2.length);

		++outstanding;
		disk_io->async_write(t, req0, write_buffer.data(), {}, write_handler(outstanding));

		disk_io->submit_jobs();
		sync(ioc, outstanding);

		++outstanding;
		disk_io->async_write(t, req1, write_buffer.data() + lt::default_block_size, {}, write_handler(outstanding));
		++outstanding;
		disk_io->async_read(t, req2, read_handler(outstanding, expected_buffer));
		disk_io->submit_jobs();
		sync(ioc, outstanding);
	}



	template <typename Fun>
	void test_unaligned_read(lt::disk_io_constructor_type constructor, Fun fun)
	{
		lt::io_context ioc;
		lt::counters cnt;
		lt::settings_pack pack;
		pack.set_int(lt::settings_pack::aio_threads, 1);
		pack.set_int(lt::settings_pack::file_pool_size, 2);

		std::unique_ptr<lt::disk_interface> disk_io
			= constructor(ioc, pack, cnt);

		lt::file_storage fs;

		fs.add_file("test", lt::default_block_size * 2);
		fs.set_num_pieces(1);
		fs.set_piece_length(lt::default_block_size * 2);

		std::string const save_path = complete("save_path");
		delete_dirs(combine_path(save_path, "test"));

		lt::aux::vector<lt::download_priority_t, lt::file_index_t> prios;
		lt::storage_params params(fs, nullptr
			, save_path
			, lt::storage_mode_sparse
			, prios
			, lt::sha1_hash("01234567890123456789"));

		lt::storage_holder t = disk_io->new_torrent(params, {});

		int outstanding = 0;
		lt::add_torrent_params atp;
		disk_io->async_check_files(t, &atp, lt::aux::vector<std::string, lt::file_index_t>{}
			, [&](lt::status_t, lt::storage_error const&) { --outstanding; std::cout << "check file sucess"<<std::endl;});
		++outstanding;
		disk_io->submit_jobs();
		sync(ioc, outstanding);

		fun(disk_io.get(), t, ioc, outstanding);

		t.reset();
		disk_io->abort(true);
	}

}



int main() try
{
	test_unaligned_read(lt::mmap_disk_io_constructor, first_side_from_store_buffer);
	


}

catch (std::exception const& e) {
	std::cerr << "ERROR: " << e.what() << "\n";
}
