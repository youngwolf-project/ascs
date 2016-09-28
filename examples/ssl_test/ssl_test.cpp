
#include <iostream>

//configuration
#define ASCS_SERVER_PORT		9527
#define ASCS_ASYNC_ACCEPT_NUM	5
//#define ASCS_FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ASCS_ENHANCED_STABILITY
//#define ASCS_DEFAULT_PACKER replaceable_packer
//#define ASCS_DEFAULT_UNPACKER replaceable_unpacker
//configuration

#include <ascs/ext/ssl.h>
using namespace ascs;
using namespace ascs::ext::ssl;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT_COMMAND "reconnect"

int main(int argc, const char* argv[])
{
	puts("Directories certs and client_certs must be in the current directory.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;

	server server_(sp, asio::ssl::context::sslv23_server);
	server_.context().set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use);
	server_.context().set_verify_mode(asio::ssl::context::verify_peer | asio::ssl::context::verify_fail_if_no_peer_cert);
	server_.context().load_verify_file("client_certs/server.crt");
	server_.context().use_certificate_chain_file("certs/server.crt");
	server_.context().use_private_key_file("certs/server.key", asio::ssl::context::pem);
	server_.context().use_tmp_dh_file("certs/dh1024.pem");

///*
	//method #1
	client client_(sp, asio::ssl::context::sslv23_client);
	client_.context().set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use);
	client_.context().set_verify_mode(asio::ssl::context::verify_peer | asio::ssl::context::verify_fail_if_no_peer_cert);
	client_.context().load_verify_file("certs/server.crt");
	client_.context().use_certificate_chain_file("client_certs/server.crt");
	client_.context().use_private_key_file("client_certs/server.key", asio::ssl::context::pem);
	client_.context().use_tmp_dh_file("client_certs/dh1024.pem");

	//please config the ssl context before creating any clients.
	client_.add_client();
	client_.add_client();
//*/
/*
	//method #2
	//to use single_client, we must construct ssl context first.
	asio::ssl::context ctx(asio::ssl::context::sslv23_client);
	ctx.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use);
	ctx.set_verify_mode(asio::ssl::context::verify_peer | asio::ssl::context::verify_fail_if_no_peer_cert);
	ctx.load_verify_file("certs/server.crt");
	ctx.use_certificate_chain_file("client_certs/server.crt");
	ctx.use_private_key_file("client_certs/server.key", asio::ssl::context::pem);
	ctx.use_tmp_dh_file("client_certs/dh1024.pem");

	single_client client_(sp, ctx);
*/
	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
		{
			sp.stop_service(&client_);
			sleep(1);
			sp.stop_service();
		}
		else if (RESTART_COMMAND == str || RECONNECT_COMMAND == str)
			puts("I still not find a way to reuse a asio::ssl::stream,\n"
				"it can reconnect to the server, but can not re-handshake with the server,\n"
				"if somebody knows how to fix this defect, please tell me, thanks in advance.");
		/*
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service(&client_);
			sleep(1);
			sp.stop_service();

			sp.start_service();
		}
		else if (RECONNECT_COMMAND == str)
			client_.graceful_shutdown(true);
		*/
		else
			server_.broadcast_msg(str);
	}

	return 0;
}

//restore configuration
#undef ASCS_SERVER_PORT
#undef ASCS_ASYNC_ACCEPT_NUM
#undef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
#undef ASCS_ENHANCED_STABILITY
#undef ASCS_DEFAULT_PACKER
#undef ASCS_DEFAULT_UNPACKER
//restore configuration
