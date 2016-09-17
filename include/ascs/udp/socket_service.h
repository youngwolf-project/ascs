/*
 * socket_service.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * UDP socket service
 */

#ifndef _ASCS_UDP_SOCKET_SERVICE_H_
#define _ASCS_UDP_SOCKET_SERVICE_H_

#include "../socket_service.h"

namespace ascs { namespace udp {

template<typename Socket, typename Pool = object_pool<Socket>>
class service_base : public multi_socket_service<Socket, Pool>
{
protected:
	typedef multi_socket_service<Socket, Pool> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	service_base(service_pump& service_pump_) : super(service_pump_) {}

	using super::add_socket;
	typename Pool::object_type add_socket(unsigned short port, const std::string& ip = std::string())
	{
		auto socket_ptr(ASCS_THIS create_object());
		socket_ptr->set_local_addr(port, ip);
		return ASCS_THIS add_socket(socket_ptr) ? socket_ptr : typename Pool::object_type();
	}

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function
	void disconnect(typename Pool::object_ctype& socket_ptr) {ASCS_THIS del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {ASCS_THIS do_something_to_all([](typename Pool::object_ctype& item) {item->disconnect();});}
	void force_close(typename Pool::object_ctype& socket_ptr) {ASCS_THIS del_object(socket_ptr); socket_ptr->force_close();}
	void force_close() {ASCS_THIS do_something_to_all([](typename Pool::object_ctype& item) {item->force_close();});}
	void graceful_close(typename Pool::object_ctype& socket_ptr) {ASCS_THIS del_object(socket_ptr); socket_ptr->graceful_close();}
	void graceful_close() {ASCS_THIS do_something_to_all([](typename Pool::object_ctype& item) {item->graceful_close();});}

protected:
	virtual void uninit() {ASCS_THIS stop(); graceful_close();}
};

}} //namespace

#endif /* _ASCS_UDP_SOCKET_SERVICE_H_ */
