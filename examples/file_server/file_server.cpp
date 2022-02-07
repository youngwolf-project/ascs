
#include <iostream>

//configuration
#define ASCS_DEFAULT_PACKER packer2<>
//#define ASCS_RECV_BUFFER_TYPE std::vector<asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
//#define ASCS_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to ascs
//note, these two macro are not requisite, I'm just showing how to use them.

//all other definitions are in the makefile, because we have two cpp files, defining them in more than one place is risky (
// we may define them to different values between the two cpp files)
//configuration

#include "file_socket.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"

int main(int argc, const char* argv[])
{
	puts("this is a file transmission server.");
#if defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
	printf("usage: %s [<port=%d> [ip=0.0.0.0]]\n", argv[0], ASCS_SERVER_PORT);
#else
	printf("usage: %s [-d] [<port=%d> [ip=0.0.0.0]]\n", argv[0], ASCS_SERVER_PORT);
#endif

	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	auto index = 0;
	if (argc >= 2 && 0 == strcmp(argv[1], "-d"))
	{
#if defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
		puts("on windows, -d is not supported!");
		return 1;
#endif
		index = 1;
	}

	service_pump sp;
#ifndef ASCS_DECREASE_THREAD_AT_RUNTIME
	//if you want to decrease service thread at runtime, then you cannot use multiple io_context, if somebody indeed needs it, please let me know.
	//with multiple io_context, the number of service thread must be bigger than or equal to the number of io_context, please note.
	//with multiple io_context, please also define macro ASCS_AVOID_AUTO_STOP_SERVICE.
	sp.set_io_context_num(8);
#endif
	server_base<file_socket> file_server_(sp);

	if (argc > 2 + index)
		file_server_.set_server_addr(atoi(argv[1 + index]), argv[2 + index]);
	else if (argc > 1 + index)
		file_server_.set_server_addr(atoi(argv[1 + index]));

#if !defined(_MSC_VER) && !defined(__MINGW64__) && !defined(__MINGW32__)
	if (1 == index)
	{
		asio::signal_set signal_receiver(sp.assign_io_context(), SIGINT, SIGTERM);
		std::function<void(const asio::error_code&, int)> signal_handler = [&](const asio::error_code& ec, int signal_number) {
			if (!ec)
				return sp.end_service();

			signal_receiver.async_wait([&](const asio::error_code& ec, int signal_number) {signal_handler(ec, signal_number);});
		};
		signal_receiver.async_wait([&](const asio::error_code& ec, int signal_number) {signal_handler(ec, signal_number);});

		sp.run_service();
		return 0;
	}
#endif

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else if (STATISTIC == str)
		{
			printf("link #: " ASCS_SF ", invalid links: " ASCS_SF "\n\n", file_server_.size(), file_server_.invalid_object_size());
			puts(file_server_.get_statistic().to_string().data());
		}
		else if (STATUS == str)
			file_server_.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			file_server_.list_all_object();
	}

	return 0;
}
