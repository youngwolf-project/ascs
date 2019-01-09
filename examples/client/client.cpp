
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
#define ASCS_WANT_ALL_MSG_SEND_NOTIFY
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
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"

//demonstrates how to access client in client_socket (just like access server in server_socket)
class i_controller
{
public:
	virtual void on_all_msg_send(uint_fast64_t id) = 0;
	//add more interfaces if needed
};

class short_connection : public client_socket
{
public:
	short_connection(asio::io_context& io_context_) : client_socket(io_context_), controller(nullptr) {}

	void set_controller(i_controller* _controller) {controller = _controller;}
	i_controller* get_controller() const {return controller;}

protected:
	virtual void on_connect() {close_reconnect();}

#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
	virtual void on_all_msg_send(in_msg_type& msg) {controller->on_all_msg_send(id());}
#endif

private:
	i_controller* controller;
};

class short_client : public multi_client_base<short_connection>, protected i_controller
{
public:
	short_client(service_pump& service_pump_) : multi_client_base<short_connection>(service_pump_) {}

	bool send_msg(const std::string& msg)
	{
		auto socket_ptr = add_socket(ASCS_SERVER_PORT + 101, ASCS_SERVER_IP);
		//if the user changed server address via command line, here needs some changes accordingly, please refer to the other client (single_client) in main function
		if (socket_ptr)
		{
			socket_ptr->set_controller(this);
			return socket_ptr->send_msg(msg);
		}

		return false;
	}

protected:
	virtual void on_all_msg_send(uint_fast64_t id) {}
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

	service_pump sp;
	single_client client(sp);
	short_client client2(sp); //without single_client, we need to define ASCS_AVOID_AUTO_STOP_SERVICE macro to forbid service_pump stopping services automatically

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	if (argc > 2)
		client.set_server_addr(atoi(argv[1]), argv[2]);
	else if (argc > 1)
		client.set_server_addr(atoi(argv[1]), ASCS_SERVER_IP);
	else
		client.set_server_addr(ASCS_SERVER_PORT + 100, ASCS_SERVER_IP);

	sp.start_service();
	auto t = create_sync_recv_thread(client);
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
		{
			client.sync_safe_send_msg(str + " (from normal client)", 100);
			//client.safe_send_msg(str + " (from normal client)");

			client2.send_msg(str + " (from short client)");
		}
	}

	return 0;
}
