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
#define ASCS_DEFAULT_PACKER packer
#endif

#ifndef ASCS_DEFAULT_UNPACKER
#define ASCS_DEFAULT_UNPACKER unpacker
#endif

namespace ascs { namespace ext {

typedef ssl::server_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> ssl_server_socket;
typedef ssl::server_base<ssl_server_socket> ssl_server;

typedef ssl::connector_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER> ssl_connector;
typedef single_socket_service<ssl_connector> ssl_sclient;
typedef tcp::client_base<ssl_connector, ssl::object_pool<ssl_connector> > ssl_client;

}} //namespace

#endif /* _ASCS_EXT_SSL_H_ */
