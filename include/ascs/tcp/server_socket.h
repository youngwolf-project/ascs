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
protected:
	typedef socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const timer::tid TIMER_BEGIN = super::TIMER_END;
	static const timer::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const timer::tid TIMER_END = TIMER_BEGIN + 10;

	server_socket_base(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg>
	server_socket_base(Server& server_, Arg& arg) : super(server_.get_service_pump(), arg), server(server_) {}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//please note, when reuse this socket, object_pool will invoke reset(), child must re-write it to initialize all member variables,
	//and then do not forget to invoke server_socket_base::reset() to initialize father's member variables
	virtual void reset() {super::reset();}

	void disconnect() {force_shutdown();}
	void force_shutdown()
	{
		if (super::shutdown_states::FORCE != this->shutdown_state)
			show_info("server link:", "been shut down.");

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool sync = false)
	{
		if (!this->is_shutting_down())
			show_info("server link:", "being shut down gracefully.");

		if (super::graceful_shutdown(sync))
			this->set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](auto id)->bool {return this->async_shutdown_handler(id, ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
	}

	void show_info(const char* head, const char* tail) const
	{
		asio::error_code ec;
		auto ep = this->lowest_layer().remote_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().data(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const asio::error_code& ec) const
	{
		asio::error_code ec2;
		auto ep = this->lowest_layer().remote_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().data(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start()
	{
		if (!this->stopped())
		{
			this->do_recv_msg();
			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); this->force_shutdown();}
	//do not forget to force_shutdown this socket(in del_client(), there's a force_shutdown() invocation)
	virtual void on_recv_error(const asio::error_code& ec)
	{
		this->show_info("server link:", "broken/been shut down", ec);

#ifdef ASCS_CLEAR_OBJECT_INTERVAL
		this->force_shutdown();
#else
		server.del_client(this->shared_from_this());
#endif
		this->shutdown_state = super::shutdown_states::NONE;
	}

private:
	bool async_shutdown_handler(timer::tid id, size_t loop_num)
	{
		assert(TIMER_ASYNC_SHUTDOWN == id);

		if (super::shutdown_states::GRACEFUL == this->shutdown_state)
		{
			--loop_num;
			if (loop_num > 0)
			{
				this->update_timer_info(id, 10, [loop_num, this](auto id)->bool {return this->async_shutdown_handler(id, loop_num);});
				return true;
			}
			else
			{
				unified_out::info_out("failed to graceful shutdown within %d seconds", ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION);
				force_shutdown();
			}
		}

		return false;
	}

protected:
	Server& server;
};

}} //namespace

#endif /* _ASCS_SERVER_SOCKET_H_ */
