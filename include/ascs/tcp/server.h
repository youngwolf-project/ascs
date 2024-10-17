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

template<typename Socket, typename Family = asio::ip::tcp, typename Pool = object_pool<Socket>, typename Server = i_server>
class generic_server : public Server, public Pool
{
protected:
	generic_server(service_pump& service_pump_) : Pool(service_pump_), acceptor(service_pump_.assign_io_context()), io_context_refs(1), listening(false) {}
	template<typename Arg> generic_server(service_pump& service_pump_, Arg&& arg) :
		Pool(service_pump_, std::forward<Arg>(arg)), acceptor(service_pump_.assign_io_context()), io_context_refs(1), listening(false) {}
	~generic_server() {clear_io_context_refs(); Pool::clear_io_context_refs();}

public:
	bool set_server_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			server_addr = typename Family::endpoint(ASCS_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			asio::error_code ec;
#if ASIO_VERSION >= 101100
			auto addr = asio::ip::make_address(ip, ec);
#else
			auto addr = asio::ip::address::from_string(ip, ec);
#endif
			if (ec)
				return false;

			server_addr = asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}
	bool set_server_addr(const std::string& file_name) {server_addr = typename Family::endpoint(file_name); return true;}
	const typename Family::endpoint& get_server_addr() const {return server_addr;}

	void add_io_context_refs(unsigned count)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (count > 0)
		{
			io_context_refs += count;
#if ASIO_VERSION < 101100
			get_service_pump().assign_io_context(acceptor.get_io_service(), count);
#else
			get_service_pump().assign_io_context(acceptor.get_executor().context(), count);
#endif
		}
	}
	void sub_io_context_refs(unsigned count)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (count > 0 && io_context_refs >= count)
		{
			io_context_refs -= count;
#if ASIO_VERSION < 101100
			get_service_pump().return_io_context(acceptor.get_io_service(), count);
#else
			get_service_pump().return_io_context(acceptor.get_executor().context(), count);
#endif
		}
	}
	void clear_io_context_refs() {sub_io_context_refs(io_context_refs);}

	bool start_listen()
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (is_listening())
			return false;

		asio::error_code ec;
		if (!acceptor.is_open()) {acceptor.open(server_addr.protocol(), ec); assert(!ec);} //user maybe has opened this acceptor (to set options for example)
#ifndef ASCS_NOT_REUSE_ADDRESS
		acceptor.set_option(typename asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		acceptor.bind(server_addr, ec); assert(!ec);
		if (ec) {unified_out::error_out("bind failed."); return false;}
		listening = true;

		auto num = async_accept_num();
		assert(num > 0);
		if (num <= 0)
			num = 16;

		std::list<typename Pool::object_type> sockets;
		unified_out::info_out("begin to pre-create %d server socket...", num);
		while (--num >= 0)
		{
			auto socket_ptr(create_object());
			if (!socket_ptr)
				break;

			sockets.emplace_back(std::move(socket_ptr));
		}
		if (num >= 0)
			unified_out::info_out("finished pre-creating server sockets, but failed %d time(s).", num + 1);
		else
			unified_out::info_out("finished pre-creating server sockets.");

#if ASIO_VERSION > 101100
		acceptor.listen(asio::socket_base::max_listen_connections, ec); assert(!ec);
#else
		acceptor.listen(asio::socket_base::max_connections, ec); assert(!ec);
#endif
		if (ec) {unified_out::error_out("listen failed."); return false;}

		ascs::do_something_to_all(sockets, [this](typename Pool::object_ctype& item) {this->do_async_accept(item);});
		return true;
	}
	bool is_listening() const {return listening;}
	void stop_listen() {std::lock_guard<std::mutex> lock(mutex); listening = false; asio::error_code ec; acceptor.cancel(ec); acceptor.close(ec);}

	typename Family::acceptor& next_layer() {return acceptor;}
	const typename Family::acceptor& next_layer() const {return acceptor;}

	//implement i_server's pure virtual functions
	virtual bool started() const {return this->service_started();}
	virtual service_pump& get_service_pump() {return Pool::get_service_pump();}
	virtual const service_pump& get_service_pump() const {return Pool::get_service_pump();}

	virtual bool socket_exist(uint_fast64_t id) {return this->exist(id);}
	virtual std::shared_ptr<tracked_executor> find_socket(uint_fast64_t id) {return this->find(id);}
	virtual bool del_socket(const std::shared_ptr<tracked_executor>& socket_ptr)
	{
		auto raw_socket_ptr(std::dynamic_pointer_cast<Socket>(socket_ptr));
		if (!raw_socket_ptr)
			return false;

		raw_socket_ptr->force_shutdown();
		return this->del_object(raw_socket_ptr);
	}
	//restore the invalid socket whose id is equal to 'id', if successful, socket_ptr's take_over function will be invoked,
	//you can restore the invalid socket to socket_ptr, everything can be restored except socket::next_layer_ (on the other
	//hand, restore socket::next_layer_ doesn't make any sense).
	virtual bool restore_socket(const std::shared_ptr<tracked_executor>& socket_ptr, uint_fast64_t id, bool init)
	{
		auto raw_socket_ptr(std::dynamic_pointer_cast<Socket>(socket_ptr));
		if (!raw_socket_ptr)
			return false;

		auto this_id = raw_socket_ptr->id();
		if (!init)
		{
			auto old_socket_ptr = this->change_object_id(raw_socket_ptr, id);
			if (old_socket_ptr)
			{
				assert(raw_socket_ptr->id() == old_socket_ptr->id());

				unified_out::info_out("object id " ASCS_LLF " been reused, id " ASCS_LLF " been discarded.", raw_socket_ptr->id(), this_id);
				raw_socket_ptr->take_over(old_socket_ptr);

				return true;
			}
		}
		else if (this->init_object_id(raw_socket_ptr, id))
		{
			unified_out::info_out("object id " ASCS_LLF " been set to " ASCS_LLF, this_id, raw_socket_ptr->id());
			return true;
		}

		return false;
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means putting the msg into tcp::socket_base's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function.
	void disconnect(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {this->do_something_to_all([&](typename Pool::object_ctype& item) {item->disconnect();});}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->force_shutdown();}
	void force_shutdown() {this->do_something_to_all([&](typename Pool::object_ctype& item) {item->force_shutdown();});}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr, bool sync = false) {this->del_object(socket_ptr); socket_ptr->graceful_shutdown(sync);}
	void graceful_shutdown() {this->do_something_to_all([](typename Pool::object_ctype& item) {item->graceful_shutdown();});}
	//for the last function, parameter sync must be false (the default value), or dead lock will occur.

