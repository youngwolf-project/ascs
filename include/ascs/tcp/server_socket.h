/*
 * server_socket.h
 *
 *  Created on: 2013-4-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef _ASCS_SERVER_SOCKET_H_
#define _ASCS_SERVER_SOCKET_H_

#include "socket.h"

namespace ascs { namespace tcp {

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class server_socket_base : public socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>,
	public std::enable_shared_from_this<server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	server_socket_base(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg>
	server_socket_base(Server& server_, Arg& arg) : super(server_.get_service_pump(), arg), server(server_) {}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function
	virtual void reset() {super::reset();}
	virtual void take_over(std::shared_ptr<server_socket_base> socket_ptr) {} //restore this socket from socket_ptr

	void disconnect() {force_shutdown();}
	void force_shutdown()
	{
		if (super::link_status::FORCE_SHUTTING_DOWN != this->status)
			this->show_info("server link:", "been shut down.");

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool sync = false)
	{
		if (this->is_broken())
			return force_shutdown();
		else if (!this->is_shutting_down())
			this->show_info("server link:", "being shut down gracefully.");

		super::graceful_shutdown(sync);
	}

protected:
	Server& get_server() {return server;}
	const Server& get_server() const {return server;}

	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); force_shutdown();}
	//do not forget to force_shutdown this socket(in del_socket(), there's a force_shutdown() invocation)
	virtual void on_recv_error(const asio::error_code& ec)
	{
		this->show_info("server link:", "broken/been shut down", ec);

#ifdef ASCS_CLEAR_OBJECT_INTERVAL
		force_shutdown();
#else
		this->status = super::link_status::BROKEN;
		server.del_socket(this->shared_from_this());
#endif
	}

	virtual void on_async_shutdown_error() {force_shutdown();}
	virtual bool on_heartbeat_error() {this->show_info("server link:", "broke unexpectedly."); force_shutdown(); return false;}

private:
	Server& server;
};

}} //namespace

#endif /* _ASCS_SERVER_SOCKET_H_ */
