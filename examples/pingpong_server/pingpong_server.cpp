
#include <iostream>

//configuration
#define ASCS_SERVER_PORT		9527
#define ASCS_ASYNC_ACCEPT_NUM	5
#define ASCS_REUSE_OBJECT //use objects pool
//#define ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
#define ASCS_MSG_BUFFER_SIZE 65536
#define ASCS_DEFAULT_UNPACKER stream_unpacker //non-protocol
//configuration

#include <ascs/ext/server.h>
using namespace ascs;
using namespace ascs::ext;

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#define QUIT_COMMAND	"quit"
#define LIST_STATUS		"status"

class echo_socket : public server_socket
{
public:
	echo_socket(i_server& server_) : server_socket(server_) {}

protected:
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {return direct_post_msg(std::move(msg));}
#endif
	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {return direct_post_msg(std::move(msg));}
};

class echo_server : public tcp::server_base<echo_socket>
{
public:
	echo_server(service_pump& service_pump_) : server_base(service_pump_) {}

	echo_socket::statistic get_statistic()
	{
		echo_socket::statistic stat;
		do_something_to_all([&stat](const auto& item) {stat += item->get_statistic();});

		return stat;
	}

protected:
	virtual bool on_accept(object_ctype& client_ptr) {asio::ip::tcp::no_delay option(true); client_ptr->lowest_layer().set_option(option); return true;}
};

int main(int argc, const char* argv[])
{
	printf("usage: pingpong_server [<service thread number=1> [<port=%d> [ip=0.0.0.0]]]\n", ASCS_SERVER_PORT);
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
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (LIST_STATUS == str)
		{
			printf("link #: " ASCS_SF ", invalid links: " ASCS_SF "\n", echo_server_.size(), echo_server_.invalid_object_size());
			puts("");
			puts(echo_server_.get_statistic().to_string().data());
		}
	}

	return 0;
}

//restore configuration
#undef ASCS_SERVER_PORT
#undef ASCS_ASYNC_ACCEPT_NUM
#undef ASCS_REUSE_OBJECT
#undef ASCS_FREE_OBJECT_INTERVAL
#undef ASCS_DEFAULT_PACKER
#undef ASCS_DEFAULT_UNPACKER
#undef ASCS_MSG_BUFFER_SIZE
//restore configuration
