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
#include "../single_service_pump.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER ascs::ext::packer<>
#endif

#ifndef ASCS_DEFAULT_UDP_UNPACKER
#define ASCS_DEFAULT_UDP_UNPACKER ascs::ext::udp_unpacker
#endif

namespace ascs { namespace ext { namespace udp {

typedef ascs::udp::socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER> socket;
template<typename Matrix = i_matrix> using socket2 = ascs::udp::socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER, Matrix>;
typedef ascs::udp::single_socket_service_base<socket> single_socket_service;
typedef ascs::udp::multi_socket_service_base<socket> multi_socket_service;
template<typename Socket, typename Matrix = i_matrix> using multi_socket_service2 = ascs::udp::multi_socket_service_base<Socket, object_pool<Socket>, Matrix>;
typedef multi_socket_service socket_service;

#ifdef ASIO_HAS_LOCAL_SOCKETS
typedef ascs::udp::unix_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER> unix_socket;
template<typename Matrix = i_matrix> using unix_socket2 = ascs::udp::unix_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER, Matrix>;
typedef ascs::udp::single_socket_service_base<unix_socket> unix_single_socket_service;
typedef ascs::udp::multi_socket_service_base<unix_socket> unix_multi_socket_service;
//typedef multi_socket_service2 unix_multi_socket_service2; //multi_socket_service2 can be used for unix socket too, but we cannot typedef it (use using instead).
#endif

}}} //namespace

#endif /* _ASCS_EXT_UDP_H_ */
