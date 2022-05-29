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

#ifndef _ASCS_TCP_ALIAS_H_
#define _ASCS_TCP_ALIAS_H_

#include "client_socket.h"
#include "client.h"

namespace ascs { namespace tcp {

template<typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = asio::ip::tcp::socket,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
using connector_base = client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>;

template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix> using client_base = multi_client_base<Socket, Pool, Matrix>;

}} //namespace

#endif /* _ASCS_TCP_ALIAS_H_ */