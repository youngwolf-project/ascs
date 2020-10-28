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

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = asio::ip::tcp::socket, typename Family = asio::ip::tcp,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class generic_server_socket : public socket_base<Socket, Family, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>,
	public std::enable_shared_from_this<generic_server_socket<Packer, Unpacker, Server, Socket, Family, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket_base<Socket, Family, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	generic_server_socket(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg> generic_server_socket(Server& server_, Arg&& arg) : super(server_.get_service_pump(), std::forward<Arg>(arg)), server(server_) {}

	virtual const char* type_name() const {return "TCP (server endpoint)";}
	virtual int type_id() const {return 2;}

	virtual void take_over(std::shared_ptr<generic_server_socket> socket_ptr) {} //restore this socket from socket_ptr

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

	virtual void on_unpack_error() {unified_out::error_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown();}
	//do not forget to force_shutdown this socket(in del_socket(), there's a force_shutdown() invocation)
	virtual void on_recv_error(const asio::error_code& ec)
	{
		this->show_info(ec, "server link:", "broken/been shut down");

#ifdef ASCS_CLEAR_OBJECT_INTERVAL
		force_shutdown();
#else
		server.del_socket(this->shared_from_this());
		this->status = super::link_status::BROKEN;
#endif
	}

	virtual void on_async_shutdown_error() {force_shutdown();}
	virtual bool on_heartbeat_error() {this->show_info("server link:", "broke unexpectedly."); force_shutdown(); return false;}

private:
	Server& server;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = asio::ip::tcp::socket,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
using server_socket_base = generic_server_socket<Packer, Unpacker, Server, Socket, asio::ip::tcp, InQueue, InContainer, OutQueue, OutContainer>;

#ifdef ASIO_HAS_LOCAL_SOCKETS
template <typename Packer, typename Unpacker, typename Server = i_server,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
using unix_server_socket_base = generic_server_socket<Packer, Unpacker, Server, asio::local::stream_protocol::socket, asio::local::stream_protocol, InQueue, InContainer, OutQueue, OutContainer>;
#endif

}} //namespace

#endif /* _ASCS_SERVER_SOCKET_H_ */
