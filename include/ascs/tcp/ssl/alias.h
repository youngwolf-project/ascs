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

#ifndef _ASCS_SSL_ALIAS_H_
#define _ASCS_SSL_ALIAS_H_

#include "ssl.h"

namespace ascs { namespace ssl {

template <typename Packer, typename Unpacker, typename Socket = asio::ssl::stream<asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
using connector_base = client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>;

template<typename Socket, typename Pool = object_pool<Socket>> using client_base = multi_client_base<Socket, Pool>;

}} //namespace

#endif /* _ASCS_SSL_ALIAS_H_ */