
#include <iostream>

//configuration
#define ASCS_SERVER_PORT		9527
#define ASCS_MAX_OBJECT_NUM		102400
#define ASCS_REUSE_OBJECT //use objects pool
#define ASCS_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
//#define ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
#define ASCS_MSG_BUFFER_SIZE	1024
#define ASCS_INPUT_QUEUE		non_lock_queue //we will never operate sending buffer concurrently, so need no locks
#define ASCS_INPUT_CONTAINER	list
#define ASCS_DECREASE_THREAD_AT_RUNTIME
//configuration

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define LIST_STATUS		"status"
#define INCREASE_THREAD	"increase_thread"
#define DECREASE_THREAD	"decrease_thread"

class echo_socket : public client_socket
{
public:
	echo_socket(asio::io_context& io_context_) : client_socket(io_context_), msg_len(ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN) {unpacker()->stripped(false);}

	void begin(size_t msg_len_) {msg_len = msg_len_;}
	void check_delay(float max_delay) {if (is_connected() && last_send_time.elapsed() > max_delay) force_shutdown();}

protected:
	virtual void on_connect()
	{
		asio::ip::tcp::no_delay option(true);
		lowest_layer().set_option(option);

		char* buff = new char[msg_len];
		memset(buff, 'Y', msg_len); //what should we send?

		last_send_time.restart();
		send_msg(buff, msg_len, true);

		delete[] buff;

		client_socket::on_connect();
	}

	//msg handling
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {handle_msg(std::move(msg)); return true;}
#endif
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(std::move(msg)); return true;}

private:
	void handle_msg(out_msg_type&& msg)
	{
		last_send_time.restart();
		direct_send_msg(std::move(msg), true);
	}

private:
	size_t msg_len;
	cpu_timer last_send_time;
};

class echo_client : public multi_client_base<echo_socket>
{
public:
	echo_client(service_pump& service_pump_) : multi_client_base<echo_socket>(service_pump_) {}

	void begin(float max_delay, size_t msg_len)
	{
		do_something_to_all([msg_len](object_ctype& item) {item->begin(msg_len);});
		set_timer(TIMER_END, 5000, [=](tid id)->bool {this->do_something_to_all([max_delay](object_ctype& item) {item->check_delay(max_delay);}); return true;});
	}
};

int main(int argc, const char* argv[])
{
	printf("usage: %s [<message size=" ASCS_SF "> [<max delay=%f (seconds)> [<service thread number=1> [<port=%d> [<ip=%s> [link num=16]]]]]]\n",
		argv[0], ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN, 1.f, ASCS_SERVER_PORT, ASCS_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	///////////////////////////////////////////////////////////
	size_t link_num = 16;
	if (argc > 6)
		link_num = std::min(ASCS_MAX_OBJECT_NUM, std::max(atoi(argv[6]), 1));

	printf("exec: echo_client with " ASCS_SF " links\n", link_num);
	///////////////////////////////////////////////////////////

	service_pump sp;
	echo_client client(sp);

//	argv[5] = "::1" //ipv6
//	argv[5] = "127.0.0.1" //ipv4
	std::string ip = argc > 5 ? argv[5] : ASCS_SERVER_IP;
	unsigned short port = argc > 4 ? atoi(argv[4]) : ASCS_SERVER_PORT;

	auto thread_num = 1;
	if (argc > 3)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[3])));
	//add one thread will seriously impact IO throughput when doing performance benchmark, this is because the business logic is very simple (send original messages back,
	//or just add up total message size), under this scenario, just one service thread without receiving buffer will obtain the best IO throughput.
	//the server has such behavior too.

	for (size_t i = 0; i < link_num; ++i)
		client.add_socket(port, ip);

	auto max_delay = 1.f;
	if (argc > 2)
		max_delay = std::max(.1f, (float) atof(argv[2]));

	auto msg_len = ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN;
	if (argc > 1)
		msg_len = std::max((size_t) 1, std::min(msg_len, (size_t) atoi(argv[1])));
	client.begin(max_delay, msg_len);

	sp.start_service(thread_num);
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (LIST_STATUS == str)
		{
			printf("link #: " ASCS_SF ", valid links: " ASCS_SF ", invalid links: " ASCS_SF "\n", client.size(), client.valid_size(), client.invalid_object_size());
			puts("");
			puts(client.get_statistic().to_string().data());
		}
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
	}

    return 0;
}
