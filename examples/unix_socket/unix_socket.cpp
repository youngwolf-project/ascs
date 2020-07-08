
#include <iostream>

//configuration
#define ASCS_REUSE_OBJECT		//use objects pool
//configuration

//#include <shared_mutex>
#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#define QUIT				"quit"
#define UNIX_SOCKET_NAME	"unix-socket"

int main(int argc, const char* argv[])
{
	service_pump sp;

	unix_server server(sp);
	unix_single_client client(sp);

	unlink(UNIX_SOCKET_NAME);
	server.set_server_addr(UNIX_SOCKET_NAME);
	client.set_server_addr(UNIX_SOCKET_NAME);

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT == str)
			sp.stop_service();
		else
		{
			client.send_msg("client says: " + str);
			server.broadcast_msg("server says: " + str);
		}
	}
	unlink(UNIX_SOCKET_NAME);

	return 0;
}
