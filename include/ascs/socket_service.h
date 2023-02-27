/*
 * socket_service.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef _ASCS_SOCKET_SERVICE_H_
#define _ASCS_SOCKET_SERVICE_H_

#include "object_pool.h"

namespace ascs
{

//only support one socket
template<typename Socket>
class single_socket_service : public service_pump::i_service, public Socket
{
public:
	single_socket_service(service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_) {}
	template<typename Arg> single_socket_service(service_pump& service_pump_, Arg&& arg) : i_service(service_pump_), Socket(service_pump_, std::forward<Arg>(arg)) {}
	~single_socket_service() {clear_io_context_refs();}

	using Socket::id; //release these functions

protected:
	virtual bool init() {this->reset_io_context_refs(); this->start(); return Socket::started();}
	virtual void uninit() {this->graceful_shutdown();} //if you wanna force shutdown, call force_shutdown before service_pump::stop_service invocation.

private:
	virtual void attach_io_context(boost::asio::io_context& io_context_, unsigned refs) {get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(boost::asio::io_context& io_context_, unsigned refs) {get_service_pump().return_io_context(io_context_, refs);}

private:
	//hide these functions
	using Socket::add_io_context_refs;
	using Socket::sub_io_context_refs;
	using Socket::clear_io_context_refs;
	using Socket::get_matrix;
};

template<typename Socket, typename Pool, typename Matrix>
class multi_socket_service : public Matrix, public Pool
{
protected:
	multi_socket_service(service_pump& service_pump_) : Pool(service_pump_) {}
	template<typename Arg> multi_socket_service(service_pump& service_pump_, Arg&& arg) : Pool(service_pump_, std::forward<Arg>(arg)) {}
	~multi_socket_service() {this->clear_io_context_refs();}

	virtual bool init()
	{
		this->do_something_to_all([](typename Pool::object_ctype& item) {item->start();});
		this->start();
		return true;
	}

public:
	//implement i_matrix's pure virtual functions
	virtual bool started() const {return this->service_started();}
	virtual service_pump& get_service_pump() {return Pool::get_service_pump();}
	virtual const service_pump& get_service_pump() const {return Pool::get_service_pump();}

	virtual bool socket_exist(uint_fast64_t id) {return this->exist(id);}
	virtual std::shared_ptr<tracked_executor> find_socket(uint_fast64_t id) {return this->find(id);}
	virtual bool del_socket(uint_fast64_t id) {return this->del_object(id);}

	typename Pool::object_type create_object() {return Pool::create_object(*this);}
	template<typename Arg> typename Pool::object_type create_object(Arg&& arg) {return Pool::create_object(*this, std::forward<Arg>(arg));}

	//will call socket_ptr's start function if the service_pump already started.
	bool add_socket(typename Pool::object_ctype& socket_ptr, unsigned additional_io_context_refs = 0)
	{
		if (this->add_object(socket_ptr))
		{
			socket_ptr->add_io_context_refs(additional_io_context_refs);
			if (get_service_pump().is_service_started()) //service already started
				socket_ptr->start();

			return true;
		}

		return false;
	}

private:
	virtual void attach_io_context(boost::asio::io_context& io_context_, unsigned refs) {get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(boost::asio::io_context& io_context_, unsigned refs) {get_service_pump().return_io_context(io_context_, refs);}
};

} //namespace

#endif /* _ASCS_SOCKET_SERVICE_H_ */
