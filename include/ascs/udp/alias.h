/*
* alias.h
*
*  Created on: 2017-7-14
*      Author: youngwolf
*		email: mail2tao@163.com
*		QQ: 676218192
*		Community on QQ: 198941541
*
* some alias, they are deprecated.
*/

#ifndef _ASCS_UDP_ALIAS_H_
#define _ASCS_UDP_ALIAS_H_

#include "socket_service.h"

namespace ascs { namespace udp {

template<typename Socket, typename Pool = object_pool<Socket>> using service_base = multi_service_base<Socket, Pool>;

}} //namespace

#endif /* _ASCS_UDP_ALIAS_H_ */