
#include <iostream>

//configuration
#define ASCS_SERVER_PORT	5050
#define ASCS_DELAY_CLOSE	5 //define this to avoid hooks for async call (and slightly improve efficiency)
//#define ASCS_INPUT_QUEUE non_lock_queue
//we cannot use non_lock_queue, because we also send messages (talking messages) out of ascs::socket::on_msg_send().
#define ASCS_DEFAULT_UNPACKER replaceable_unpacker<>
#define ASCS_RECV_BUFFER_TYPE std::vector<asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
#define ASCS_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to ascs
//configuration

#include "file_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define REQUEST_FILE	"get"

std::atomic_ushort completed_client_num;
int link_num = 1;
fl_type file_size;

int main(int argc, const char* argv[])
{
	puts("this is a file transfer client.");
	printf("usage: %s [<port=%d> [<ip=%s> [link num=1]]]\n", argv[0], ASCS_SERVER_PORT, ASCS_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	file_client client(sp);

	if (argc > 3)
		link_num = std::min(256, std::max(atoi(argv[3]), 1));

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
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else if (str.size() > sizeof(REQUEST_FILE) && !strncmp(REQUEST_FILE, str.data(), sizeof(REQUEST_FILE) - 1) && isspace(str[sizeof(REQUEST_FILE) - 1]))
		{
			str.erase(0, sizeof(REQUEST_FILE));
			auto files = split_string(str);
			do_something_to_all(files, [&](const std::string& item) {
				completed_client_num = 0;
				file_size = 0;

				printf("transfer %s begin.\n", item.data());
				if (client.find(0)->get_file(item))
				{
					//client.do_something_to_all([&item](file_client::object_ctype& item2) {if (0 != item2->id()) item2->get_file(item);});
					//if you always return false, do_something_to_one will be equal to do_something_to_all.
					client.do_something_to_one([&item](file_client::object_ctype& item2)->bool {if (0 != item2->id()) item2->get_file(item); return false;});
					client.start();

					while (completed_client_num != (unsigned short) link_num)
						std::this_thread::sleep_for(std::chrono::milliseconds(50));

					client.stop(item);
				}
				else
					printf("transfer %s failed!\n", item.data());
			});
		}
		else
			client.at(0)->talk(str);
	}

	return 0;
}
