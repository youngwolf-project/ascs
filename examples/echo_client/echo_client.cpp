
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	9527
//#define ASCS_REUSE_OBJECT //use objects pool
#define ASCS_DELAY_CLOSE	5 //define this to avoid hooks for async call (and slightly improve efficiency)
//#define ASCS_CLEAR_OBJECT_INTERVAL 1
#define ASCS_SYNC_DISPATCH
#define ASCS_DISPATCH_BATCH_MSG
//#define ASCS_WANT_MSG_SEND_NOTIFY
//#define ASCS_FULL_STATISTIC //full statistic will slightly impact efficiency
#define ASCS_AVOID_AUTO_STOP_SERVICE
#define ASCS_DECREASE_THREAD_AT_RUNTIME
//#define ASCS_MAX_SEND_BUF	65536
//#define ASCS_MAX_RECV_BUF	65536
//if there's a huge number of links, please reduce messge buffer via ASCS_MAX_SEND_BUF and ASCS_MAX_RECV_BUF macro.
//please think about if we have 512 links, how much memory we can accupy at most with default ASCS_MAX_SEND_BUF and ASCS_MAX_RECV_BUF?
//it's 2 * 1M * 512 = 1G

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	0
//0-default packer and unpacker, head(length) + body
//1-packer2 and unpacker2, head(length) + body
//2-fixed length packer and unpacker
//3-prefix and/or suffix packer and unpacker

#if 0 == PACKER_UNPACKER_TYPE
#define ASCS_HUGE_MSG
#define ASCS_MSG_BUFFER_SIZE 1000000
#define ASCS_MAX_SEND_BUF (10 * ASCS_MSG_BUFFER_SIZE)
#define ASCS_MAX_RECV_BUF (10 * ASCS_MSG_BUFFER_SIZE)
#define ASCS_DEFAULT_UNPACKER flexible_unpacker<std::string>
//this unpacker only pre-allocated a buffer of 4000 bytes, but it can parse messages up to ST_ASIO_MSG_BUFFER_SIZE (here is 1000000) bytes,
//it works as the default unpacker for messages <= 4000, otherwise, it works as non_copy_unpacker
#elif 1 == PACKER_UNPACKER_TYPE
#define ASCS_DEFAULT_PACKER packer2<unique_buffer<std::string>, std::string>
#define ASCS_DEFAULT_UNPACKER unpacker2<unique_buffer<std::string>, std::string, flexible_unpacker<std::string>>
#elif 2 == PACKER_UNPACKER_TYPE
#undef ASCS_HEARTBEAT_INTERVAL
#define ASCS_HEARTBEAT_INTERVAL	0 //not support heartbeat
#define ASCS_DEFAULT_PACKER fixed_length_packer
#define ASCS_DEFAULT_UNPACKER fixed_length_unpacker
#elif 3 == PACKER_UNPACKER_TYPE
#define ASCS_DEFAULT_PACKER prefix_suffix_packer
#define ASCS_DEFAULT_UNPACKER prefix_suffix_unpacker
#endif

#if defined(ASCS_WANT_MSG_SEND_NOTIFY) && (!defined(ASCS_HEARTBEAT_INTERVAL) || ASCS_HEARTBEAT_INTERVAL <= 0)
#define ASCS_INPUT_QUEUE non_lock_queue //we will never operate sending buffer concurrently, so need no locks
#endif
//configuration

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

static bool check_msg;

