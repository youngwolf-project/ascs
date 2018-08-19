
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	9527
#define ASCS_DELAY_CLOSE	1 //this demo not used object pool and doesn't need life cycle management,
							  //so, define this to avoid hooks for async call (and slightly improve efficiency),
							  //any value which is bigger than zero is okay.
#define ASCS_PASSIVE_RECV
#define ASCS_DISPATCH_BATCH_MSG
#define ASCS_ALIGNED_TIMER
#define ASCS_CUSTOM_LOG
#define ASCS_DEFAULT_UNPACKER	non_copy_unpacker
//#define ASCS_DEFAULT_UNPACKER	stream_unpacker

//the following three macros demonstrate how to support huge msg(exceed 65535 - 2).
//huge msg will consume huge memory, for example, if we want to support 1M msg size, because every tcp::socket has a
//private unpacker which has a fixed buffer with at lest 1M size, so just for unpackers, 1K tcp::socket will consume 1G memory.
//if we consider the send buffer and recv buffer, the buffer's default max size is 1K, so, every tcp::socket
//can consume 2G(2 * 1M * 1K) memory when performance testing(both send buffer and recv buffer are full).
//generally speaking, if there are 1K clients connected to the server, the server can consume
//1G(occupied by unpackers) + 2G(occupied by msg buffer) * 1K = 2049G memory theoretically.
//please note that the server also need to define at least ASCS_HUGE_MSG and ASCS_MSG_BUFFER_SIZE macros too.

//#define ASCS_HUGE_MSG
//#define ASCS_MSG_BUFFER_SIZE (1024 * 1024)
//#define ASCS_MAX_MSG_NUM 8 //reduce msg buffer size to reduce memory occupation
#define ASCS_HEARTBEAT_INTERVAL	5 //if use stream_unpacker, heartbeat messages will be treated as normal messages,
								  //because stream_unpacker doesn't support heartbeat
//configuration

//demonstrate how to use custom log system:
//use your own code to replace the following all_out_helper2 macros, then you can record logs according to your wishes.
//custom log should be defined(or included) before including any ascs header files except base.h
//notice: please don't forget to define the ASCS_CUSTOM_LOG macro.
#include <ascs/base.h>
using namespace ascs;

class unified_out
{
public:
	static void fatal_out(const char* fmt, ...) {all_out_helper2("fatal");}
	static void error_out(const char* fmt, ...) {all_out_helper2("error");}
	static void warning_out(const char* fmt, ...) {all_out_helper2("warning");}
	static void info_out(const char* fmt, ...) {all_out_helper2("info");}
	static void debug_out(const char* fmt, ...) {all_out_helper2("debug");}
};

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"

std::thread create_sync_recv_thread(single_client& client)
{
	return std::thread([&client]() {
		while (!client.is_connected())
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

		typename ASCS_DEFAULT_UNPACKER::container_type msg_can;
		while (client.sync_recv_msg(msg_can))
		{
			do_something_to_all(msg_can, [](single_client::out_msg_type& msg) {printf("sync recv(" ASCS_SF ") : %s\n", msg.size(), msg.data());});
			msg_can.clear();
		}
		puts("sync recv end.");
	});
}

int main(int argc, const char* argv[])
{
	printf("usage: %s [<port=%d> [ip=%s]]\n", argv[0], ASCS_SERVER_PORT + 100, ASCS_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	single_client client(sp);

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	if (argc > 2)
		client.set_server_addr(atoi(argv[1]), argv[2]);
	else if (argc > 1)
		client.set_server_addr(atoi(argv[1]), ASCS_SERVER_IP);
	else
		client.set_server_addr(ASCS_SERVER_PORT + 100, ASCS_SERVER_IP);

	auto t = create_sync_recv_thread(client);
	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
		{
			sp.stop_service();
			t.join();
		}
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			t.join();

			t = create_sync_recv_thread(client);
			sp.start_service();
		}
		else if (RECONNECT == str)
			client.graceful_shutdown(true);
		else
			client.safe_send_msg(str, false);
	}

	return 0;
}
