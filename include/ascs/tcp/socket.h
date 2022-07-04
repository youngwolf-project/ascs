/*
 * socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint
 */

#ifndef _ASCS_TCP_SOCKET_H_
#define _ASCS_TCP_SOCKET_H_

#include "../socket.h"

namespace ascs { namespace tcp {

template<typename Socket, typename OutMsgType> class reader_writer : public Socket
{
public:
	reader_writer(asio::io_context& io_context_) : Socket(io_context_) {}
	template<typename Arg> reader_writer(asio::io_context& io_context_, Arg&& arg) : Socket(io_context_, std::forward<Arg>(arg)) {}

	typedef std::function<void(const asio::error_code& ec, size_t bytes_transferred)> ReadWriteCallBack;

protected:
	bool async_read(ReadWriteCallBack&& call_back)
	{
		auto recv_buff = this->unpacker()->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);
		if (0 == asio::buffer_size(recv_buff))
		{
			unified_out::error_out(ASCS_LLF " the unpacker returned an empty buffer, quit receiving!", this->id());
			return false;
		}

		asio::async_read(this->next_layer(), recv_buff, [this](const asio::error_code& ec, size_t bytes_transferred)->size_t {
			return this->completion_checker(ec, bytes_transferred);}, std::forward<ReadWriteCallBack>(call_back));
		return true;
	}
	bool parse_msg(size_t bytes_transferred, std::list<OutMsgType>& msg_can) {return this->unpacker()->parse_msg(bytes_transferred, msg_can);}

	size_t batch_msg_send_size() const
	{
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
		return 0;
#else
		return asio::detail::default_max_transfer_size;
#endif
	}
	template<typename Buffer> void async_write(const Buffer& msg_can, ReadWriteCallBack&& call_back)
		{asio::async_write(this->next_layer(), msg_can, std::forward<ReadWriteCallBack>(call_back));}

private:
	size_t completion_checker(const asio::error_code& ec, size_t bytes_transferred)
	{
		auto_duration dur(this->stat.unpack_time_sum);
		return this->unpacker()->completion_condition(ec, bytes_transferred);
	}
};

template<typename Socket, typename Packer, typename Unpacker,
	template<typename> class InQueue, template<typename> class InContainer, template<typename> class OutQueue, template<typename> class OutContainer,
	template<typename, typename> class ReaderWriter = reader_writer>
class socket_base : public ReaderWriter<socket2<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>, typename Unpacker::msg_type>
{
public:
	typedef typename Packer::msg_type in_msg_type;
	typedef typename Packer::msg_ctype in_msg_ctype;
	typedef typename Unpacker::msg_type out_msg_type;
	typedef typename Unpacker::msg_ctype out_msg_ctype;

private:
	typedef ReaderWriter<socket2<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>, out_msg_type> super;

protected:
	enum link_status {CONNECTED, FORCE_SHUTTING_DOWN, GRACEFUL_SHUTTING_DOWN, BROKEN, HANDSHAKING};

	socket_base(asio::io_context& io_context_) : super(io_context_), status(link_status::BROKEN) {}
	template<typename Arg> socket_base(asio::io_context& io_context_, Arg&& arg) : super(io_context_, std::forward<Arg>(arg)), status(link_status::BROKEN) {}

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 5;

	virtual bool is_ready() {return is_connected();}
	virtual void send_heartbeat()
	{
		auto_duration dur(stat.pack_time_sum);
		auto msg = this->packer()->pack_heartbeat();
		dur.end();

		if (!msg.empty())
			do_direct_send_msg(std::move(msg));
	}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function, so if you want to do some additional initialization
	// for this socket, do it at here and in the constructor.
	//for tcp::single_client_base and ssl::single_client_base, this virtual function will never be called, please note.
	virtual void reset() {status = link_status::BROKEN; sending_msgs.clear(); super::reset();}

	//SOCKET status
	link_status get_link_status() const {return status;}
	bool is_broken() const {return link_status::BROKEN == status;}
	bool is_connected() const {return link_status::CONNECTED == status;}
	bool is_shutting_down() const {return link_status::FORCE_SHUTTING_DOWN == status || link_status::GRACEFUL_SHUTTING_DOWN == status;}

