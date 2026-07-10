
#include <iostream>

#include <ascs/ext/tcp.h>
#include <ascs/ext/callbacks.h>
using namespace ascs::tcp;
using namespace ascs::ext::callbacks;

#define QUIT_COMMAND	"quit"

//this callback can be invoked concurrently for different sockets
template<typename Socket, typename Msg> size_t cb_handle_msg(Socket* socket, Msg& msg)
{
	printf("handle msg: %s\n", msg.data());
	return 1;
}

typedef ascs::ext::tcp::client_socket orig_client_socket;
typedef c_socket<orig_client_socket> cb_client_socket;
typedef ascs::ext::tcp::server_socket orig_server_socket;
typedef s_socket<orig_server_socket> cb_server_socket;
int main()
{
	ascs::service_pump sp;

	single_client_base<cb_client_socket> c(sp);
	c.register_on_msg_handle([](orig_client_socket* socket, typename orig_client_socket::out_msg_type& msg) {
		return cb_handle_msg(socket, msg);
	});

	server_base<cb_server_socket, object_pool<ascs::object_pool<cb_server_socket>>> s(sp);
	s.register_on_create([](ascs::object_pool<cb_server_socket>*, typename ascs::object_pool<cb_server_socket>::object_ctype& ss) {
		ss->register_on_msg_handle([](orig_server_socket* socket, typename orig_server_socket::out_msg_type& msg) {
			return cb_handle_msg(socket, msg);
		});
	});

	multi_client_base<cb_client_socket, object_pool<ascs::object_pool<cb_client_socket>>> mc(sp);
	mc.register_on_create([](ascs::object_pool<cb_client_socket>*, typename ascs::object_pool<cb_client_socket>::object_ctype& cs) {
		cs->register_on_msg_handle([](orig_client_socket* socket, typename orig_client_socket::out_msg_type& msg) {
			return cb_handle_msg(socket, msg);
		});
	});
	mc.add_socket();
	mc.add_socket();

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
			c.send_msg(str + " (from single client)");
			s.broadcast_msg(str + " (from server)");
			mc.broadcast_msg(str + " (from multi client)");
		}
	}
}