///////////////////////////////////////////////////
//msg sending interface
#define TCP_RANDOM_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
{ \
	auto index = (size_t) ((uint64_t) rand() * (size() - 1) / RAND_MAX); \
	auto socket_ptr = at(index); \
	return socket_ptr ? socket_ptr->SEND_FUNNAME(pstr, len, num, can_overflow, prior) : false; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//msg sending interface
///////////////////////////////////////////////////

class echo_socket : public client_socket
{
public:
	echo_socket(i_matrix& matrix_) : client_socket(matrix_), recv_bytes(0), recv_index(0)
	{
#if 3 == PACKER_UNPACKER_TYPE
		std::dynamic_pointer_cast<ASCS_DEFAULT_PACKER>(packer())->prefix_suffix("begin", "end");
		std::dynamic_pointer_cast<ASCS_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("begin", "end");
#endif
	}

	uint64_t get_recv_bytes() const {return recv_bytes;}
	operator uint64_t() const {return recv_bytes;}

	void clear_status() {recv_bytes = recv_index = 0;}
	void begin(size_t msg_num_, size_t msg_len, char msg_fill)
	{
		clear_status();
		msg_num = msg_num_;

		std::string msg(msg_len, msg_fill);
		msg.replace(0, sizeof(size_t), (const char*) &recv_index, sizeof(size_t)); //seq

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
		send_msg(msg); //can not apply new feature introduced by version 1.4.0, we need a whole message in on_msg_send
#else
		send_msg(std::move(msg)); //new feature introduced by version 1.4.0, avoid one memory replication
#endif
	}

protected:
	//msg handling
#ifdef ASCS_SYNC_DISPATCH
	//do not hold msg_can for further using, return from on_msg as quickly as possible
	//access msg_can freely within this callback, it's always thread safe.
	virtual size_t on_msg(std::list<out_msg_type>& msg_can)
	{
		ascs::do_something_to_all(msg_can, [this](out_msg_type& msg) {this->handle_msg(msg);});
		msg_can.clear(); //if we left behind some messages in msg_can, they will be dispatched via on_msg_handle asynchronously, which means it's
		//possible that on_msg_handle be invoked concurrently with the next on_msg (new messages arrived) and then disorder messages.
		//here we always consumed all messages, so we can use sync message dispatching, otherwise, we should not use sync message dispatching
		//except we can bear message disordering.

		return 1;
		//if we indeed handled some messages, do return 1
		//if we handled nothing, return 1 is also okey but will very slightly impact performance (if msg_can is not empty), return 0 is suggested
	}
#endif
#ifdef ASCS_DISPATCH_BATCH_MSG
	//do not hold msg_can for further using, access msg_can and return from on_msg_handle as quickly as possible
	//can only access msg_can via functions that marked as 'thread safe', if you used non-lock queue, its your responsibility to guarantee
	// that new messages will not come until we returned from this callback (for example, pingpong test).
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		out_container_type tmp_can;
		msg_can.swap(tmp_can); //to consume a part of the messages in msg_can, see echo_server

		ascs::do_something_to_all(tmp_can, [this](out_msg_type& msg) {this->handle_msg(msg);});
		return 1;
		//if we indeed handled some messages, do return 1, else, return 0
		//if we handled nothing, but want to re-dispatch messages immediately, return 1
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//msg handling end

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg)
	{
		if (0 == --msg_num)
			return;

		auto pstr = packer()->raw_data(msg);
		auto msg_len = packer()->raw_data_len(msg);

		size_t send_index;
		memcpy(&send_index, pstr, sizeof(size_t));
		++send_index;
		memcpy(pstr, &send_index, sizeof(size_t)); //seq

		send_msg(pstr, msg_len, true);
	}
#endif

private:
	void handle_msg(out_msg_ctype& msg)
	{
		recv_bytes += msg.size();
		if (check_msg && (msg.size() < sizeof(size_t) || 0 != memcmp(&recv_index, msg.data(), sizeof(size_t))))
			printf("check msg error: " ASCS_LLF "->" ASCS_SF "/" ASCS_SF ".\n", id(), recv_index, *(size_t*) msg.data());
		++recv_index;

		//i'm the bottleneck -_-
//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

private:
	uint64_t recv_bytes;
	size_t recv_index, msg_num;
};

class echo_client : public multi_client_base<echo_socket>
{
public:
	echo_client(service_pump& service_pump_) : multi_client_base<echo_socket>(service_pump_) {}

	uint64_t get_recv_bytes()
	{
		uint64_t total_recv_bytes = 0;
		do_something_to_all([&total_recv_bytes](object_ctype& item) {total_recv_bytes += *item;});
//		do_something_to_all([&total_recv_bytes](object_ctype& item) {total_recv_bytes += item->get_recv_bytes();});

		return total_recv_bytes;
	}

	void clear_status() {do_something_to_all([](object_ctype& item) {item->clear_status();});}
	void begin(size_t msg_num, size_t msg_len, char msg_fill) {do_something_to_all([&](object_ctype& item) {item->begin(msg_num, msg_len, msg_fill);});}

	void shutdown_some_client(size_t n)
	{
		static auto index = -1;
		++index;

		switch (index % 6)
		{
#ifdef ASCS_CLEAR_OBJECT_INTERVAL
			//method #1
			//notice: these methods need to define ASCS_CLEAR_OBJECT_INTERVAL macro, because it just shut down the socket,
			//not really remove them from object pool, this will cause echo_client still send data via them, and wait responses from them.
			//for this scenario, the smaller ASCS_CLEAR_OBJECT_INTERVAL macro is, the better experience you will get, so set it to 1 second.
		case 0: do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->graceful_shutdown(), false : true;});				break;
		case 1: do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->graceful_shutdown(false, false), false : true;});	break;
		case 2: do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->force_shutdown(), false : true;});					break;
