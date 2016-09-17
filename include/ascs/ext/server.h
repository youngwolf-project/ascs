/*
 * server.h
 *
 *  Created on: 2016-9-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * server related conveniences.
 */

#ifndef _ASCS_EXT_SERVER_H_
#define _ASCS_EXT_SERVER_H_

#include "packer.h"
#include "unpacker.h"
#include "../tcp/server_socket.h"
#include "../tcp/server.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER packer
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER unpacker
#endif

namespace ascs { namespace ext {

typedef tcp::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> server_socket;
typedef tcp::server_base<server_socket> server;

}} //namespace

#endif /* _ASCS_EXT_SERVER_H_ */
