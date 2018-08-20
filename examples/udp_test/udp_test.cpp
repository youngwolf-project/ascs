
#include <iostream>

//configuration
#define ASCS_DELAY_CLOSE 1 //this demo not used object pool and doesn't need life cycle management,
						   //so, define this to avoid hooks for async call (and slightly improve efficiency),
						   //any value which is bigger than zero is okay.
#define ASCS_MAX_SYNC_RECV	1
//#define ASCS_DEFAULT_UDP_UNPACKER replaceable_udp_unpacker<>
#define ASCS_HEARTBEAT_INTERVAL 5 //neither udp_unpacker nor replaceable_udp_unpacker support heartbeat message,
								  //so heartbeat will be treated as normal message.
#define ASCS_SYNC_SEND
//configuration

#include <ascs/ext/udp.h>
using namespace ascs;
using namespace ascs::ext::udp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

std::thread create_sync_recv_thread(single_service& service)
{
	return std::thread([&service]() {
		while (!service.is_ready())
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

		std::list<single_service::out_msg_type> msg_can;
		while (service.sync_recv_msg(msg_can))
		{
			do_something_to_all(msg_can, [](single_service::out_msg_type& msg) {printf("sync recv(" ASCS_SF ") : %s\n", msg.size(), msg.data());});
			msg_can.clear();
		}
		puts("sync recv end.");
	});
}

int main(int argc, const char* argv[])
{
	printf("usage: %s <my port> <peer port> [peer ip=127.0.0.1]\n", argv[0]);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else if (argc < 3)
		return 1;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	single_service service(sp);
	service.set_local_addr((unsigned short) atoi(argv[1])); //for multicast, do not bind to a specific IP, just port is enough
	service.set_peer_addr((unsigned short) atoi(argv[2]), argc >= 4 ? argv[3] : "127.0.0.1");

	sp.start_service();
	//for multicast, join it after start_service():
//	service.lowest_layer().set_option(asio::ip::multicast::join_group(asio::ip::address::from_string("x.x.x.x")));

	//if you must join it before start_service():
//	service.lowest_layer().open(ASCS_UDP_DEFAULT_IP_VERSION);
//	service.lowest_layer().set_option(asio::ip::multicast::join_group(asio::ip::address::from_string("x.x.x.x")));
//	sp.start_service();

	auto t = create_sync_recv_thread(service);
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

			sp.start_service();
			t = create_sync_recv_thread(service);
		}
		else
			service.sync_safe_send_native_msg(str, false); //to send to different endpoints, use overloads that take a const asio::ip::udp::endpoint& parameter
	}

	return 0;
}