#else
			//method #2
			//this is a equivalence of calling i_server::del_socket in server_socket_base::on_recv_error (see server_socket_base for more details).
		case 0: while (n-- > 0) graceful_shutdown(at(0));			break;
		case 1: while (n-- > 0) graceful_shutdown(at(0), false);	break;
		case 2: while (n-- > 0) force_shutdown(at(0));				break;
#endif
			//if you just want to reconnect to the server, you should do it like this:
		case 3: do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->graceful_shutdown(true), false : true;});			break;
		case 4: do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->graceful_shutdown(true, false), false : true;});	break;
		case 5: do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->force_shutdown(true), false : true;});				break;
		}
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_RANDOM_SEND_MSG(random_send_msg, send_msg)
	TCP_RANDOM_SEND_MSG(random_send_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow is false, success at here just means putting the msg into tcp::socket's send buffer successfully
	TCP_RANDOM_SEND_MSG(safe_random_send_msg, safe_send_msg)
	TCP_RANDOM_SEND_MSG(safe_random_send_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////
};

void send_msg_one_by_one(echo_client& client, size_t msg_num, size_t msg_len, char msg_fill)
{
	check_msg = true;
	uint64_t total_msg_bytes = msg_num * msg_len * client.size();

	cpu_timer begin_time;
	client.begin(msg_num, msg_len, msg_fill);
	unsigned percent = 0;
	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto new_percent = (unsigned) (100 * client.get_recv_bytes() / total_msg_bytes);
		if (percent != new_percent)
		{
			percent = new_percent;
			printf("\r%u%%", percent);
			fflush(stdout);
		}
	} while (percent < 100);
	begin_time.stop();

	printf(" finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n",
		begin_time.elapsed(), client.size() * msg_num / begin_time.elapsed(), total_msg_bytes / begin_time.elapsed() / 1024 / 1024);
}

void send_msg_randomly(echo_client& client, size_t msg_num, size_t msg_len, char msg_fill)
{
	check_msg = false;
	uint64_t send_bytes = 0;
	uint64_t total_msg_bytes = msg_num * msg_len;
	auto buff = new char[msg_len];
	memset(buff, msg_fill, msg_len);

	cpu_timer begin_time;
	unsigned percent = 0;
	for (size_t i = 0; i < msg_num; ++i)
	{
		memcpy(buff, &i, sizeof(size_t)); //seq

		client.safe_random_send_msg((const char*) buff, msg_len); //can_overflow is false, it's important
		send_bytes += msg_len;

		auto new_percent = (unsigned) (100 * send_bytes / total_msg_bytes);
		if (percent != new_percent)
		{
			percent = new_percent;
			printf("\r%u%%", percent);
			fflush(stdout);
		}
	}

	while(client.get_recv_bytes() < total_msg_bytes)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

	begin_time.stop();
	delete[] buff;

	printf(" finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n",
		begin_time.elapsed(), msg_num / begin_time.elapsed(), total_msg_bytes / begin_time.elapsed() / 1024 / 1024);
}