protected:
	virtual int async_accept_num() {return ASCS_ASYNC_ACCEPT_NUM;}
	virtual bool init() {return start_listen() ? (this->start(), true) : false;}
	virtual void uninit() {this->stop(); stop_listen(); force_shutdown();} //if you wanna graceful shutdown, call graceful_shutdown before stop_service.

	virtual bool on_accept(typename Pool::object_ctype& socket_ptr) {return true;}
	virtual void start_next_accept() {std::lock_guard<std::mutex> lock(mutex); do_async_accept(create_object());}

	//if you want to ignore this error and continue to accept new connections immediately, return true in this virtual function;
	//if you want to ignore this error and continue to accept new connections after a specific delay, start a timer immediately and return false
	// (don't call stop_listen()), after the timer exhausts, call start_next_accept() in the callback function.
	//otherwise, don't rewrite this virtual function or call generic_server::on_accept_error() directly after your code.
	virtual bool on_accept_error(const asio::error_code& ec, typename Pool::object_ctype& socket_ptr)
	{
		if (asio::error::operation_aborted != ec)
		{
			unified_out::error_out("failed to accept new connection because of %s, will stop listening.", ec.message().data());
			stop_listen();
		}

		return false;
	}

protected:
	typename Pool::object_type create_object() {return Pool::create_object(*this);}
	template<typename Arg> typename Pool::object_type create_object(Arg&& arg) {return Pool::create_object(*this, std::forward<Arg>(arg));}

	bool add_socket(typename Pool::object_ctype& socket_ptr)
	{
		if (this->add_object(socket_ptr))
		{
			socket_ptr->show_info("client:", "arrive.");
			if (get_service_pump().is_service_started()) //service already started
				socket_ptr->start();

			return true;
		}

		socket_ptr->show_info("client:", "been refused because of too many clients or id conflict.");
		return false;
	}

private:
	//call following two functions (via timer object's add_io_context_refs, sub_io_context_refs or clear_io_context_refs) in:
	// 1. on_xxxx callbacks on this object;
	// 2. use this->post or this->set_timer to emit an async event, then in the callback.
	//otherwise, you must protect them to not be called with reset and on_close simultaneously
	virtual void attach_io_context(asio::io_context& io_context_, unsigned refs) {get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(asio::io_context& io_context_, unsigned refs) {get_service_pump().return_io_context(io_context_, refs);}

	void accept_handler(const asio::error_code& ec, typename Pool::object_ctype& socket_ptr)
	{
		if (!ec)
		{
			if (on_accept(socket_ptr))
				add_socket(socket_ptr);

			if (is_listening())
				start_next_accept();
		}
		else if (on_accept_error(ec, socket_ptr))
			start_next_accept();
	}

	void do_async_accept(typename Pool::object_ctype& socket_ptr)
		{if (socket_ptr) acceptor.async_accept(socket_ptr->lowest_layer(), ASCS_COPY_ALL_AND_THIS(const asio::error_code& ec) {this->accept_handler(ec, socket_ptr);});}

private:
	typename Family::endpoint server_addr;
	typename Family::acceptor acceptor;
	unsigned io_context_refs;
	std::mutex mutex;
	bool listening;
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = i_server>
class server_base : public generic_server<Socket, asio::ip::tcp, Pool, Server>
{
private:
	typedef generic_server<Socket, asio::ip::tcp, Pool, Server> super;

public:
	server_base(service_pump& service_pump_) : super(service_pump_) {this->set_server_addr(ASCS_SERVER_PORT);}
	template<typename Arg> server_base(service_pump& service_pump_, Arg&& arg) : super(service_pump_, std::forward<Arg>(arg)) {this->set_server_addr(ASCS_SERVER_PORT);}
};

#ifdef ASIO_HAS_LOCAL_SOCKETS
template<typename Socket, typename Pool = object_pool<Socket>, typename Server = i_server>
class unix_server_base : public generic_server<Socket, asio::local::stream_protocol, Pool, Server>
{
private:
	typedef generic_server<Socket, asio::local::stream_protocol, Pool, Server> super;

public:
	unix_server_base(service_pump& service_pump_) : super(service_pump_) {this->set_server_addr("./ascs-unix-socket");}
};
#endif

}} //namespace

#endif /* _ASCS_SERVER_H_ */
