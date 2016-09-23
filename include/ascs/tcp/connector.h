/*
 * sconnector.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef _ASCS_CONNECTOR_H_
#define _ASCS_CONNECTOR_H_

#include "socket.h"

namespace ascs { namespace tcp {

template <typename Packer, typename Unpacker, typename Socket = asio::ip::tcp::socket>
class connector_base : public socket_base<Socket, Packer, Unpacker>
{
protected:
	typedef socket_base<Socket, Packer, Unpacker> super;

public:
	static const unsigned char TIMER_BEGIN = super::TIMER_END;
	static const unsigned char TIMER_CONNECT = TIMER_BEGIN;
	static const unsigned char TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN + 1;
	static const unsigned char TIMER_END = TIMER_BEGIN + 10;

	connector_base(asio::io_service& io_service_) : super(io_service_), connected(false), reconnecting(true)
		{set_server_addr(ASCS_SERVER_PORT, ASCS_SERVER_IP);}

	template<typename Arg>
	connector_base(asio::io_service& io_service_, Arg& arg) : super(io_service_, arg), connected(false), reconnecting(true)
		{set_server_addr(ASCS_SERVER_PORT, ASCS_SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this connector_base when invoke it
	//notice, when reusing this connector_base, object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke connector_base::reset() to initialize father's member variables
	virtual void reset() {connected = false; reconnecting = true; super::reset();}
	virtual bool obsoleted() {return !reconnecting && super::obsoleted();}

	bool set_server_addr(unsigned short port, const std::string& ip = ASCS_SERVER_IP)
	{
		asio::error_code ec;
		auto addr = asio::ip::address::from_string(ip, ec);
		if (ec)
			return false;

		server_addr = asio::ip::tcp::endpoint(addr, port);
		return true;
	}
	const asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	bool is_connected() const {return connected;}

	//if the connection is broken unexpectedly, connector_base will try to reconnect to the server automatically.
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (1 != ASCS_THIS shutdown_state)
		{
			show_info("client link:", "been shut down.");
			reconnecting = reconnect;
			connected = false;
		}

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (ASCS_THIS is_shutting_down())
			return;
		else if (!is_connected())
			return force_shutdown(reconnect);

		show_info("client link:", "being shut down gracefully.");
		reconnecting = reconnect;
		connected = false;

		if (super::graceful_shutdown(sync))
			ASCS_THIS set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](auto id)->bool {return ASCS_THIS async_shutdown_handler(id, ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
	}

	void show_info(const char* head, const char* tail) const
	{
		asio::error_code ec;
		auto ep = ASCS_THIS lowest_layer().local_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const asio::error_code& ec) const
	{
		asio::error_code ec2;
		auto ep = ASCS_THIS lowest_layer().local_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().c_str(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ASCS_THIS stopped())
		{
			if (reconnecting && !is_connected())
				ASCS_THIS lowest_layer().async_connect(server_addr, ASCS_THIS make_handler_error([this](const auto& ec) {ASCS_THIS connect_handler(ec);}));
			else
				ASCS_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	//after how much time(ms), connector_base will try to reconnect to the server, negative means give up.
	virtual int prepare_reconnect(const asio::error_code& ec) {return ASCS_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_closable() {return !reconnecting;}
	virtual bool is_send_allowed() {return is_connected() && super::is_send_allowed();}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}
	virtual void on_recv_error(const asio::error_code& ec)
	{
		show_info("client link:", "broken/been shut down", ec);

		force_shutdown(ASCS_THIS is_shutting_down() ? reconnecting : prepare_reconnect(ec) >= 0);
		ASCS_THIS shutdown_state = 0;

		if (reconnecting)
			ASCS_THIS start();
	}

	bool prepare_next_reconnect(const asio::error_code& ec)
	{
		if ((asio::error::operation_aborted != ec || reconnecting) && !ASCS_THIS stopped())
		{
#ifdef _WIN32
			if (asio::error::connection_refused != ec && asio::error::network_unreachable != ec && asio::error::timed_out != ec)
#endif
			{
				asio::error_code ec;
				ASCS_THIS lowest_layer().close(ec);
			}

			auto delay = prepare_reconnect(ec);
			if (delay >= 0)
			{
				ASCS_THIS set_timer(TIMER_CONNECT, delay, [this](auto id)->bool {ASCS_THIS do_start(); return false;});
				return true;
			}
		}

		return false;
	}

private:
	bool async_shutdown_handler(unsigned char id, ssize_t loop_num)
	{
		assert(TIMER_ASYNC_SHUTDOWN == id);

		if (2 == ASCS_THIS shutdown_state)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ASCS_THIS update_timer_info(id, 10, [loop_num, this](auto id)->bool {return ASCS_THIS async_shutdown_handler(id, loop_num);});
				return true;
			}
			else
			{
				unified_out::info_out("failed to graceful shutdown within %d seconds", ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION);
				force_shutdown(reconnecting);
			}
		}

		return false;
	}

	void connect_handler(const asio::error_code& ec)
	{
		if (!ec)
		{
			connected = reconnecting = true;
			ASCS_THIS reset_state();
			on_connect();
			ASCS_THIS send_msg(); //send buffer may have msgs, send them
			do_start();
		}
		else
			prepare_next_reconnect(ec);
	}

protected:
	asio::ip::tcp::endpoint server_addr;
	bool connected;
	bool reconnecting;
};

}} //namespace

#endif /* _ASCS_CONNECTOR_H_ */
