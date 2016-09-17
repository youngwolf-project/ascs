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

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER packer
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER unpacker
#endif

namespace ascs { namespace ext {

typedef tcp::connector_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> connector;
typedef single_socket_service<connector> sclient;
typedef tcp::client_base<connector> client;

}} //namespace

#endif /* _ASCS_EXT_CLIENT_H_ */
