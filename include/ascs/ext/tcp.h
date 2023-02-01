/*
 * tcp.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * TCP related conveniences.
 */

#ifndef _ASCS_EXT_TCP_H_
#define _ASCS_EXT_TCP_H_

#include "packer.h"
#include "unpacker.h"
#include "../tcp/client_socket.h"
#include "../tcp/proxy/socks.h"
#include "../tcp/client.h"
#include "../tcp/server_socket.h"
#include "../tcp/server.h"
#include "../single_service_pump.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER ascs::ext::packer<>
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER ascs::ext::unpacker<>
#endif

namespace ascs { namespace ext { namespace tcp {

typedef ascs::tcp::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> client_socket;
template<typename Matrix = i_matrix> using client_socket2 = ascs::tcp::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Matrix>;
typedef client_socket connector;
typedef ascs::tcp::single_client_base<client_socket> single_client;
typedef ascs::tcp::multi_client_base<client_socket> multi_client;
template<typename Socket, typename Matrix = i_matrix> using multi_client2 = ascs::tcp::multi_client_base<Socket, object_pool<Socket>, Matrix>;
typedef multi_client client;

typedef ascs::tcp::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> server_socket;
template<typename Server = ascs::tcp::i_server> using server_socket2 = ascs::tcp::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Server>;
typedef ascs::tcp::server_base<server_socket> server;
template<typename Socket, typename Server = ascs::tcp::i_server> using server2 = ascs::tcp::server_base<Socket, object_pool<Socket>, Server>;

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
typedef ascs::tcp::unix_client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> unix_client_socket;
template<typename Matrix = i_matrix> using unix_client_socket2 = ascs::tcp::unix_client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Matrix>;
typedef ascs::tcp::single_client_base<unix_client_socket> unix_single_client;
typedef ascs::tcp::multi_client_base<unix_client_socket> unix_multi_client;
//typedef multi_client2 unix_multi_client2; //multi_client2 can be used for unix socket too, but we cannot typedef it (use using instead).

typedef ascs::tcp::unix_server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> unix_server_socket;
template<typename Server = ascs::tcp::i_server> using unix_server_socket2 = ascs::tcp::unix_server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Server>;
typedef ascs::tcp::unix_server_base<unix_server_socket> unix_server;
template<typename Socket, typename Server = ascs::tcp::i_server> using unix_server2 = ascs::tcp::unix_server_base<Socket, object_pool<Socket>, Server>;
#endif

namespace proxy {

namespace socks4 {
	typedef ascs::tcp::proxy::socks4::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> client_socket;
	template<typename Matrix = i_matrix> using client_socket2 = ascs::tcp::proxy::socks4::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Matrix>;
	typedef client_socket connector;
	typedef ascs::tcp::single_client_base<client_socket> single_client;
	typedef ascs::tcp::multi_client_base<client_socket> multi_client;
	//typedef ascs::ext::tcp::multi_client2 multi_client2; //multi_client2 can be used for socks4 too, but we cannot typedef it (use using instead).
	typedef multi_client client;
}

namespace socks5 {
	typedef ascs::tcp::proxy::socks5::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> client_socket;
	template<typename Matrix = i_matrix> using client_socket2 = ascs::tcp::proxy::socks5::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Matrix>;
	typedef client_socket connector;
	typedef ascs::tcp::single_client_base<client_socket> single_client;
	typedef ascs::tcp::multi_client_base<client_socket> multi_client;
	//typedef ascs::ext::tcp::multi_client2 multi_client2; //multi_client2 can be used for socks5 too, but we cannot typedef it (use using instead).
	typedef multi_client client;
}

}

}}} //namespace

#endif /* _ASCS_EXT_TCP_H_ */