//use up to a specific worker threads to send messages concurrently
void send_msg_concurrently(echo_client& client, size_t send_thread_num, size_t msg_num, size_t msg_len, char msg_fill)
{
	check_msg = true;
	auto link_num = client.size();
	auto group_num = std::min(send_thread_num, link_num);
	auto group_link_num = link_num / group_num;
	auto left_link_num = link_num - group_num * group_link_num;
	uint64_t total_msg_bytes = link_num * msg_len;
	total_msg_bytes *= msg_num;

	auto group_index = (size_t) -1;
	size_t this_group_link_num = 0;

	std::vector<std::list<echo_client::object_type>> link_groups(group_num);
	client.do_something_to_all([&](echo_client::object_ctype& item) {
		if (0 == this_group_link_num)
		{
			this_group_link_num = group_link_num;
			if (left_link_num > 0)
			{
				++this_group_link_num;
				--left_link_num;
			}

			++group_index;
		}

		--this_group_link_num;
		link_groups[group_index].emplace_back(item);
	});

	cpu_timer begin_time;
	std::list<std::thread> threads;
	do_something_to_all(link_groups, [&threads, &msg_num, &msg_len, &msg_fill](const std::list<echo_client::object_type>& item) {
		threads.emplace_back([&item, msg_num, msg_len, msg_fill]() {
			auto buff = new char[msg_len];
			memset(buff, msg_fill, msg_len);
			for (size_t i = 0; i < msg_num; ++i)
			{
				memcpy(buff, &i, sizeof(size_t)); //seq
				do_something_to_all(item, [&buff, &msg_len](echo_client::object_ctype& item2) {item2->safe_send_msg(buff, msg_len, false);}); //can_overflow is false, it's important
			}
			delete[] buff;
		});
	});

	unsigned percent = 0;
	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto new_percent = (unsigned) (100 * client.get_recv_bytes() / total_msg_bytes);
		if (percent != new_percent)
		{
			percent = new_percent;
			printf("\r%u%%", percent);
			fflush(stdout);
		}
	} while (percent < 100);
	do_something_to_all(threads, [](std::thread& t) {if (t.joinable()) t.join();});
	begin_time.stop();

	printf(" finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n",
		begin_time.elapsed(), link_num * msg_num / begin_time.elapsed(), total_msg_bytes / begin_time.elapsed() / 1024 / 1024);
}

static bool is_testing;
void start_test(int repeat_times, char mode, echo_client& client, size_t send_thread_num, size_t msg_num, size_t msg_len, char msg_fill)
{
	for (int i = 0; i < repeat_times; ++i)
	{
		printf("this is the %d / %d test...\n", i + 1, repeat_times);
		client.clear_status();
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
		if (0 == mode)
			send_msg_one_by_one(client, msg_num, msg_len, msg_fill);
		else
		{
			puts("if ASCS_WANT_MSG_SEND_NOTIFY defined, only support mode 0!");
			break;
		}
#else
		if (0 == mode)
			send_msg_concurrently(client, send_thread_num, msg_num, msg_len, msg_fill);
		else
			send_msg_randomly(client, msg_num, msg_len, msg_fill);
#endif
	}

	is_testing = false;
}

int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=1> [<send thread number=8> [<port=%d> [<ip=%s> [link num=16]]]]]\n", argv[0], ASCS_SERVER_PORT, ASCS_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	///////////////////////////////////////////////////////////
	size_t link_num = 16;
	if (argc > 5)
		link_num = std::min(ASCS_MAX_OBJECT_NUM, std::max(atoi(argv[5]), 1));

	printf("exec: %s with " ASCS_SF " links\n", argv[0], link_num);
	///////////////////////////////////////////////////////////

	service_pump sp;
	echo_client client(sp);
	//echo client means to cooperate with echo server while doing performance test, it will not send msgs back as echo server does,
	//otherwise, dead loop will occur, network resource will be exhausted.

