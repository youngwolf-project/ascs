/*
 * client.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef _ASCS_CLIENT_H_
#define _ASCS_CLIENT_H_

#include "../socket_service.h"

namespace ascs { namespace tcp {

template<typename Socket, typename Pool = object_pool<Socket>>
class client_base : public multi_socket_service<Socket, Pool>
{
protected:
	typedef multi_socket_service<Socket, Pool> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	client_base(service_pump& service_pump_) : super(service_pump_) {}
	template<typename Arg>
	client_base(service_pump& service_pump_, const Arg& arg) : super(service_pump_, arg) {}

	//connected link size, may smaller than total object size(object_pool::size)
	size_t valid_size()
	{
		size_t size = 0;
		ASCS_THIS do_something_to_all([&size](const auto& item) {if (item->is_connected()) ++size;});
		return size;
	}

	typename Pool::object_type add_client()
	{
		auto client_ptr(ASCS_THIS create_object());
		return ASCS_THIS add_socket(client_ptr, false) ? client_ptr : typename Pool::object_type();
	}
	typename Pool::object_type add_client(unsigned short port, const std::string& ip = ASCS_SERVER_IP)
	{
		auto client_ptr(ASCS_THIS create_object());
		client_ptr->set_server_addr(port, ip);
		return ASCS_THIS add_socket(client_ptr, false) ? client_ptr : typename Pool::object_type();
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a client_ptr parameter will remove the link from object pool first, then call corresponding function, if you want to reconnect to the server,
	//please call client_ptr's 'disconnect' 'force_shutdown' or 'graceful_shutdown' with true 'reconnect' directly.
	void disconnect(typename Pool::object_ctype& client_ptr) {ASCS_THIS del_object(client_ptr); client_ptr->disconnect(false);}
	void disconnect(bool reconnect = false) {ASCS_THIS do_something_to_all([=](const auto& item) {item->disconnect(reconnect);});}
	void force_shutdown(typename Pool::object_ctype& client_ptr) {ASCS_THIS del_object(client_ptr); client_ptr->force_shutdown(false);}
	void force_shutdown(bool reconnect = false) {ASCS_THIS do_something_to_all([=](const auto& item) {item->force_shutdown(reconnect);});}
	void graceful_shutdown(typename Pool::object_ctype& client_ptr, bool sync = true) {ASCS_THIS del_object(client_ptr); client_ptr->graceful_shutdown(false, sync);}
	void graceful_shutdown(bool reconnect = false, bool sync = true) {ASCS_THIS do_something_to_all([=](const auto& item) {item->graceful_shutdown(reconnect, sync);});}

protected:
	virtual void uninit() {ASCS_THIS stop(); graceful_shutdown();}
};

}} //namespace

#endif /* _ASCS_CLIENT_H_ */
