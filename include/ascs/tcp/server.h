/*
 * server.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef _ASCS_SERVER_H_
#define _ASCS_SERVER_H_

#include "../object_pool.h"

namespace ascs { namespace tcp {

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = i_server>
class server_base : public Server, public Pool
{
public:
	using Pool::TIMER_BEGIN;
	using Pool::TIMER_END;

	server_base(service_pump& service_pump_) : Pool(service_pump_), acceptor(service_pump_) {set_server_addr(ASCS_SERVER_PORT);}
	template<typename Arg>
	server_base(service_pump& service_pump_, const Arg& arg) : Pool(service_pump_, arg), acceptor(service_pump_) {set_server_addr(ASCS_SERVER_PORT);}

	bool set_server_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			server_addr = asio::ip::tcp::endpoint(ASCS_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			asio::error_code ec;
			auto addr = asio::ip::address::from_string(ip, ec);
			if (ec)
				return false;

			server_addr = asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}
	const asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	void stop_listen() {asio::error_code ec; acceptor.cancel(ec); acceptor.close(ec);}
	bool is_listening() const {return acceptor.is_open();}

	//implement i_server's pure virtual functions
	virtual service_pump& get_service_pump() {return Pool::get_service_pump();}
	virtual const service_pump& get_service_pump() const {return Pool::get_service_pump();}
	virtual bool del_client(const std::shared_ptr<timer>& client_ptr)
	{
		auto raw_client_ptr(std::dynamic_pointer_cast<Socket>(client_ptr));
		return raw_client_ptr && this->del_object(raw_client_ptr) ? raw_client_ptr->force_shutdown(), true : false;
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into send buffer successfully
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a client_ptr parameter will remove the link from object pool first, then call corresponding function.
	void disconnect(typename Pool::object_ctype& client_ptr) {this->del_object(client_ptr); client_ptr->disconnect();}
	void disconnect() {this->do_something_to_all([=](const auto& item) {item->disconnect();});}
	void force_shutdown(typename Pool::object_ctype& client_ptr) {this->del_object(client_ptr); client_ptr->force_shutdown();}
	void force_shutdown() {this->do_something_to_all([=](const auto& item) {item->force_shutdown();});}
	void graceful_shutdown(typename Pool::object_ctype& client_ptr, bool sync = false) {this->del_object(client_ptr); client_ptr->graceful_shutdown(sync);}
	void graceful_shutdown() {this->do_something_to_all([](const auto& item) {item->graceful_shutdown();});} //async must be false(the default value), or dead lock will occur.

protected:
	virtual bool init()
	{
		asio::error_code ec;
		acceptor.open(server_addr.protocol(), ec); assert(!ec);
#ifndef ASCS_NOT_REUSE_ADDRESS
		acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec); assert(!ec);
#endif
		acceptor.bind(server_addr, ec); assert(!ec);
		if (ec) {get_service_pump().stop(); unified_out::error_out("bind failed."); return false;}
		acceptor.listen(asio::ip::tcp::acceptor::max_connections, ec); assert(!ec);
		if (ec) {get_service_pump().stop(); unified_out::error_out("listen failed."); return false;}

		this->start();

		for (auto i = 0; i < ASCS_ASYNC_ACCEPT_NUM; ++i)
			start_next_accept();

		return true;
	}
	virtual void uninit() {this->stop(); stop_listen(); force_shutdown();}
	virtual bool on_accept(typename Pool::object_ctype& client_ptr) {return true;}

	//if you want to ignore this error and continue to accept new connections immediately, return true in this virtual function;
	//if you want to ignore this error and continue to accept new connections after a specific delay, start a timer immediately and return false (don't call stop_listen()),
	// when the timer ends up, call start_next_accept() in the callback function.
	//otherwise, don't rewrite this virtual function or call server_base::on_accept_error() directly after your code.
	virtual bool on_accept_error(const asio::error_code& ec, typename Pool::object_ctype& client_ptr)
	{
		if (asio::error::operation_aborted != ec)
		{
			unified_out::error_out("failed to accept new connection because of %s, will stop listening.", ec.message().data());
			stop_listen();
		}

		return false;
	}

	virtual void start_next_accept()
	{
		auto client_ptr = this->create_object(*this);
		acceptor.async_accept(client_ptr->lowest_layer(), [=](const auto& ec) {this->accept_handler(ec, client_ptr);});
	}

protected:
	bool add_client(typename Pool::object_ctype& client_ptr)
	{
		if (this->add_object(client_ptr))
		{
			client_ptr->show_info("client:", "arrive.");
			return true;
		}

		client_ptr->show_info("client:", "been refused because of too many clients.");
		client_ptr->force_shutdown();
		return false;
	}

	void accept_handler(const asio::error_code& ec, typename Pool::object_ctype& client_ptr)
	{
		if (!ec)
		{
			if (on_accept(client_ptr) && add_client(client_ptr))
				client_ptr->start();

			start_next_accept();
		}
		else if (on_accept_error(ec, client_ptr))
			start_next_accept();
	}

protected:
	asio::ip::tcp::endpoint server_addr;
	asio::ip::tcp::acceptor acceptor;
};

}} //namespace

#endif /* _ASCS_SERVER_H_ */
