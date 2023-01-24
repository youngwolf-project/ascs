
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	9527
#define ASCS_SYNC_SEND
#define ASCS_SYNC_RECV
//#define ASCS_PASSIVE_RECV //because we not defined this macro, this demo will use mix model to receive messages, which means
							//some messages will be dispatched via on_msg_handle(), some messages will be returned via sync_recv_msg(),
							//if the server send messages quickly enough, you will see them cross together.
#define ASCS_ALIGNED_TIMER
#define ASCS_CUSTOM_LOG
#define ASCS_DEFAULT_UNPACKER	non_copy_unpacker
//#define ASCS_DEFAULT_UNPACKER	stream_unpacker

//the following two macros demonstrate how to support huge msg(exceed 65535 - 2).
#define ASCS_HUGE_MSG
#define ASCS_MSG_BUFFER_SIZE	1000000
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
#include <ascs/ext/callbacks.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;
using namespace ascs::ext::tcp::proxy;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"
#define STATISTIC		"statistic"

//we only want close reconnecting mechanism on these sockets, so it cannot be done by defining macro ASCS_RECONNECT to false
///*
//method 1
class short_client : public multi_client_base<callbacks::c_socket<socks4::client_socket>>
{
public:
	short_client(service_pump& service_pump_) : multi_client_base(service_pump_) {set_server_addr(ASCS_SERVER_PORT);}

	void set_server_addr(unsigned short _port, const std::string& _ip = ASCS_SERVER_IP) {port = _port; ip = _ip;}
	bool send_msg(const std::string& msg) {return send_msg(std::string(msg), port, ip);}
	bool send_msg(std::string&& msg) {return send_msg(std::move(msg), port, ip);}
	bool send_msg(const std::string& msg, unsigned short port, const std::string& ip) {return send_msg(std::string(msg), port, ip);}
	bool send_msg(std::string&& msg, unsigned short port, const std::string& ip)
	{
		auto socket_ptr = add_socket(port, ip);
		if (!socket_ptr)
			return false;

		//register event callback from outside of the socket, it also can be done from inside of the socket, see echo_server for more details
		socket_ptr->register_on_connect([](socks4::client_socket* socket) {socket->set_reconnect(false);}, true); //close reconnection mechanism

		//without following setting, socks4::client_socket will be downgraded to normal client_socket
		//socket_ptr->set_target_addr(9527, "172.27.0.14"); //target server address, original server address becomes SOCK4 server address
		return socket_ptr->send_msg(std::move(msg));
	}

private:
	unsigned short port;
	std::string ip;
};
//*/
//method 2
/*
class short_client : public multi_client_base<socks4::client_socket, callbacks::object_pool<object_pool<socks4::client_socket>>>
{
public:
	short_client(service_pump& service_pump_) : multi_client_base(service_pump_) {set_server_addr(ASCS_SERVER_PORT);}

	void set_server_addr(unsigned short _port, const std::string& _ip = ASCS_SERVER_IP) {port = _port; ip = _ip;}
	bool send_msg(const std::string& msg) {return send_msg(std::string(msg), port, ip);}
	bool send_msg(std::string&& msg) {return send_msg(std::move(msg), port, ip);}
	bool send_msg(const std::string& msg, unsigned short port, const std::string& ip) {return send_msg(std::string(msg), port, ip);}
	bool send_msg(std::string&& msg, unsigned short port, const std::string& ip)
	{
		auto socket_ptr = add_socket(port, ip);
		if (!socket_ptr)
			return false;

		//we now can not call register_on_connect on socket_ptr, since socket_ptr is not wrapped by callbacks::c_socket
		//register event callback from outside of the socket, it also can be done from inside of the socket, see echo_server for more details
		//socket_ptr->register_on_connect([](socks4::client_socket* socket) {socket->set_reconnect(false);}, true); //close reconnection mechanism

		//without following setting, socks4::client_socket will be downgraded to normal client_socket
		//socket_ptr->set_target_addr(9527, "172.27.0.14"); //target server address, original server address becomes SOCK4 server address
		return socket_ptr->send_msg(std::move(msg));
	}

private:
	unsigned short port;
	std::string ip;
};
void on_create(object_pool<socks4::client_socket>* op, object_pool<socks4::client_socket>::object_ctype& socket_ptr) {socket_ptr->set_reconnect(false);}
*/

