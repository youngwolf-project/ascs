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

template<typename Socket> using single_service_base = single_socket_service<Socket>;

template<typename Socket, typename Pool = object_pool<Socket>>
class multi_service_base : public multi_socket_service<Socket, Pool>
{
private:
	typedef multi_socket_service<Socket, Pool> super;

public:
	multi_service_base(service_pump& service_pump_) : super(service_pump_) {}

	using super::add_socket;
	typename Pool::object_type add_socket(unsigned short port, const std::string& ip = std::string())
	{
		auto socket_ptr(this->create_object());
		socket_ptr->set_local_addr(port, ip);
		return this->add_socket(socket_ptr) ? socket_ptr : typename Pool::object_type();
	}

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function
	void disconnect(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {this->do_something_to_all([](typename Pool::object_ctype& item) {item->disconnect();});}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->force_shutdown();}
	void force_shutdown() {this->do_something_to_all([](typename Pool::object_ctype& item) {item->force_shutdown();});}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->graceful_shutdown();}
	void graceful_shutdown() {this->do_something_to_all([](typename Pool::object_ctype& item) {item->graceful_shutdown();});}

protected:
	virtual void uninit() {this->stop(); graceful_shutdown();}
};

}} //namespace

#endif /* _ASCS_UDP_SOCKET_SERVICE_H_ */
