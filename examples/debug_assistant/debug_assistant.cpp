
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
	virtual bool on_msg_handle(out_msg_type& msg) {return send_native_msg(msg.peer_addr, msg);}
	//msg handling end
};

int main()
{
	puts("echo server with length (2 bytes big endian) + body protocol: 9527\n"
		"echo server with non-protocol: 9528\n"
		"echo server udp: 9528\n"
		"type " QUIT_COMMAND " to end.");

	service_pump sp;
	server_base<echo_socket> echo_server(sp);
	server_base<echo_stream_socket> echo_stream_server(sp);
	single_udp_service udp_service(sp);

	echo_stream_server.set_server_addr(9528);
	udp_service.set_local_addr(9528);

	sp.start_service();
	while (sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else
		{
		}
	}

	return 0;
}
