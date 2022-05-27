/*
 * ssl.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make ascs support ssl (based on asio::ssl)
 */

#ifndef _ASCS_SSL_H_
#define _ASCS_SSL_H_

#include <asio/ssl.hpp>

#include "../client.h"
#include "../server.h"
#include "../client_socket.h"
#include "../server_socket.h"
#include "../../object_pool.h"

namespace ascs { namespace ssl {

template<typename Socket> class socket : public Socket
{
public:
	template<typename Arg> socket(Arg&& arg, asio::ssl::context& ctx_) : Socket(std::forward<Arg>(arg), ctx_), ctx(ctx_) {}

public:
	virtual void reset() {this->reset_next_layer(ctx); Socket::reset();}

	asio::ssl::context& get_context() {return ctx;}

protected:
	virtual void on_recv_error(const asio::error_code& ec)
	{
		shutdown_ssl(true);
		Socket::on_recv_error(ec);
	}

	virtual void on_handshake(const asio::error_code& ec)
	{
		if (!ec)
			unified_out::info_out(ASCS_LLF " handshake success.", this->id());
		else
			unified_out::error_out(ASCS_LLF " handshake failed: %s", this->id(), ec.message().data());
	}

	void shutdown_ssl(bool sync = true)
	{
		if (!this->is_ready())
			return;

		this->status = Socket::link_status::GRACEFUL_SHUTTING_DOWN;
		if (!sync)
		{
			this->show_info("ssl link:", "been shutting down.");
			this->next_layer().async_shutdown(this->make_handler_error([this](const asio::error_code& ec) {
				if (ec && asio::error::eof != ec) //the endpoint who initiated a shutdown operation will get error eof.
					unified_out::info_out(ASCS_LLF " async shutdown ssl link failed (maybe intentionally because of reusing)", this->id());
			}));
		}
		else
		{
			this->show_info("ssl link:", "been shut down.");

			asio::error_code ec;
			this->next_layer().shutdown(ec);
			if (ec && asio::error::eof != ec) //the endpoint who initiated a shutdown operation will get error eof.
				unified_out::info_out(ASCS_LLF " shutdown ssl link failed: %s", this->id(), ec.message().data());
		}
	}

private:
	asio::ssl::context& ctx;
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class client_socket_base : public socket<tcp::client_socket_base<Packer, Unpacker, Matrix, asio::ssl::stream<asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket<tcp::client_socket_base<Packer, Unpacker, Matrix, asio::ssl::stream<asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer>> super;

public:
	client_socket_base(asio::io_context& io_context_, asio::ssl::context& ctx_) : super(io_context_, ctx_) {}
	client_socket_base(Matrix& matrix_, asio::ssl::context& ctx_) : super(matrix_, ctx_) {}

	virtual const char* type_name() const {return "SSL (client endpoint)";}
	virtual int type_id() const {return 3;}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false) {graceful_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (this->is_ready())
		{
			this->set_reconnect(reconnect);
			shutdown_ssl(sync);
		}
		else
			super::force_shutdown(reconnect);
	}

protected:
	virtual void on_unpack_error() {unified_out::info_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown(this->is_reconnect());}
	virtual void after_close()
	{
		if (this->is_reconnect())
			this->reset_next_layer(this->get_context());

		super::after_close();
	}

private:
	virtual void connect_handler(const asio::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (!ec)
		{
			this->status = super::link_status::HANDSHAKING;
			this->next_layer().async_handshake(asio::ssl::stream_base::client, this->make_handler_error([this](const asio::error_code& ec) {this->handle_handshake(ec);}));
		}
		else
			super::connect_handler(ec);
	}

	void handle_handshake(const asio::error_code& ec)
	{
		this->on_handshake(ec);

		if (!ec)
			super::connect_handler(ec); //return to tcp::client_socket_base::connect_handler
		else
			this->force_shutdown();
	}

	using super::shutdown_ssl;
};

template<typename Object>
class object_pool : public ascs::object_pool<Object>
{
private:
	typedef ascs::object_pool<Object> super;

public:
	object_pool(service_pump& service_pump_, asio::ssl::context::method&& m) : super(service_pump_), ctx(std::forward<asio::ssl::context::method>(m)) {}
	asio::ssl::context& context() {return ctx;}

protected:
	template<typename Arg> typename object_pool::object_type create_object(Arg&& arg) {return super::create_object(std::forward<Arg>(arg), ctx);}

private:
	asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = tcp::i_server,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class server_socket_base : public socket<tcp::server_socket_base<Packer, Unpacker, Server, asio::ssl::stream<asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket<tcp::server_socket_base<Packer, Unpacker, Server, asio::ssl::stream<asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer>> super;

public:
	server_socket_base(Server& server_, asio::ssl::context& ctx_) : super(server_, ctx_) {}

	virtual const char* type_name() const {return "SSL (server endpoint)";}
	virtual int type_id() const {return 4;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();} //must with async mode (the default value), because server_base::uninit will call this function
	void graceful_shutdown(bool sync = false) {if (this->is_ready()) shutdown_ssl(sync); else super::force_shutdown();}

protected:
	virtual bool do_start() //intercept tcp::server_socket_base::do_start (to add handshake)
	{
		this->status = super::link_status::HANDSHAKING;
		this->next_layer().async_handshake(asio::ssl::stream_base::server, this->make_handler_error([this](const asio::error_code& ec) {this->handle_handshake(ec);}));
		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); this->force_shutdown();}

private:
	void handle_handshake(const asio::error_code& ec)
	{
		this->on_handshake(ec);

		if (!ec)
			super::do_start(); //return to tcp::server_socket_base::do_start
		else
			this->get_server().del_socket(this->shared_from_this());
	}

	using super::shutdown_ssl;
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = tcp::i_server> using server_base = tcp::server_base<Socket, Pool, Server>;
template<typename Socket> class single_client_base : public tcp::single_client_base<Socket>
{
private:
	typedef tcp::single_client_base<Socket> super;

public:
	single_client_base(service_pump& service_pump_, asio::ssl::context& ctx_) : super(service_pump_, ctx_) {}

protected:
	virtual bool init() {if (0 == this->get_io_context_refs()) this->reset_next_layer(this->get_context()); return super::init();}
};
template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix> using multi_client_base = tcp::multi_client_base<Socket, Pool, Matrix>;

}} //namespace

#endif /* _ASCS_SSL_H_ */
