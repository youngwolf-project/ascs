
#include <iostream>

//configuration
#define ASCS_NOT_REUSE_ADDRESS
#define ASCS_SYNC_RECV
#define ASCS_SYNC_SEND
#define ASCS_PASSIVE_RECV //if you annotate this definition, this demo will use mix model to receive messages, which means
						  //some messages will be dispatched via on_msg_handle(), some messages will be returned via sync_recv_msg(),
						  //type more than one messages (separate them by space) in one line with ENTER key to send them,
						  //you will see them cross together on the receiver's screen.
						  //with this macro, if heartbeat not applied, macro ASCS_AVOID_AUTO_STOP_SERVICE must be defined to avoid the service_pump run out.
#define ASCS_AVOID_AUTO_STOP_SERVICE
#define ASCS_UDP_CONNECT_MODE true
//#define ASCS_HEARTBEAT_INTERVAL 5 //neither udp_unpacker nor udp_unpacker2 support heartbeat message, so heartbeat will be treated as normal message.
//#define ASCS_DEFAULT_UDP_UNPACKER udp_unpacker2<>
//configuration

#include <ascs/ext/reliable_udp.h>
using namespace ascs::ext::udp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

std::thread create_sync_recv_thread(single_reliable_socket_service& service)
{
	return std::thread([&]() {
		std::list<single_reliable_socket_service::out_msg_type> msg_can;
		auto re = ascs::sync_call_result::SUCCESS;
		do
		{
			re = service.sync_recv_msg(msg_can, 50); //ascs will not maintain messages in msg_can anymore after sync_recv_msg return, please note.
			if (ascs::sync_call_result::SUCCESS == re)
			{
				ascs::do_something_to_all(msg_can, [](single_reliable_socket_service::out_msg_type& msg) {printf("sync recv(" ASCS_SF ") : %s\n", msg.size(), msg.data());});
				msg_can.clear(); //sync_recv_msg just append new message(s) to msg_can, please note.
			}
		} while (ascs::sync_call_result::SUCCESS == re || ascs::sync_call_result::TIMEOUT == re);
		puts("sync recv end.");
	});
}

//because st_asio_wrapper is header only, it cannot provide the implementation of below global function, but kcp needs it,
//you're supposed to provide it and call reliable_socket_base::output directly in it, like:
int output(const char* buf, int len, ikcpcb * kcp, void* user) {return ((single_reliable_socket_service*) user)->output(buf, len);}

int main(int argc, const char* argv[])
{
	printf("usage: %s <my port> <peer port> [peer ip=127.0.0.1]\n", argv[0]);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else if (argc < 3)
		return 1;
	else
		puts("type " QUIT_COMMAND " to end.");

	ascs::service_pump sp;
	single_reliable_socket_service service(sp);
	service.set_local_addr((unsigned short) atoi(argv[1])); //for multicast, do not bind to a specific IP, just port is enough
	service.set_peer_addr((unsigned short) atoi(argv[2]), argc >= 4 ? argv[3] : "127.0.0.1");

	//reliable_socket cannot become reliable without below statement, instead, it downgrade to normal UDP socket
	service.create_kcpcb(0, (void*) &service);
	//without below statement, your application will core dump
	ikcp_setoutput(service.get_kcpcb(), &output);

	sp.start_service();
	//for broadcast
//	service.lowest_layer().set_option(asio::socket_base::broadcast(true)); //usage: ./udp_test 5000 5000 "255.255.255.255"
	//for multicast, join it after start_service():
//	service.lowest_layer().set_option(asio::ip::multicast::join_group(asio::ip::make_address("x.x.x.x"))); // >= asio 1.11

	//if you must join it before start_service():
//	service.lowest_layer().open(ASCS_UDP_DEFAULT_IP_VERSION);
//	service.lowest_layer().set_option(asio::ip::multicast::join_group(asio::ip::address::from_string("x.x.x.x"))); // < asio 1.11
//	sp.start_service();

	//demonstrate how to change local address if the binding was failed.
	if (!service.service_started())
	{
		service.set_local_addr(6666);
		sp.start_service(&service);
	}

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
			service.sync_safe_send_native_msg(str); //to send to different endpoints, use overloads that take a const asio::ip::udp::endpoint& parameter
	}

	return 0;
}
