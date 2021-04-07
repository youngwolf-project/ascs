
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	5050
#define ASCS_DELAY_CLOSE	5 //define this to avoid hooks for async call (and slightly improve efficiency)
#define ASCS_PASSIVE_RECV
#define ASCS_RECV_BUFFER_TYPE std::vector<asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
#define ASCS_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to ascs
//configuration

#include "file_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define REQUEST_FILE	"get"

int link_num = 1;
fl_type file_size;
std::atomic_int_fast64_t received_size;

int main(int argc, const char* argv[])
{
	puts("this is a file transmission client.");
	printf("usage: %s [<port=%d> [<ip=%s> [link num=1]]]\n", argv[0], ASCS_SERVER_PORT, ASCS_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	file_client client(sp);

	if (argc > 3)
		link_num = std::min(256, std::max(atoi(argv[3]), 1)); //link number cannot exceed 500, because file_server's macro ASCS_START_OBJECT_ID is defined as 500

	for (auto i = 0; i < link_num; ++i)
	{
//		argv[2] = "::1" //ipv6
//		argv[2] = "127.0.0.1" //ipv4
		if (argc > 2)
			client.add_socket(atoi(argv[1]), argv[2])->set_index(i);
		else if (argc > 1)
			client.add_socket(atoi(argv[1]))->set_index(i);
		else
			client.add_socket()->set_index(i);
	}

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else if (STATISTIC == str)
		{
			printf("link #: " ASCS_SF ", valid links: " ASCS_SF ", invalid links: " ASCS_SF "\n\n", client.size(), client.valid_size(), client.invalid_object_size());
			puts(client.get_statistic().to_string().data());
		}
		else if (STATUS == str)
			client.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			client.list_all_object();
		else if (str.size() > sizeof(REQUEST_FILE) && !strncmp(REQUEST_FILE, str.data(), sizeof(REQUEST_FILE) - 1) && isspace(str[sizeof(REQUEST_FILE) - 1]))
		{
			str.erase(0, sizeof(REQUEST_FILE));
			client.get_file(split_string(str));
		}
		else
			client.at(0)->talk(str);
	}

	return 0;
}
