
#include <iostream>

//configuration
#define ASCS_SERVER_PORT		9527
#define ASCS_MAX_OBJECT_NUM		102400
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
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

class echo_socket : public client_socket
{
public:
	echo_socket(i_matrix& matrix_) : client_socket(matrix_), max_delay(1.f), msg_len(ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN) {unpacker()->stripped(false);}
	void begin(float max_delay_, size_t msg_len_) {max_delay = max_delay_; msg_len = msg_len_;}

protected:
	bool check_delay(bool restart_timer)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (is_connected() && last_send_time.elapsed() > max_delay)
		{
			force_shutdown();
			return false;
		}
		else if (restart_timer)
			last_send_time.restart();

		return true;
	}

	virtual void on_connect()
	{
		asio::ip::tcp::no_delay option(true);
		lowest_layer().set_option(option);

		char* buff = new char[msg_len];
		memset(buff, '$', msg_len); //what should we send?

		last_send_time.restart();
		send_msg(buff, msg_len, true);

		delete[] buff;
		set_timer(TIMER_END, 5000, ASCS_COPY_ALL_AND_THIS(tid id)->bool {return this->check_delay(false);});

		client_socket::on_connect();
	}

	//msg handling
#ifdef ASCS_SYNC_DISPATCH
	virtual size_t on_msg(std::list<out_msg_type>& msg_can)
	{
		ascs::do_something_to_all(msg_can, [this](out_msg_type& msg) {handle_msg(msg);});
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

		ascs::do_something_to_all(tmp_can, [this](out_msg_type& msg) {handle_msg(msg);});
		return 1;
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//msg handling end

private:
	void handle_msg(out_msg_type& msg) {if (check_delay(true)) direct_send_msg(std::move(msg), true);}

private:
	float max_delay;
	size_t msg_len;

	cpu_timer last_send_time;
	std::mutex mutex;
};

class echo_client : public multi_client_base<echo_socket>
{
public:
	echo_client(service_pump& service_pump_) : multi_client_base<echo_socket>(service_pump_) {}
	void begin(float max_delay, size_t msg_len) {do_something_to_all([&](object_ctype& item) {item->begin(max_delay, msg_len);});}
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

	printf("exec: concurrent_client with " ASCS_SF " links\n", link_num);
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
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (STATISTIC == str)
		{
			printf("link #: " ASCS_SF ", valid links: " ASCS_SF ", invalid links: " ASCS_SF "\n\n", client.size(), client.valid_size(), client.invalid_object_size());
			puts(client.get_statistic().to_string().data());
		}
		else if (STATUS == str)
			client.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			client.list_all_object();
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
	}

    return 0;
}
