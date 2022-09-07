
#include <iostream>

//configuration
#define ASCS_SERVER_PORT		9527
#define ASCS_MAX_OBJECT_NUM		102400
#define ASCS_ASYNC_ACCEPT_NUM	1024 //pre-create 1024 server socket, this is very useful if creating server socket is very expensive
#define ASCS_REUSE_OBJECT //use objects pool
#define ASCS_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve performance)
#define ASCS_MSG_BUFFER_SIZE	1024
#define ASCS_SYNC_DISPATCH
#ifdef ASCS_SYNC_DISPATCH
	#define ASCS_INPUT_QUEUE	non_lock_queue
	#define ASCS_OUTPUT_QUEUE	non_lock_queue
#endif
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
	echo_socket(i_server& server_) : server_socket(server_) {unpacker()->stripped(false);}
	//other heavy things can be done in the constructor too, because we pre-created ASCS_ASYNC_ACCEPT_NUM echo_socket objects

protected:
	//msg handling
#ifdef ASCS_SYNC_DISPATCH
	virtual size_t on_msg(std::list<out_msg_type>& msg_can)
	{
		ascs::do_something_to_all(msg_can, [this](out_msg_type& msg) {direct_send_msg(std::move(msg));});
		msg_can.clear();

		return 1;
	}
#endif
#ifdef ASCS_DISPATCH_BATCH_MSG
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		assert(!msg_can.empty());
		out_container_type tmp_can;
		msg_can.swap(tmp_can);

		ascs::do_something_to_all(tmp_can, [this](out_msg_type& msg) {direct_send_msg(std::move(msg));});
		return 1;
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {direct_send_msg(std::move(msg), true); return true;}
#endif
	//msg handling end
};

class echo_server : public server_base<echo_socket>
{
public:
	echo_server(service_pump& service_pump_) : server_base<echo_socket>(service_pump_) {}

protected:
	virtual bool on_accept(object_ctype& socket_ptr) {asio::ip::tcp::no_delay option(true); socket_ptr->lowest_layer().set_option(option); return true;}
	virtual void start_next_accept()
	{
		//after we accepted ASCS_ASYNC_ACCEPT_NUM - 10 connections, we start to create new echo_socket objects (one echo_socket per one accepting)
		if (size() + 10 >= ASCS_ASYNC_ACCEPT_NUM) //only left 10 async accepting operations
			server_base<echo_socket>::start_next_accept();
		else
			puts("stopped one async accepting.");
	}
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
