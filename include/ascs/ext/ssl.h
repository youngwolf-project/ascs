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
#include "../ssl/ssl.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER ascs::ext::packer
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER ascs::ext::unpacker
#endif

namespace ascs { namespace ext { namespace ssl {

typedef ascs::ssl::connector_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> connector;
typedef ascs::single_socket_service<ascs::ext::ssl::connector> single_client;
typedef ascs::tcp::client_base<ascs::ext::ssl::connector, ascs::ssl::object_pool<ascs::ext::ssl::connector>> client;

typedef ascs::ssl::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> server_socket;
typedef ascs::ssl::server_base<ascs::ext::ssl::server_socket> server;

}}} //namespace

#endif /* _ASCS_EXT_SSL_H_ */
