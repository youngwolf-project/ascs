/*
 * reliable_udp.h
 *
 *  Created on: 2021-9-3
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * reliable udp related conveniences.
 */

#ifndef _ASCS_EXT_RELIABLE_UDP_H_
#define _ASCS_EXT_RELIABLE_UDP_H_

#include "packer.h"
#include "unpacker.h"
#include "../udp/reliable_socket.h"
#include "../udp/socket_service.h"
#include "../single_service_pump.h"

#ifndef ASCS_DEFAULT_PACKER
#define ASCS_DEFAULT_PACKER ascs::ext::packer<>
#endif

#ifndef ASCS_DEFAULT_UDP_UNPACKER
#define ASCS_DEFAULT_UDP_UNPACKER ascs::ext::udp_unpacker
#endif

namespace ascs { namespace ext { namespace udp {

typedef ascs::udp::reliable_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER> reliable_socket;
template<typename Matrix = i_matrix> using reliable_socket2 = ascs::udp::reliable_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UDP_UNPACKER, Matrix>;
typedef ascs::udp::single_socket_service_base<reliable_socket> single_reliable_socket_service;
typedef ascs::udp::multi_socket_service_base<reliable_socket> multi_reliable_socket_service;
template<typename Socket, typename Matrix = i_matrix> using multi_reliable_socket_service2 = ascs::udp::multi_socket_service_base<Socket, object_pool<Socket>, Matrix>;
typedef multi_reliable_socket_service reliable_socket_service;

}}} //namespace

#endif /* _ASCS_EXT_RELIABLE_UDP_H_ */
