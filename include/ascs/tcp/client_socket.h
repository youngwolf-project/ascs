/*
 * client_socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef _ASCS_CLIENT_SOCKET_H_
#define _ASCS_CLIENT_SOCKET_H_

#include "socket.h"

namespace ascs { namespace tcp {

template<typename Socket, typename Matrix = i_matrix, typename Family = asio::ip::tcp> class generic_client_socket : public Socket
{
private:
	typedef Socket super;

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_CONNECT = TIMER_BEGIN;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 5;

	static bool set_addr(asio::ip::tcp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = asio::ip::tcp::endpoint(ASCS_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			asio::error_code ec;
#if ASIO_VERSION >= 101100
			auto addr = asio::ip::make_address(ip, ec); assert(!ec);
#else
			auto addr = asio::ip::address::from_string(ip, ec); assert(!ec);
#endif
			if (ec)
			{
				unified_out::error_out("invalid IP address %s.", ip.data());
				return false;
			}

			endpoint = asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}

protected:
	generic_client_socket(asio::io_context& io_context_) : super(io_context_) {first_init();}
	template<typename Arg> generic_client_socket(asio::io_context& io_context_, Arg&& arg) : super(io_context_, std::forward<Arg>(arg)) {first_init();}

	generic_client_socket(Matrix& matrix_) : super(matrix_.get_service_pump()) {first_init(&matrix_);}
	template<typename Arg> generic_client_socket(Matrix& matrix_, Arg&& arg) : super(matrix_.get_service_pump(), std::forward<Arg>(arg)) {first_init(&matrix_);}

	~generic_client_socket() {this->clear_io_context_refs();}

public:
	virtual const char* type_name() const {return "TCP (client endpoint)";}
	virtual int type_id() const {return 1;}

	virtual void reset() {need_reconnect = ASCS_RECONNECT; super::reset();}

	bool set_server_addr(unsigned short port, const std::string& ip = ASCS_SERVER_IP) {return set_addr(server_addr, port, ip);}
	bool set_server_addr(const std::string& file_name) {server_addr = typename Family::endpoint(file_name); return true;}
	const typename Family::endpoint& get_server_addr() const {return server_addr;}

	//if you don't want to reconnect to the server after link broken, define macro ASCS_RECONNECT as false, call set_reconnect(false) in on_connect()
	// or rewrite after_close() virtual function and do nothing in it.
	//if you want to control the retry times and delay time after reconnecting failed, rewrite prepare_reconnect virtual function.
	//disconnect(bool), force_shutdown(bool) and graceful_shutdown(bool, bool) can overwrite reconnecting behavior, please note.
	//reset() virtual function will set reconnecting behavior according to macro ASCS_RECONNECT, please note.
	//if prepare_reconnect returns negative value, reconnecting will be closed, please note.
	void set_reconnect(bool reconnect) {need_reconnect = reconnect;}
	bool is_reconnect() const {return need_reconnect;}

	//if the connection is broken unexpectedly, generic_client_socket will try to reconnect to the server automatically (if need_reconnect is true).
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		need_reconnect = reconnect;

		if (reconnect && this->is_broken() && !this->started())
			return this->start();
		else if (super::link_status::FORCE_SHUTTING_DOWN != this->status)
			this->show_info("client link:", "been shut down.");

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		need_reconnect = reconnect;

		if (reconnect && this->is_broken() && !this->started())
			return this->start();
		else if (this->is_broken())
			return force_shutdown(reconnect);
		else if (!this->is_shutting_down())
			this->show_info("client link:", "being shut down gracefully.");

		super::graceful_shutdown(sync);
	}

protected:
	//helper function, just call it in constructor
	void first_init(Matrix* matrix_ = nullptr) {need_reconnect = ASCS_RECONNECT; matrix = matrix_;}

	Matrix* get_matrix() {return matrix;}
	const Matrix* get_matrix() const {return matrix;}

	virtual bool do_start() //connect
	{
		assert(!this->is_connected());
		return bind() && this->set_timer(TIMER_CONNECT, 50, [this](typename super::tid id)->bool {this->connect(true); return false;});
	}

	virtual void connect_handler(const asio::error_code& ec)
	{
		if (!ec) //already started, so cannot call start()
			super::do_start();
		else
			prepare_next_reconnect(ec);
	}

	//after how much time (ms), generic_client_socket will try to reconnect the server, negative value means give up.
	virtual int prepare_reconnect(const asio::error_code& ec) {return ASCS_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out(ASCS_LLF " connecting success.", this->id());}
	virtual void on_unpack_error() {unified_out::info_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown(need_reconnect);}
	virtual void on_recv_error(const asio::error_code& ec)
	{
		this->show_info(ec, "client link:", "broken/been shut down");

		force_shutdown(need_reconnect);
		this->status = super::link_status::BROKEN;

#ifndef ASCS_CLEAR_OBJECT_INTERVAL
		if (!need_reconnect && nullptr != matrix)
			matrix->del_socket(this->id());
#endif
	}

	virtual void on_async_shutdown_error() {force_shutdown(need_reconnect);}
	virtual bool on_heartbeat_error()
	{
		this->show_info("client link:", "broke unexpectedly.");

		force_shutdown(need_reconnect);
		return false;
	}

	virtual void on_close()
	{
		if (!need_reconnect)
			this->clear_io_context_refs();

		super::on_close();
	}

	//reconnect at here rather than in on_recv_error to make sure no async invocations performed on this socket before reconnecting.
	//if you don't want to reconnect the server after link broken, rewrite this virtual function and do nothing in it or call close_reconnt().
	//if you want to control the retry times and delay time after reconnecting failed, rewrite prepare_reconnect virtual function.
	virtual void after_close() {if (need_reconnect) this->start();}

	virtual bool bind() {return true;}

private:
	//call following two functions (via timer object's add_io_context_refs, sub_io_context_refs or clear_io_context_refs) in:
	// 1. on_xxxx callbacks on this object;
	// 2. use this->post or this->set_timer to emit an async event, then in the callback.
	//otherwise, you must protect them to not be called with reset and on_close simultaneously
	//actually, you're recommended to not use them, use add_socket instead.
	virtual void attach_io_context(asio::io_context& io_context_, unsigned refs) {if (nullptr != matrix) matrix->get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(asio::io_context& io_context_, unsigned refs) {if (nullptr != matrix) matrix->get_service_pump().return_io_context(io_context_, refs);}

	bool connect(bool first)
	{
		if (!first && !bind())
			return false;

		this->lowest_layer().async_connect(server_addr, this->make_handler_error([this](const asio::error_code& ec) {this->connect_handler(ec);}));
		return true;
	}

	bool prepare_next_reconnect(const asio::error_code& ec)
	{
		if (need_reconnect && this->started() && !this->stopped())
		{
#ifdef _WIN32
			if (asio::error::connection_refused != ec && asio::error::network_unreachable != ec && asio::error::timed_out != ec)
#endif
			{
				asio::error_code ec;
				this->lowest_layer().close(ec);
			}

			auto delay = prepare_reconnect(ec);
			if (delay < 0)
				need_reconnect = false;
			else if (this->set_timer(TIMER_CONNECT, delay, [this](typename super::tid id)->bool {this->connect(false); return false;}))
				return true;
		}

		unified_out::info_out(ASCS_LLF " reconnecting abandoned.", this->id());
		super::force_shutdown();
		return false;
	}

private:
	bool need_reconnect;
	typename Family::endpoint server_addr;

	Matrix* matrix;
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = asio::ip::tcp::socket,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class client_socket_base : public generic_client_socket<socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix>
{
private:
	typedef generic_client_socket<socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix> super;

public:
	client_socket_base(asio::io_context& io_context_) : super(io_context_) {this->set_server_addr(ASCS_SERVER_PORT, ASCS_SERVER_IP);}
	template<typename Arg>
	client_socket_base(asio::io_context& io_context_, Arg&& arg) : super(io_context_, std::forward<Arg>(arg)) {this->set_server_addr(ASCS_SERVER_PORT, ASCS_SERVER_IP);}

	client_socket_base(Matrix& matrix_) : super(matrix_) {this->set_server_addr(ASCS_SERVER_PORT, ASCS_SERVER_IP);}
	template<typename Arg> client_socket_base(Matrix& matrix_, Arg&& arg) : super(matrix_, std::forward<Arg>(arg)) {this->set_server_addr(ASCS_SERVER_PORT, ASCS_SERVER_IP);}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return super::set_addr(local_addr, port, ip);}
	const asio::ip::tcp::endpoint& get_local_addr() const {return local_addr;}

protected:
	virtual bool bind()
	{
		if (0 != local_addr.port() || !local_addr.address().is_unspecified())
		{
			auto& lowest_object = this->lowest_layer();

			asio::error_code ec;
			if (!lowest_object.is_open()) //user maybe has opened this socket (to set options for example)
			{
				lowest_object.open(local_addr.protocol(), ec); assert(!ec);
				if (ec)
				{
					unified_out::error_out("cannot create socket: %s", ec.message().data());
					return false;
				}
			}

			lowest_object.bind(local_addr, ec);
			if (ec && asio::error::invalid_argument != ec)
			{
				unified_out::error_out("cannot bind socket: %s", ec.message().data());
				return false;
			}
		}

		return true;
	}

private:
	asio::ip::tcp::endpoint local_addr;
};

#ifdef ASIO_HAS_LOCAL_SOCKETS
template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class unix_client_socket_base : public generic_client_socket<socket_base<asio::local::stream_protocol::socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix, asio::local::stream_protocol>
{
private:
	typedef generic_client_socket<socket_base<asio::local::stream_protocol::socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix, asio::local::stream_protocol> super;

public:
	unix_client_socket_base(asio::io_context& io_context_) : super(io_context_) {this->set_server_addr("./ascs-unix-socket");}
	unix_client_socket_base(Matrix& matrix_) : super(matrix_) {this->set_server_addr("./ascs-unix-socket");}
};
#endif

}} //namespace

#endif /* _ASCS_CLIENT_SOCKET_H_ */
