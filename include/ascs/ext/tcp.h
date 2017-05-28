/*
 * client.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * client related conveniences.
 */

#ifndef _ASCS_EXT_CLIENT_H_
#define _ASCS_EXT_CLIENT_H_

#include "packer.h"
#include "unpacker.h"
#include "../tcp/connector.h"
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

typedef ascs::tcp::connector_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> connector;
typedef ascs::tcp::single_client_base<connector> single_client;
typedef ascs::tcp::client_base<connector> client;

typedef ascs::tcp::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> server_socket;
typedef ascs::tcp::server_base<server_socket> server;

}}} //namespace

#endif /* _ASCS_EXT_CLIENT_H_ */