//	argv[4] = "::1" //ipv6
//	argv[4] = "127.0.0.1" //ipv4
	std::string ip = argc > 4 ? argv[4] : ASCS_SERVER_IP;
	unsigned short port = argc > 3 ? atoi(argv[3]) : ASCS_SERVER_PORT;

	//method #1, create and add clients manually.
	auto socket_ptr = client.create_object();
	//socket_ptr->set_server_addr(port, ip); //we don't have to set server address at here, the following do_something_to_all will do it for us
	//some other initializations according to your business
	client.add_socket(socket_ptr);
	socket_ptr.reset(); //important, otherwise, object_pool will not be able to free or reuse this object.

	//method #2, add clients first without any arguments, then set the server address.
	for (size_t i = 1; i < link_num / 2; ++i)
		client.add_socket();
	client.do_something_to_all([&port, &ip](echo_client::object_ctype& item) {item->set_server_addr(port, ip);});

	//method #3, add clients and set server address in one invocation.
	for (auto i = std::max((size_t) 1, link_num / 2); i < link_num; ++i)
		client.add_socket(port, ip);

	size_t send_thread_num = 8;
	if (argc > 2)
		send_thread_num = (size_t) std::max(1, std::min(16, atoi(argv[2])));

	auto thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));
	//add one thread will seriously impact IO throughput when doing performance benchmark, this is because the business logic is very simple (send original messages back,
	//or just add up total message size), under this scenario, just one service thread without receiving buffer will obtain the best IO throughput.
	//the server has such behavior too.

	sp.start_service(thread_num);
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (STATISTIC == str)
		{
			printf("link #: " ASCS_SF ", valid links: " ASCS_SF ", invalid links: " ASCS_SF "\n\n", client.size(), client.valid_size(), client.invalid_object_size());
			static statistic last_stat;
			statistic this_stat = client.get_statistic();
			puts((this_stat - last_stat).to_string().data());
			last_stat = this_stat;
		}
		else if (STATUS == str)
			client.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			client.list_all_object();
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
		else if (is_testing)
			puts("testing has not finished yet!");
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service(thread_num);
		}
		else
		{
			if ('+' == str[0] || '-' == str[0])
			{
				auto n = (size_t) atoi(std::next(str.data()));
				if (0 == n)
					n = 1;

				if ('+' == str[0])
					for (; n > 0 && client.add_socket(port, ip); --n);
				else
				{
					if (n > client.size())
						n = client.size();

					client.shutdown_some_client(n);
				}

				link_num = client.size();
				continue;
			}

#ifdef ASCS_CLEAR_OBJECT_INTERVAL
			link_num = client.size();
			if (link_num != client.valid_size())
			{
				puts("please wait for a while, because object_pool has not cleaned up invalid links.");
				continue;
			}
#endif
			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			auto msg_fill = '0';
			char mode = 0; //0 broadcast, 1 randomly pick one link per msg
			auto repeat_times = 1;

			auto parameters = split_string(str);
			auto iter = std::begin(parameters);
			if (iter != std::end(parameters)) msg_num = std::max((size_t) atoll(iter++->data()), (size_t) 1);

#if 0 == PACKER_UNPACKER_TYPE || 1 == PACKER_UNPACKER_TYPE
			if (iter != std::end(parameters)) msg_len = std::min(packer<>::get_max_msg_size(),
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
#elif 2 == PACKER_UNPACKER_TYPE
			if (iter != std::end(parameters)) ++iter;
			msg_len = 1024; //we hard code this because the default fixed length is 1024, and we not changed it
#elif 3 == PACKER_UNPACKER_TYPE
			if (iter != std::end(parameters)) msg_len = std::min((size_t) ASCS_MSG_BUFFER_SIZE,
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
#endif
			if (iter != std::end(parameters)) msg_fill = *iter++->data();
			if (iter != std::end(parameters)) mode = *iter++->data() - '0';
			if (iter != std::end(parameters)) repeat_times = std::max(atoi(iter++->data()), 1);

			if (0 != mode && 1 != mode)
				puts("unrecognized mode!");
			else
			{
				printf("test parameters after adjustment: " ASCS_SF " " ASCS_SF " %c %d\n", msg_num, msg_len, msg_fill, mode);
				puts("performance test begin, this application will have no response during the test!");

				is_testing = true;
				std::thread([=, &client]() {start_test(repeat_times, mode, client, send_thread_num, msg_num, msg_len, msg_fill);}).detach();
			}
		}
	}

    return 0;
}