	std::string endpoint_to_string(const asio::ip::tcp::endpoint& ep) const {return ep.address().to_string() + ':' + std::to_string(ep.port());}
#ifdef ASIO_HAS_LOCAL_SOCKETS
	std::string endpoint_to_string(const asio::local::stream_protocol::endpoint& ep) const {return ep.path();}
#endif

	void show_info(const char* head = nullptr, const char* tail = nullptr) const
	{
		asio::error_code ec;
		std::string local_addr, remote_addr;
		auto ep = this->lowest_layer().local_endpoint(ec);
		if (!ec)
			local_addr = endpoint_to_string(ep);

		ec.clear();
		ep = this->lowest_layer().remote_endpoint(ec);
		if (!ec)
			remote_addr = endpoint_to_string(ep);

		unified_out::info_out(ASCS_LLF " %s [%s %s] %s", this->id(), nullptr == head ? "" : head,
			local_addr.data(), remote_addr.data(), nullptr == tail ? "" : tail);
	}

	void show_info(const asio::error_code& ec, const char* head = nullptr, const char* tail = nullptr) const
	{
		if (!ec)
			return show_info(head, tail);

		asio::error_code ec2;
		std::string local_addr, remote_addr;
		auto ep = this->lowest_layer().local_endpoint(ec2);
		if (!ec2)
			local_addr = endpoint_to_string(ep);

		ec2.clear();
		ep = this->lowest_layer().remote_endpoint(ec2);
		if (!ec2)
			remote_addr = endpoint_to_string(ep);

		unified_out::error_out(ASCS_LLF " %s [%s %s] %s (%d %s)", this->id(), nullptr == head ? "" : head,
			local_addr.data(), remote_addr.data(), nullptr == tail ? "" : tail, ec.value(), ec.message().data());
	}

	void show_status() const
	{
		std::stringstream s;

		if (stat.establish_time > stat.break_time)
		{
			s << "\n\testablish time: ";
			log_formater::to_time_str(stat.establish_time, s);
		}
		else if (stat.break_time > 0)
		{
			s << "\n\tbreak time: ";
			log_formater::to_time_str(stat.break_time, s);
		}

		if (stat.last_send_time > 0)
		{
			s << "\n\tlast send time: ";
			log_formater::to_time_str(stat.last_send_time, s);
		}

		if (stat.last_recv_time > 0)
		{
			s << "\n\tlast recv time: ";
			log_formater::to_time_str(stat.last_recv_time, s);
		}

		unified_out::info_out(
			"\n\tid: " ASCS_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ASCS_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\tlink status: %d"
			"\n\trecv suspended: %d"
			"\n\tsend buffer usage: %.2f%%"
			"\n\trecv buffer usage: %.2f%%"
			"%s",
			this->id(), this->started(), this->is_sending(),
#ifdef ASCS_PASSIVE_RECV
			this->is_reading(),
#endif
			this->is_dispatching(), status, this->is_recv_idle(),
			this->send_buf_usage() * 100.f, this->recv_buf_usage() * 100.f, s.str().data());
	}

	///////////////////////////////////////////////////
	//msg sending interface
	//if the message already packed, do call direct_send_msg or direct_sync_send_msg to reduce unnecessary memory replication, if you will not
	// use it any more, use std::move to wrap it when calling direct_send_msg or direct_sync_send_msg.
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ASCS_SYNC_SEND
	TCP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SYNC_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	TCP_SYNC_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
#ifdef ASCS_EXPOSE_SEND_INTERFACE
	using super::send_msg;
#endif
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	void force_shutdown() {this->dispatch_in_io_strand([this]() {this->_force_shutdown();});}
	void graceful_shutdown() {this->dispatch_in_io_strand([this]() {this->_graceful_shutdown();});}

	//used by ssl and websocket
	void start_graceful_shutdown_monitoring()
		{this->set_timer(TIMER_ASYNC_SHUTDOWN, ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 1000, [this](typename super::tid id)->bool {return this->shutdown_handler(1);});}
	void stop_graceful_shutdown_monitoring() {this->stop_timer(TIMER_ASYNC_SHUTDOWN);}

