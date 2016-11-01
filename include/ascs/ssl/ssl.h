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
	#error asio::ssl::stream not support reusing!
#endif

namespace ascs { namespace ssl {

template <typename Packer, typename Unpacker, typename Socket = asio::ssl::stream<asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class connector_base : public tcp::connector_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
protected:
	typedef tcp::connector_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

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
			unified_out::error_out("asio::ssl::stream not support reconnecting!");

		if (!shutdown_ssl())
			super::force_shutdown(false);
	}

	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (reconnect)
			unified_out::error_out("asio::ssl::stream not support reconnecting!");

		if (!shutdown_ssl())
			super::graceful_shutdown(false, sync);
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!this->stopped())
		{
			if (this->reconnecting && !this->is_connected())
				this->lowest_layer().async_connect(this->server_addr, this->make_handler_error([this](const auto& ec) {this->connect_handler(ec);}));
			else if (!authorized_)
				this->next_layer().async_handshake(asio::ssl::stream_base::client, this->make_handler_error([this](const auto& ec) {this->handshake_handler(ec);}));
			else
				this->do_recv_msg();

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
		if (!this->is_shutting_down() && authorized_)
		{
			this->show_info("ssl client link:", "been shut down.");
			this->shutdown_state = super::shutdown_states::GRACEFUL;
			this->reconnecting = false;
			authorized_ = false;

			asio::error_code ec;
			this->next_layer().shutdown(ec);

			re = !ec;
		}

		return re;
	}

private:
	void connect_handler(const asio::error_code& ec)
	{
		if (!ec)
		{
			this->connected = this->reconnecting = false;
			this->reset_state();
			this->on_connect();
			do_start();
		}
		else
			this->prepare_next_reconnect(ec);
	}

	void handshake_handler(const asio::error_code& ec)
	{
		on_handshake(ec);
		if (!ec)
		{
			authorized_ = true;
			this->send_msg(); //send buffer may have msgs, send them
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
	asio::ssl::context& context() {return ctx;}

	using super::create_object;
	typename object_pool::object_type create_object() {return create_object(this->sp, ctx);}
	template<typename Arg>
	typename object_pool::object_type create_object(Arg& arg) {return create_object(arg, ctx);}

protected:
	asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = asio::ssl::stream<asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
using server_socket_base = tcp::server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>;

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
		auto client_ptr = this->create_object(*this);
		this->acceptor.async_accept(client_ptr->lowest_layer(), [client_ptr, this](const auto& ec) {this->accept_handler(ec, client_ptr);});
	}

private:
	void accept_handler(const asio::error_code& ec, typename server_base::object_ctype& client_ptr)
	{
		if (!ec)
		{
			if (this->on_accept(client_ptr))
				client_ptr->next_layer().async_handshake(asio::ssl::stream_base::server, [client_ptr, this](const auto& ec) {this->handshake_handler(ec, client_ptr);});

			start_next_accept();
		}
		else
			this->stop_listen();
	}

	void handshake_handler(const asio::error_code& ec, typename server_base::object_ctype& client_ptr)
	{
		this->on_handshake(ec, client_ptr);
		if (!ec && this->add_client(client_ptr))
			client_ptr->start();
	}
};

}} //namespace

#endif /* _ASICS_SSL_H_ */
