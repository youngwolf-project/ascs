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
	virtual void on_handshake(const asio::error_code& ec)
	{
		if (!ec)
			this->show_info(nullptr, "handshake success.");
		else
			this->show_info(ec, nullptr, "handshake failed");
	}
	virtual void on_recv_error(const asio::error_code& ec) {this->stop_graceful_shutdown_monitoring(); Socket::on_recv_error(ec);}

	void shutdown_ssl()
	{
		this->status = Socket::link_status::GRACEFUL_SHUTTING_DOWN;
		this->dispatch_in_io_strand([this]() {
			this->show_info("ssl link:", "been shutting down.");
			this->start_graceful_shutdown_monitoring();
			this->next_layer().async_shutdown(this->make_handler_error([this](const asio::error_code& ec) {
				//do not stop the shutdown monitoring at here, sometimes, this async_shutdown cannot trigger on_recv_error,
				//very strange, maybe, there's a bug in Asio. so we stop it in on_recv_error.
				//this->stop_graceful_shutdown_monitoring();
				if (ec)
					this->show_info(ec, "ssl link", "async shutdown failed");
			}));
		});
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
	using super::super;

	virtual const char* type_name() const {return "SSL (client endpoint)";}
	virtual int type_id() const {return 3;}

	//these functions are not thread safe, please note.
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false) {graceful_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false)
	{
		if (this->is_ready())
		{
			this->set_reconnect(reconnect);
			shutdown_ssl();
		}
		else
			super::force_shutdown(reconnect);
	}

protected:
	virtual void on_unpack_error() {unified_out::info_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown(this->is_reconnect());}
	virtual void on_close()
	{
		this->reset_next_layer(this->get_context());
		super::on_close();
	}

private:
	virtual void connect_handler(const asio::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (!ec)
		{
			this->status = super::link_status::HANDSHAKING;
			this->next_layer().async_handshake(asio::ssl::stream_base::client, this->make_handler_error([this](const asio::error_code& ec) {handle_handshake(ec);}));
		}
		else
			super::connect_handler(ec);
	}

	void handle_handshake(const asio::error_code& ec)
	{
		this->on_handshake(ec);
		ec ? this->force_shutdown() : super::connect_handler(ec); //return to tcp::client_socket_base::connect_handler
	}

	using super::shutdown_ssl;
};

template<typename Object>
class object_pool : public ascs::object_pool<Object>
{
private:
	typedef ascs::object_pool<Object> super;

public:
	object_pool(service_pump& service_pump_, const asio::ssl::context::method& m) : super(service_pump_), ctx(m) {}
	asio::ssl::context& context() {return ctx;}

protected:
	template<typename Arg> typename super::object_type create_object(Arg&& arg) {return super::create_object(std::forward<Arg>(arg), ctx);}

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
	using super::super;

	virtual const char* type_name() const {return "SSL (server endpoint)";}
	virtual int type_id() const {return 4;}

	//these functions are not thread safe, please note.
	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();}
	void graceful_shutdown() {if (this->is_ready()) shutdown_ssl(); else super::force_shutdown();}

protected:
	virtual bool do_start() //intercept tcp::server_socket_base::do_start (to add handshake)
	{
		this->status = super::link_status::HANDSHAKING;
		this->next_layer().async_handshake(asio::ssl::stream_base::server, this->make_handler_error([this](const asio::error_code& ec) {handle_handshake(ec);}));
		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown();}

private:
	void handle_handshake(const asio::error_code& ec)
	{
		this->on_handshake(ec);
		ec ? this->get_server().del_socket(this->shared_from_this()) : super::do_start(); //return to tcp::server_socket_base::do_start
	}

	using super::shutdown_ssl;
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = tcp::i_server> using server_base = tcp::server_base<Socket, Pool, Server>;
template<typename Socket> class single_client_base : public tcp::single_client_base<Socket>
{
private:
	typedef tcp::single_client_base<Socket> super;

public:
	template<typename Arg> single_client_base(service_pump& service_pump_, Arg&& arg) : super(service_pump_, *arg), ctx_holder(std::forward<Arg>(arg)) {}

protected:
	virtual bool init() {if (0 == this->get_io_context_refs()) this->reset_next_layer(this->get_context()); return super::init();}

private:
	std::shared_ptr<asio::ssl::context> ctx_holder;
};
template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix> using multi_client_base = tcp::multi_client_base<Socket, Pool, Matrix>;

}} //namespace

#endif /* _ASCS_SSL_H_ */