	virtual bool do_start()
	{
		status = link_status::CONNECTED;
		stat.establish_time = time(nullptr);

		on_connect(); //in this virtual function, stat.last_recv_time has not been updated (super::do_start will update it), please note
		return super::do_start();
	}

#ifdef ASCS_WANT_BATCH_MSG_SEND_NOTIFY
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	#ifdef _MSC_VER
		#pragma message("macro ASCS_WANT_BATCH_MSG_SEND_NOTIFY will not take effect with macro ASCS_WANT_MSG_SEND_NOTIFY.")
	#else
		#warning macro ASCS_WANT_BATCH_MSG_SEND_NOTIFY will not take effect with macro ASCS_WANT_MSG_SEND_NOTIFY.
	#endif
#else
	//some messages have been sent to the kernel buffer
	//notice: messages are packed, using inconstant reference is for the ability of swapping
	virtual void on_msg_send(typename super::in_container_type& msg_can) = 0;
#endif
#endif

	//generally, you don't have to rewrite this to maintain the status of connections
	//msg_can contains messages that were failed to send and tcp::socket_base will not hold them any more, if you want to re-send them in the future,
	// you must take over them and re-send (at any time) them via direct_send_msg.
	//DO NOT hold msg_can for further usage, just swap its content with your own container in this virtual function.
	virtual void on_send_error(const asio::error_code& ec, typename super::in_container_type& msg_can)
		{unified_out::error_out(ASCS_LLF " send msg error (%d %s)", this->id(), ec.value(), ec.message().data());}

	virtual void on_recv_error(const asio::error_code& ec) = 0;

	virtual void on_close()
	{
#ifdef ASCS_SYNC_SEND
		ascs::do_something_to_all(sending_msgs, [](typename super::in_msg& msg) {if (msg.p) msg.p->set_value(sync_call_result::NOT_APPLICABLE);});
#endif
		status = link_status::BROKEN;
		super::on_close();
	}

	virtual void on_connect() {}
	//msg can not be unpacked
	//the socket is still available, so don't need to shutdown this tcp::socket_base if the unpacker can continue to work.
	virtual void on_unpack_error() = 0;
	virtual void on_async_shutdown_error() = 0;

private:
	using super::close;
	using super::handle_error;
	using super::handle_msg;
	using super::do_direct_send_msg;
#ifdef ASCS_SYNC_SEND
	using super::do_direct_sync_send_msg;
#endif

	void _force_shutdown() {if (link_status::FORCE_SHUTTING_DOWN != status) shutdown();}
	void _graceful_shutdown()
	{
		if (is_broken())
			shutdown();
		else if (!is_shutting_down())
		{
			status = link_status::GRACEFUL_SHUTTING_DOWN;

			asio::error_code ec;
			this->lowest_layer().shutdown(asio::socket_base::shutdown_send, ec);
			if (ec) //graceful shutdown is impossible
				shutdown();
			else
				this->set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](typename super::tid id)->bool {return this->shutdown_handler(ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
		}
	}

	void shutdown()
	{
		if (is_broken())
			close(true);
		else
		{
			status = link_status::FORCE_SHUTTING_DOWN;
			close();
		}
	}

	virtual void do_recv_msg()
	{
#ifdef ASCS_PASSIVE_RECV
		if (reading || !is_ready())
			return;
#endif
#ifdef ASCS_PASSIVE_RECV
		if (this->async_read(make_strand_handler(rw_strand,
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);}))))
			reading = true;
#else
		this->async_read(make_strand_handler(rw_strand,
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);})));
#endif
	}

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
#ifdef ASCS_PASSIVE_RECV
		reading = false; //clear reading flag before calling handle_msg() to make sure that recv_msg() is available in on_msg() and on_msg_handle()
