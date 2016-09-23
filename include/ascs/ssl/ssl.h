/*
 * ssl.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make ascs support asio::ssl
 */

#ifndef _ASICS_SSL_H_
#define _ASICS_SSL_H_

#include <asio/ssl.hpp>

#include "../object_pool.h"
#include "../tcp/connector.h"
#include "../tcp/client.h"
#include "../tcp/server_socket.h"
#include "../tcp/server.h"

#ifdef ASCS_REUSE_OBJECT
	#error asio::ssl::stream not support reuse!
#endif

namespace ascs { namespace ssl {

template <typename Packer, typename Unpacker, typename Socket = asio::ssl::stream<asio::ip::tcp::socket>>
class connector_base : public tcp::connector_base<Packer, Unpacker, Socket>
{
protected:
	typedef tcp::connector_base<Packer, Unpacker, Socket> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	connector_base(asio::io_service& io_service_, asio::ssl::context& ctx) : super(io_service_, ctx), authorized_(false) {}

	virtual void reset() {authorized_ = false; super::reset();}
	bool authorized() const {return authorized_;}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (reconnect)
			unified_out::error_out("asio::ssl::stream not support reuse!");

		if (!shutdown_ssl())
			super::force_shutdown(false);
	}

	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (reconnect)
			unified_out::error_out("asio::ssl::stream not support reuse!");

		if (!shutdown_ssl())
			super::graceful_shutdown(false, sync);
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ASCS_THIS stopped())
		{
			if (ASCS_THIS reconnecting && !ASCS_THIS is_connected())
				ASCS_THIS lowest_layer().async_connect(ASCS_THIS server_addr, ASCS_THIS make_handler_error([this](const auto& ec) {ASCS_THIS connect_handler(ec);}));
			else if (!authorized_)
				ASCS_THIS next_layer().async_handshake(asio::ssl::stream_base::client, ASCS_THIS make_handler_error([this](const auto& ec) {ASCS_THIS handshake_handler(ec);}));
			else
				ASCS_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {authorized_ = false; super::on_unpack_error();}
	virtual void on_recv_error(const asio::error_code& ec) {authorized_ = false; super::on_recv_error(ec);}
	virtual void on_handshake(const asio::error_code& ec)
	{
		if (!ec)
			unified_out::info_out("handshake success.");
		else
			unified_out::error_out("handshake failed: %s", ec.message().data());
	}
	virtual bool is_send_allowed() {return authorized() && super::is_send_allowed();}

	bool shutdown_ssl()
	{
		bool re = false;
		if (!ASCS_THIS is_shutting_down() && authorized_)
		{
			ASCS_THIS show_info("ssl client link:", "been shut down.");
			ASCS_THIS shutdown_state = 2;
			ASCS_THIS reconnecting = false;
			authorized_ = false;

			asio::error_code ec;
			ASCS_THIS next_layer().shutdown(ec);

			re = !ec;
		}

		return re;
	}

private:
	void connect_handler(const asio::error_code& ec)
	{
		if (!ec)
		{
			ASCS_THIS connected = ASCS_THIS reconnecting = false;
			ASCS_THIS reset_state();
			ASCS_THIS on_connect();
			do_start();
		}
		else
			ASCS_THIS prepare_next_reconnect(ec);
	}

	void handshake_handler(const asio::error_code& ec)
	{
		on_handshake(ec);
		if (!ec)
		{
			authorized_ = true;
			ASCS_THIS send_msg(); //send buffer may have msgs, send them
			do_start();
		}
		else
			force_shutdown(false);
	}

protected:
	bool authorized_;
};

template<typename Object>
class object_pool : public ascs::object_pool<Object>
{
protected:
	typedef ascs::object_pool<Object> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	object_pool(service_pump& service_pump_, const asio::ssl::context::method& m) : super(service_pump_), ctx(m) {}
	asio::ssl::context& ssl_context() {return ctx;}

	using super::create_object;
	typename object_pool::object_type create_object() {return create_object(ASCS_THIS sp, ctx);}
	template<typename Arg>
	typename object_pool::object_type create_object(Arg& arg) {return create_object(arg, ctx);}

protected:
	asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = asio::ssl::stream<asio::ip::tcp::socket>>
using server_socket_base = tcp::server_socket_base<Packer, Unpacker, Server, Socket>;

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = i_server>
class server_base : public tcp::server_base<Socket, Pool, Server>
{
protected:
	typedef tcp::server_base<Socket, Pool, Server> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	server_base(service_pump& service_pump_, asio::ssl::context::method m) : super(service_pump_, m) {}

protected:
	virtual void on_handshake(const asio::error_code& ec, typename server_base::object_ctype& client_ptr)
	{
		if (!ec)
			client_ptr->show_info("handshake with", "success.");
		else
		{
			std::string error_info = "failed: ";
			error_info += ec.message().data();
			client_ptr->show_info("handshake with", error_info.data());
		}
	}

	virtual void start_next_accept()
	{
		auto client_ptr = ASCS_THIS create_object(*this);
		ASCS_THIS acceptor.async_accept(client_ptr->lowest_layer(), [client_ptr, this](const auto& ec) {ASCS_THIS accept_handler(ec, client_ptr);});
	}

private:
	void accept_handler(const asio::error_code& ec, typename server_base::object_ctype& client_ptr)
	{
		if (!ec)
		{
			if (ASCS_THIS on_accept(client_ptr))
				client_ptr->next_layer().async_handshake(asio::ssl::stream_base::server, [client_ptr, this](const auto& ec) {
					ASCS_THIS on_handshake(ec, client_ptr);
					if (!ec && ASCS_THIS add_client(client_ptr))
						client_ptr->start();
				});

			start_next_accept();
		}
		else
			ASCS_THIS stop_listen();
	}
};

}} //namespace

#endif /* _ASICS_SSL_H_ */