std::thread create_sync_recv_thread(client_socket& client)
{
	return std::thread([&]() {
		ASCS_DEFAULT_UNPACKER::container_type msg_can;
		sync_call_result re = sync_call_result::SUCCESS;
		do
		{
			re = client.sync_recv_msg(msg_can, 50); //ascs will not maintain messages in msg_can anymore after sync_recv_msg return, please note.
			if (sync_call_result::SUCCESS == re)
			{
				do_something_to_all(msg_can, [](single_client::out_msg_type& msg) {printf("sync recv(" ASCS_SF ") : %s\n", msg.size(), msg.data());});
				msg_can.clear(); //sync_recv_msg just append new message(s) to msg_can, please note.
			}
		} while (sync_call_result::SUCCESS == re || sync_call_result::TIMEOUT == re);
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

	//demonstrate how to use single_service_pump
	single_service_pump<socks5::single_client> client;
	//singel_service_pump also is a service_pump, this let us to control client2 via client
	short_client client2(client); //without single_client, we need to define ASCS_AVOID_AUTO_STOP_SERVICE macro to forbid service_pump stopping services automatically
	//method 2
	/*
	//close reconnection mechanism, 3 approaches
	client2.register_on_create([](object_pool<socks4::client_socket>*, object_pool<socks4::client_socket>::object_ctype& socket_ptr) {
		socket_ptr->set_reconnect(false);
	});
	client2.register_on_create(std::bind(on_create, std::placeholders::_1, std::placeholders::_2));
	client2.register_on_create(on_create);
	*/
	//method 2

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	unsigned short port = ASCS_SERVER_PORT + 100;
	std::string ip = ASCS_SERVER_IP;
	if (argc > 1)
		port = (unsigned short) atoi(argv[1]);
	if (argc > 2)
		ip = argv[2];

	client.set_server_addr(port, ip);
	//without following setting, socks5::single_client will be downgraded to normal single_client
	//client.set_target_addr(9527, "172.27.0.14"); //target server address, original server address becomes SOCK5 server address
	//client.set_auth("ascs", "ascs"); //can be omitted if the SOCKS5 server support non-auth
	client2.set_server_addr(port + 100, ip);

	client.start_service();
	auto t = create_sync_recv_thread(client);
	while(client.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
		{
			client.stop_service();
			t.join();
		}
		else if (RESTART_COMMAND == str)
		{
			client.stop_service();
			t.join();

			t = create_sync_recv_thread(client);
			client.start_service();
		}
		else if (RECONNECT == str)
			client.graceful_shutdown(true);
		else if (STATISTIC == str)
			puts(client.get_statistic().to_string().data());
		else
		{
			std::string tmp_str = str + " (from short client)"; //make a backup of str first, because it will be moved into client

			//each of following 4 tests is exclusive from other 3, because str will be moved into client (to reduce one memory replication)
			//to avoid this, call other overloads that accept const references.
			//we also have another 4 tests that send native messages (if the server used stream_unpacker) not listed, you can try to complete them.
			//test #1
			client.sync_safe_send_msg(std::move(str), std::string(" (from normal client)"), 100); //new feature introduced in 1.4.0
			//test #2
			//client.sync_send_msg(std::move(str), std::string(" (from normal client)"), 100); //new feature introduced in 1.4.0
			//test #3
			//client.safe_send_msg(std::move(str),  std::string(" (from normal client)")); //new feature introduced in 1.4.0
			//test #4
			//client.send_msg(std::move(str), std::string(" (from normal client)")); //new feature introduced in 1.4.0

			client2.send_msg(tmp_str);
		}
	}

	return 0;
}
