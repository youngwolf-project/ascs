
#include <iostream>

//configuration
//#define ASCS_DEFAULT_PACKER replaceable_packer<>
//#define ASCS_DEFAULT_UDP_UNPACKER replaceable_udp_unpacker<>
//configuration

#include <ascs/ext/udp.h>
using namespace ascs;
using namespace ascs::ext::udp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

int main(int argc, const char* argv[])
{
	printf("usage: %s <my port> <peer port> [peer ip=127.0.0.1]\n", argv[0]);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else if (argc < 3)
		return 1;
	else
		puts("type " QUIT_COMMAND " to end.");

	auto local_port = (unsigned short) atoi(argv[1]);
	asio::error_code ec;
	auto peer_addr = asio::ip::udp::endpoint(asio::ip::address::from_string(argc >= 4 ? argv[3] : "127.0.0.1", ec), (unsigned short) atoi(argv[2]));
	assert(!ec);

	std::string str;
	service_pump sp;
	single_service service(sp);
	service.set_local_addr(local_port);

	sp.start_service();
	while(sp.is_running())
	{
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else
			service.safe_send_native_msg(peer_addr, str);
	}

	return 0;
}