#endif
		auto need_next_recv = false;
		if (bytes_transferred > 0)
		{
			stat.last_recv_time = time(nullptr);

			auto_duration dur(stat.unpack_time_sum);
			auto unpack_ok = this->parse_msg(bytes_transferred, temp_msg_can);
			dur.end();

			if (!unpack_ok)
			{
				if (!ec)
					on_unpack_error();

				this->unpacker()->reset(); //user can get the left half-baked msg in unpacker's reset()
			}

			need_next_recv = handle_msg(); //if macro ASCS_PASSIVE_RECV been defined, handle_msg will always return false
		}
		else if (!ec)
		{
			assert(false);
			unified_out::error_out(ASCS_LLF " read 0 byte without any errors which is unexpected, please check your unpacker!", this->id());
		}

		if (ec)
		{
			handle_error();
			on_recv_error(ec);
		}
		//if you wrote an terrible unpacker whoes completion_condition always returns 0, it will cause ascs to occupies almost all CPU resources
		// because of following do_recv_msg() invocation (rapidly and repeatedly), please note.
		else if (need_next_recv)
			do_recv_msg(); //receive msg in sequence
	}

	virtual bool do_send_msg(bool in_strand = false)
	{
		if (!in_strand && sending)
			return true;

		auto end_time = statistic::now();
		send_buffer.move_items_out(this->batch_msg_send_size(), sending_msgs);
		sending_buffer.clear(); //this buffer will not be refreshed according to sending_msgs timely
		ascs::do_something_to_all(sending_msgs, [&](typename super::in_msg& item) {
			this->stat.send_delay_sum += end_time - item.begin_time;
			this->sending_buffer.emplace_back(item.data(), item.size());
		});

		if (!sending_buffer.empty())
		{
			sending = true;
			sending_msgs.front().restart();
			this->async_write(sending_buffer, make_strand_handler(rw_strand,
				this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);})));
			return true;
		}

		sending = false;
		return false;
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			stat.last_send_time = time(nullptr);

			stat.send_byte_sum += bytes_transferred;
			stat.send_time_sum += statistic::now() - sending_msgs.front().begin_time;
			stat.send_msg_sum += sending_buffer.size();
#ifdef ASCS_SYNC_SEND
			ascs::do_something_to_all(sending_msgs, [](typename super::in_msg& item) {if (item.p) {item.p->set_value(sync_call_result::SUCCESS);}});
#endif
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
			this->on_msg_send(sending_msgs.front());
#elif defined(ASCS_WANT_BATCH_MSG_SEND_NOTIFY)
			on_msg_send(sending_msgs);
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
			if (send_buffer.empty())
#if defined(ASCS_WANT_MSG_SEND_NOTIFY) || !defined(ASCS_WANT_BATCH_MSG_SEND_NOTIFY)
				this->on_all_msg_send(sending_msgs.back());
#else
			{
				if (!sending_msgs.empty())
					this->on_all_msg_send(sending_msgs.back());
				else //on_msg_send consumed all messages
				{
					in_msg_type msg;
					this->on_all_msg_send(msg);
				}
			}
#endif
#endif
			sending_msgs.clear();
#ifndef ASCS_ARBITRARY_SEND
			if (!do_send_msg(true) && !send_buffer.empty()) //send msg in sequence
#endif
				do_send_msg(true); //just make sure no pending msgs
		}
		else
		{
#ifdef ASCS_SYNC_SEND
			ascs::do_something_to_all(sending_msgs, [](typename super::in_msg& item) {if (item.p) {item.p->set_value(sync_call_result::NOT_APPLICABLE);}});
#endif
			on_send_error(ec, sending_msgs);
			sending_msgs.clear(); //clear sending messages after on_send_error, then user can decide how to deal with them in on_send_error

			sending = false;
		}
	}

	bool shutdown_handler(size_t loop_num)
	{
		if (link_status::GRACEFUL_SHUTTING_DOWN == status)
		{
			--loop_num;
			if (loop_num > 0)
			{
				this->change_timer_call_back(TIMER_ASYNC_SHUTDOWN, ASCS_COPY_ALL_AND_THIS(typename super::tid id)->bool {return this->shutdown_handler(loop_num);});
				return true;
			}
			else
			{
				unified_out::error_out(ASCS_LLF " failed to graceful shutdown within %d seconds", this->id(), ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION);
				on_async_shutdown_error();
			}
		}

		return false;
	}

protected:
	volatile link_status status;

private:
	using super::stat;
	using super::temp_msg_can;

	using super::send_buffer;
	using super::sending;

#ifdef ASCS_PASSIVE_RECV
	using super::reading;
#endif
	using super::rw_strand;

	typename super::in_container_type sending_msgs;
	std::vector<asio::const_buffer> sending_buffer; //just to reduce memory allocation and keep the size of sending items (linear complexity, it's very important).
};

}} //namespace

#endif /* _ASCS_TCP_SOCKET_H_ */
