
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	9527
#define ASCS_REUSE_OBJECT	//use objects pool
#define ASCS_MAX_OBJECT_NUM	1024
//configuration

#include <ascs/ext/tcp.h>
#include <ascs/ext/udp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT_COMMAND	"quit"
#define STATUS			"status"

template<typename Object>
class timed_object_pool : public object_pool<Object>
{
private:
	typedef object_pool<Object> super;
	typedef typename super::tid tid; //old gcc needs this

protected:
	timed_object_pool(service_pump& service_pump_) : super(service_pump_) {}

	void start()
	{
		super::start();

		this->set_timer(super::TIMER_END, 1000 * 60, [this](tid id)->bool {
			auto now = time(nullptr);
			this->do_something_to_all([&now](typename super::object_ctype& object_ptr) {
				if (object_ptr->get_statistic().last_recv_time + 10 * 60 < now)
					object_ptr->force_shutdown();
			});

			return true;
		});
	}
};

class echo_socket : public server_socket
{
public:
	echo_socket(i_server& server_) : server_socket(server_) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(std::move(msg));}
	//msg handling end
};

class echo_stream_socket : public server_socket_base<dummy_packer<std::string>, stream_unpacker>
{
public:
	echo_stream_socket(i_server& server_) : server_socket_base<dummy_packer<std::string>, stream_unpacker>(server_) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return send_native_msg(std::move(msg));}
	//msg handling end
};

class single_udp_service : public ascs::ext::udp::single_socket_service
{
public:
	single_udp_service(service_pump& service_pump_) : ascs::ext::udp::single_socket_service(service_pump_) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return direct_send_msg(std::move(msg));} //packer and unpacker have the same type of message
	//virtual bool on_msg_handle(out_msg_type& msg) {return send_native_msg(msg.peer_addr, std::move(msg));} //packer and unpacker have different types of message
	//msg handling end
};

int main(int argc, const char* argv[])
{
	auto daemon = false;
	if (argc >= 2 && 0 == strcmp(argv[1], "-d"))
	{
#if defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
		puts("on windows, -d is not supported!");
		return 1;
#endif

		//setbuf(stdout, nullptr);
		setvbuf(stdout, nullptr, _IOLBF, 0);
		daemon = true;
	}

	puts("echo server with length (2 bytes big endian) + body protocol: 9527\n"
		"echo server with non-protocol: 9528\n"
		"echo server udp: 9528\n"
		"type " QUIT_COMMAND " to end.");

	service_pump sp;
	server_base<echo_socket, timed_object_pool<echo_socket>> echo_server(sp);
	server_base<echo_stream_socket, timed_object_pool<echo_stream_socket>> echo_stream_server(sp);
	single_udp_service udp_service(sp);

	echo_stream_server.set_server_addr(9528);
	udp_service.set_local_addr(9528);

#if !defined(_MSC_VER) && !defined(__MINGW64__) && !defined(__MINGW32__)
	if (daemon)
	{
		asio::signal_set signal_receiver(sp, SIGINT, SIGTERM, SIGUSR1);
		std::function<void (const asio::error_code&, int)> signal_handler = [&](const asio::error_code& ec, int signal_number) {
			if (!ec)
			{
				if (SIGUSR1 == signal_number)
				{
					echo_server.list_all_status();
					echo_stream_server.list_all_status();
				}
				else
					return sp.end_service();
			}

			signal_receiver.async_wait([&signal_handler](const asio::error_code& ec, int signal_number) {signal_handler(ec, signal_number);});
		};
		signal_receiver.async_wait([&signal_handler](const asio::error_code& ec, int signal_number) {signal_handler(ec, signal_number);});

		sp.run_service();
		return 0;
	}
#else
	(void) daemon;
#endif

	sp.start_service();
	while (sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (STATUS == str)
		{
			echo_server.list_all_status();
			echo_stream_server.list_all_status();
		}
		else
		{
		}
	}

	return 0;
}
