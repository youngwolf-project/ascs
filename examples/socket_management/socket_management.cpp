
#include <iostream>
#include <map>

//configuration
#define ASCS_REUSE_OBJECT		//use objects pool
#define ASCS_HEARTBEAT_INTERVAL	5
#define ASCS_AVOID_AUTO_STOP_SERVICE
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

static std::map<std::string, uint_fast64_t> link_map;
static std::mutex link_map_mutex;

bool add_link(const std::string& name, uint_fast64_t id)
{
	std::lock_guard<std::mutex> lock(link_map_mutex);
	if (link_map.count(name) > 0)
	{
		printf("%s already exists.\n", name.data());
		return false;
	}

	printf("add socket %s.\n", name.data());
	link_map[name] = id;
	return true;
}

bool del_link(const std::string& name)
{
	std::lock_guard<std::mutex> lock(link_map_mutex);
	return link_map.erase(name) > 0;
}

uint_fast64_t find_link(const std::string& name)
{
	std::lock_guard<std::mutex> lock(link_map_mutex);
	auto iter = link_map.find(name);
	return iter != std::end(link_map) ? iter->second : -1;
}

uint_fast64_t find_and_del_link(const std::string& name)
{
	uint_fast64_t id = -1;

	std::lock_guard<std::mutex> lock(link_map_mutex);
	auto iter = link_map.find(name);
	if (iter != std::end(link_map))
	{
		id = iter->second;
		link_map.erase(iter);
	}

	return id;
}

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
					client.del_link(*iter);
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
