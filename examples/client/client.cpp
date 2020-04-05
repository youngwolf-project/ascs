
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	9527
#define ASCS_DELAY_CLOSE	1 //this demo not used object pool and doesn't need life cycle management,
							  //so, define this to avoid hooks for async call (and slightly improve efficiency),
							  //any value which is bigger than zero is okay.
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
//#define ASCS_HUGE_MSG
//#define ASCS_MSG_BUFFER_SIZE (1024 * 1024) //should not bigger than ASCS_MAX_SEND_BUF and ASCS_MAX_RECV_BUF, please note
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
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"
#define STATISTIC		"statistic"

//we only want close reconnecting mechanism on this socket, so we don't define macro ASCS_RECONNECT
class short_connection : public client_socket
{
public:
	short_connection(i_matrix& matrix_) : client_socket(matrix_) {}

protected:
	virtual void on_connect() {close_reconnect(); client_socket::on_connect();} //close reconnecting mechanism
};

class short_client : public multi_client_base<short_connection>
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
		return socket_ptr ? socket_ptr->send_msg(std::move(msg)) : false;
	}

private:
	unsigned short port;
	std::string ip;
};

std::thread create_sync_recv_thread(single_client& client)
{
	return std::thread([&client]() {
		typename ASCS_DEFAULT_UNPACKER::container_type msg_can;
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
	single_service_pump<single_client> client;
	//singel_service_pump also is a service_pump, this let us to control client2 via client
	short_client client2(client); //without single_client, we need to define ASCS_AVOID_AUTO_STOP_SERVICE macro to forbid service_pump stopping services automatically

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	unsigned short port = ASCS_SERVER_PORT + 100;
	std::string ip = ASCS_SERVER_IP;
	if (argc > 1)
		port = (unsigned short) atoi(argv[1]);
	if (argc > 2)
		ip = argv[2];

	client.set_server_addr(port, ip);
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
