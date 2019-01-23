
#include <iostream>
#include <map>

//configuration
#define ASCS_REUSE_OBJECT		//use objects pool
#define ASCS_HEARTBEAT_INTERVAL	5
#define ASCS_AVOID_AUTO_STOP_SERVICE
#define ASCS_RECONNECT			false
#define ASCS_DEFAULT_PACKER		prefix_suffix_packer
#define ASCS_DEFAULT_UNPACKER	prefix_suffix_unpacker
//configuration

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#include "server.h"
#include "client.h"

int main(int argc, const char* argv[])
{
	service_pump sp;
	my_server server(sp);
	my_client client(sp);

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if ("quit" == str)
			sp.stop_service();
		else
		{
			auto parameters = split_string(str);
			auto iter = std::begin(parameters);
			if (iter == std::end(parameters))
				continue;

			if ("add" == *iter)
			{
				++iter;
				if (iter != std::end(parameters))
					client.add_link(*iter);
			}
			else if ("del" == *iter)
			{
				++iter;
				if (iter != std::end(parameters))
					client.shutdown_link(*iter);
			}
			else
			{
				std::string name = *iter++;
				for (; iter != std::end(parameters); ++iter)
					client.send_msg(name, *iter);
			}
		}
	}

    return 0;
}
