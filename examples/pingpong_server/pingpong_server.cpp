
#include <iostream>

//configuration
#define ASCS_SERVER_PORT		9527
#define ASCS_REUSE_OBJECT //use objects pool
#define ASCS_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
#define ASCS_SYNC_DISPATCH
#define ASCS_MSG_BUFFER_SIZE	65536
#define ASCS_DEFAULT_UNPACKER stream_unpacker //non-protocol
#define ASCS_DECREASE_THREAD_AT_RUNTIME
//configuration

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

class echo_socket : public server_socket
{
public:
	echo_socket(i_server& server_) : server_socket(server_) {}

protected:
	//msg handling: send the original msg back (echo server), must define macro ASCS_SYNC_DISPATCH
	//do not hold msg_can for further usage, access msg_can and return from on_msg as quickly as possible
	//access msg_can freely within this callback, it's always thread safe.
	virtual size_t on_msg(std::list<out_msg_type>& msg_can)
	{
		//if the type of out_msg_type and in_msg_type are not identical, the compilation will fail, then you should use send_native_msg instead.
		ascs::do_something_to_all(msg_can, [this](out_msg_type& msg) {direct_send_msg(std::move(msg));});
		msg_can.clear(); //if we left behind some messages in msg_can, they will be dispatched via on_msg_handle asynchronously, which means it's
		//possible that on_msg_handle be invoked concurrently with the next on_msg (new messages arrived) and then disorder messages.
		//here we always consumed all messages, so we can use sync message dispatching, otherwise, we should not use sync message dispatching
		//except we can bear message disordering.

		return 1;
	}
	//msg handling end
};

class echo_server : public server_base<echo_socket>
{
public:
	echo_server(service_pump& service_pump_) : server_base<echo_socket>(service_pump_) {}

protected:
	virtual bool on_accept(object_ctype& socket_ptr) {asio::ip::tcp::no_delay option(true); socket_ptr->lowest_layer().set_option(option); return true;}
};

int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=1> [<port=%d> [ip=0.0.0.0]]]\n", argv[0], ASCS_SERVER_PORT);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	echo_server echo_server_(sp);

	if (argc > 3)
		echo_server_.set_server_addr(atoi(argv[2]), argv[3]);
	else if (argc > 2)
		echo_server_.set_server_addr(atoi(argv[2]));

	auto thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));

	sp.start_service(thread_num);
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (STATISTIC == str)
		{
			printf("link #: " ASCS_SF ", invalid links: " ASCS_SF "\n\n", echo_server_.size(), echo_server_.invalid_object_size());
			puts(echo_server_.get_statistic().to_string().data());
		}
		else if (STATUS == str)
			echo_server_.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			echo_server_.list_all_object();
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
	}

	return 0;
}
