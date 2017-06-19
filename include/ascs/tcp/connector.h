/*
 * connector.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef _ASCS_CONNECTOR_H_
#define _ASCS_CONNECTOR_H_

#include "client_socket.h"

namespace ascs { namespace tcp {

#ifdef ASCS_HAS_TEMPLATE_USING
template <typename Packer, typename Unpacker, typename Socket = asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
using connector_base = client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>;
#else
template <typename Packer, typename Unpacker, typename Socket = asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class connector_base : public client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>;
{
private:
	typedef client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	connector_base(asio::io_service& io_service_) : super(io_service_) {}
	template<typename Arg> connector_base(asio::io_service& io_service_, Arg& arg) : super(io_service_, arg) {}
};
#endif

}} //namespace

#endif /* _ASCS_CONNECTOR_H_ */
