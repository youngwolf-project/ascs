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

template<typename Socket, typename Server = i_server> class generic_server_socket : public Socket, public std::enable_shared_from_this<generic_server_socket<Socket, Server>>
{
public:
	typedef generic_server_socket<Socket, Server> type_of_object_restore;

private:
	typedef Socket super;

public:
	generic_server_socket(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg> generic_server_socket(Server& server_, Arg&& arg) : super(server_.get_service_pump(), std::forward<Arg>(arg)), server(server_) {}
	~generic_server_socket() {this->clear_io_context_refs();}

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

	//this function is not thread safe, please note.
	void graceful_shutdown()
	{
		if (this->is_broken())
			return force_shutdown();
		else if (!this->is_shutting_down())
			this->show_info("server link:", "being shut down gracefully.");

		super::graceful_shutdown();
	}

protected:
	Server& get_server() {return server;}
	const Server& get_server() const {return server;}

	virtual void on_unpack_error() {unified_out::error_out(ASCS_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown();}
	virtual void on_recv_error(const asio::error_code& ec) {this->show_info(ec, "server link:", "broken/been shut down"); force_shutdown();}
	virtual void on_async_shutdown_error() {force_shutdown();}
	virtual bool on_heartbeat_error() {this->show_info("server link:", "broke unexpectedly."); force_shutdown(); return false;}

	virtual void on_close()
	{
		this->clear_io_context_refs();
#ifndef ASCS_CLEAR_OBJECT_INTERVAL
		server.del_socket(this->shared_from_this(), false);
#endif
		super::on_close();
	}

private:
	//call following two functions (via timer object's add_io_context_refs, sub_io_context_refs or clear_io_context_refs) in:
	// 1. on_xxxx callbacks on this object;
	// 2. use this->post or this->set_timer to emit an async event, then in the callback.
	//otherwise, you must protect them to not be called with reset and on_close simultaneously
	virtual void attach_io_context(asio::io_context& io_context_, unsigned refs) {server.get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(asio::io_context& io_context_, unsigned refs) {server.get_service_pump().return_io_context(io_context_, refs);}

private:
	Server& server;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = asio::ip::tcp::socket,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
using server_socket_base = generic_server_socket<socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Server>;

#ifdef ASIO_HAS_LOCAL_SOCKETS
template<typename Packer, typename Unpacker, typename Server = i_server,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
using unix_server_socket_base = generic_server_socket<socket_base<asio::local::stream_protocol::socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Server>;
#endif

}} //namespace

#endif /* _ASCS_SERVER_SOCKET_H_ */
