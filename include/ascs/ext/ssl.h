/*
 * ssl.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * ssl related conveniences.
 */

#ifndef _ASCS_EXT_SSL_H_
#define _ASCS_EXT_SSL_H_

#include "packer.h"
#include "unpacker.h"
#include "../tcp/ssl/ssl.h"
#include "../single_service_pump.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER ascs::ext::packer<>
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER ascs::ext::unpacker<>
#endif

namespace ascs { namespace ext { namespace ssl {

typedef ascs::ssl::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> client_socket;
template<typename Matrix = i_matrix> using client_socket2 = ascs::ssl::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Matrix>;
typedef client_socket connector;
typedef ascs::ssl::single_client_base<client_socket> single_client;
typedef ascs::ssl::multi_client_base<client_socket> multi_client;
template<typename Socket, typename Matrix = i_matrix> using multi_client2 = ascs::ssl::multi_client_base<Socket, ascs::ssl::object_pool<Socket>, Matrix>;
typedef multi_client client;

typedef ascs::ssl::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> server_socket;
template<typename Server = ascs::tcp::i_server> using server_socket2 = ascs::ssl::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, Server>;
typedef ascs::ssl::server_base<server_socket> server;
template<typename Socket, typename Server = ascs::tcp::i_server> using server2 = ascs::ssl::server_base<Socket, ascs::ssl::object_pool<Socket>, Server>;

}}} //namespace

#endif /* _ASCS_EXT_SSL_H_ */
