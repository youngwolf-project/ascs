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
#include "../tcp/client.h"
#include "../tcp/server_socket.h"
#include "../tcp/server.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER ascs::ext::packer
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER ascs::ext::unpacker
#endif

namespace ascs { namespace ext { namespace tcp {

typedef ascs::tcp::client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> client_socket;
typedef client_socket connector;
typedef ascs::tcp::single_client_base<client_socket> single_client;
typedef ascs::tcp::client_base<client_socket> client;

typedef ascs::tcp::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> server_socket;
typedef ascs::tcp::server_base<server_socket> server;

}}} //namespace

#endif /* _ASCS_EXT_TCP_H_ */
