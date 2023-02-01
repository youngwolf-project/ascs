
#include <iostream>
#include <boost/tokenizer.hpp>

//configuration
#define ASCS_REUSE_OBJECT		//use objects pool
#define ASCS_HEARTBEAT_INTERVAL	5
#define ASCS_AVOID_AUTO_STOP_SERVICE
#define ASCS_RECONNECT			false
//#define ASCS_SHARED_MUTEX_TYPE std::shared_mutex	//we search objects frequently, defining this can promote performance, otherwise (or std::shared_mutex
//#define ASCS_SHARED_LOCK_TYPE	std::shared_lock	//is unavailable), you should not define these two macro and ascs will use std::mutex instead.

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	0
//0-default packer and unpacker, head(length) + body
//1-packer2 and unpacker2, head(length) + body
//2-fixed length packer and unpacker
//3-prefix and/or suffix packer and unpacker

#if 1 == PACKER_UNPACKER_TYPE
#define ASCS_DEFAULT_PACKER packer2<unique_buffer<std::string>, std::string>
#define ASCS_DEFAULT_UNPACKER unpacker2<unique_buffer<std::string>>
#elif 2 == PACKER_UNPACKER_TYPE
#undef ASCS_HEARTBEAT_INTERVAL
#define ASCS_HEARTBEAT_INTERVAL	0 //not support heartbeat
#define ASCS_DEFAULT_PACKER fixed_length_packer
#define ASCS_DEFAULT_UNPACKER fixed_length_unpacker
#elif 3 == PACKER_UNPACKER_TYPE
#define ASCS_DEFAULT_PACKER prefix_suffix_packer
#define ASCS_DEFAULT_UNPACKER prefix_suffix_unpacker
#endif
//configuration

//#include <shared_mutex>
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
			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char>> parameters(str, sep);
			auto iter = std::begin(parameters);
			if (iter == std::end(parameters))
				continue;

			if ("add" == *iter)
				for (++iter; iter != std::end(parameters); ++iter)
					client.add_link(*iter);
			else if ("del" == *iter)
				for (++iter; iter != std::end(parameters); ++iter)
					client.shutdown_link(*iter);
			else
			{
				std::string name = *iter++;
				for (; iter != std::end(parameters); ++iter)
#if 2 == PACKER_UNPACKER_TYPE
					client.send_msg(name, std::string(1024, '$')); //the default fixed length is 1024
#else
					client.send_msg(name, std::move(*iter));
#endif
			}
		}
	}

    return 0;
}
