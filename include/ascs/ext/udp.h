/*
 * udp.h
 *
 *  Created on: 2016-9-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * udp related conveniences.
 */

#ifndef _ASCS_EXT_UDP_H_
#define _ASCS_EXT_UDP_H_

#include "packer.h"
#include "unpacker.h"
#include "../udp/socket.h"
#include "../udp/socket_service.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER packer
#endif

#ifndef ASCS_DEFAULT_UDP_UNPACKER
#define ASCS_DEFAULT_UDP_UNPACKER udp_unpacker
#endif

namespace ascs { namespace ext {

typedef udp::socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER> udp_socket;
typedef single_socket_service<udp_socket> udp_sservice;
typedef udp::service_base<udp_socket> udp_service;

}} //namespace

#endif /* _ASCS_EXT_UDP_H_ */
