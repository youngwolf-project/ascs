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
#include "../container.h"

namespace ascs { namespace tcp {

template <typename Socket, typename Packer, typename Unpacker,
	template<typename, typename> class InQueue, template<typename> class InContainer,
	template<typename, typename> class OutQueue, template<typename> class OutContainer>
class socket_base : public socket<Socket, Packer, Unpacker, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef typename Packer::msg_type in_msg_type;
	typedef typename Packer::msg_ctype in_msg_ctype;
	typedef typename Unpacker::msg_type out_msg_type;
	typedef typename Unpacker::msg_ctype out_msg_ctype;

private:
	typedef socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

protected:
	enum link_status {CONNECTED, FORCE_SHUTTING_DOWN, GRACEFUL_SHUTTING_DOWN, BROKEN};

	socket_base(asio::io_context& io_context_) : super(io_context_), strand(io_context_) {first_init();}
	template<typename Arg> socket_base(asio::io_context& io_context_, Arg& arg) : super(io_context_, arg), strand(io_context_) {first_init();}

	//helper function, just call it in constructor
	void first_init() {status = link_status::BROKEN; unpacker_ = std::make_shared<Unpacker>();}

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 10;

	virtual bool obsoleted() {return !is_shutting_down() && super::obsoleted();}
	virtual bool is_ready() {return is_connected();}
	virtual void send_heartbeat()
	{
		auto_duration dur(this->stat.pack_time_sum);
		auto msg = this->packer_->pack_heartbeat();
		dur.end();
		this->do_direct_send_msg(std::move(msg));
	}

	//reset all, be ensure that there's no any operations performed on this tcp::socket_base when invoke it
	void reset() {status = link_status::BROKEN; last_send_msg.clear(); unpacker_->reset(); super::reset();}

	//SOCKET status
	bool is_broken() const {return link_status::BROKEN == status;}
	bool is_connected() const {return link_status::CONNECTED == status;}
	bool is_shutting_down() const {return link_status::FORCE_SHUTTING_DOWN == status || link_status::GRACEFUL_SHUTTING_DOWN == status;}

	void show_info(const char* head, const char* tail) const
	{
		asio::error_code ec;
		auto local_ep = this->lowest_layer().local_endpoint(ec);
		if (!ec)
		{
			auto remote_ep = this->lowest_layer().remote_endpoint(ec);
			if (!ec)
				unified_out::info_out("%s (%s:%hu %s:%hu) %s", head,
					local_ep.address().to_string().data(), local_ep.port(),
					remote_ep.address().to_string().data(), remote_ep.port(), tail);
		}
	}

	void show_info(const char* head, const char* tail, const asio::error_code& ec) const
	{
		asio::error_code ec2;
		auto local_ep = this->lowest_layer().local_endpoint(ec2);
		if (!ec2)
		{
			auto remote_ep = this->lowest_layer().remote_endpoint(ec2);
			if (!ec2)
				unified_out::info_out("%s (%s:%hu %s:%hu) %s (%d %s)", head,
					local_ep.address().to_string().data(), local_ep.port(),
					remote_ep.address().to_string().data(), remote_ep.port(), tail, ec.value(), ec.message().data());
		}
	}

	void show_status() const
	{
		unified_out::info_out(
			"\n\tid: " ASCS_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ASCS_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\tlink status: %d"
			"\n\trecv suspended: %d",
			this->id(), this->started(), this->is_sending(),
#ifdef ASCS_PASSIVE_RECV
			this->is_reading(),
#endif
			this->is_dispatching(), status, this->is_recv_idle());
	}

	//get or change the unpacker at runtime
	std::shared_ptr<i_unpacker<out_msg_type>> unpacker() {return unpacker_;}
	std::shared_ptr<const i_unpacker<out_msg_type>> unpacker() const {return unpacker_;}
#ifdef ASCS_PASSIVE_RECV
	//changing unpacker must before calling ascs::socket::recv_msg, and define ASCS_PASSIVE_RECV macro.
	void unpacker(const std::shared_ptr<i_unpacker<out_msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}
	virtual void recv_msg() {if (!this->reading && is_ready()) this->dispatch_strand(strand, [this]() {this->do_recv_msg();});}
#endif

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	void force_shutdown() {if (link_status::FORCE_SHUTTING_DOWN != status) shutdown();}
	void graceful_shutdown(bool sync) //will block until shutdown success or time out if sync equal to true
	{
		if (is_broken())
			shutdown();
		else if (!is_shutting_down())
		{
			status = link_status::GRACEFUL_SHUTTING_DOWN;

			asio::error_code ec;
			this->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
			if (ec) //graceful shutdown is impossible
				shutdown();
			else if (!sync)
				this->set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](typename super::tid id)->bool {return this->async_shutdown_handler(ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
			else
			{
				auto loop_num = ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100; //seconds to 10 milliseconds
				while (--loop_num >= 0 && link_status::GRACEFUL_SHUTTING_DOWN == status)
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				if (loop_num < 0) //graceful shutdown is impossible
				{
					unified_out::info_out("failed to graceful shutdown within %d seconds", ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION);
					shutdown();
				}
			}
		}
	}

	virtual bool do_start()
	{
		status = link_status::CONNECTED;
		this->stat.establish_time = time(nullptr);

		on_connect(); //in this virtual function, this->stat.last_recv_time has not been updated (super::do_start will update it), please note
		return super::do_start();
	}

	virtual void on_connect() {}
	//msg can not be unpacked
	//the socket is still available, so don't need to shutdown this tcp::socket_base
	virtual void on_unpack_error() = 0;
	virtual void on_async_shutdown_error() = 0;

private:
#ifndef ASCS_PASSIVE_RECV
	virtual void recv_msg() {this->dispatch_strand(strand, [this]() {this->do_recv_msg();});}
#endif
	virtual void send_msg() {this->dispatch_strand(strand, [this]() {this->do_send_msg(false);});}

	void shutdown()
	{
		if (!is_broken())
			status = link_status::FORCE_SHUTTING_DOWN; //not thread safe because of this assignment
		this->close();
	}

	size_t completion_checker(const asio::error_code& ec, size_t bytes_transferred)
	{
		auto_duration dur(this->stat.unpack_time_sum);
		return this->unpacker_->completion_condition(ec, bytes_transferred);
	}

	void do_recv_msg()
	{
#ifdef ASCS_PASSIVE_RECV
		if (this->reading)
			return;
#endif
		auto recv_buff = unpacker_->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);
		if (0 == asio::buffer_size(recv_buff))
			unified_out::error_out("The unpacker returned an empty buffer, quit receiving!");
		else
		{
#ifdef ASCS_PASSIVE_RECV
			this->reading = true;
#endif
			asio::async_read(this->next_layer(), recv_buff,
				[this](const asio::error_code& ec, size_t bytes_transferred)->size_t {return this->completion_checker(ec, bytes_transferred);}, make_strand_handler(strand,
					this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);})));
		}
	}

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
#ifdef ASCS_PASSIVE_RECV
		this->reading = false;
#endif
		if (!ec && bytes_transferred > 0)
		{
			this->stat.last_recv_time = time(nullptr);

			typename Unpacker::container_type temp_msg_can;
			auto_duration dur(this->stat.unpack_time_sum);
			auto unpack_ok = unpacker_->parse_msg(bytes_transferred, temp_msg_can);
			dur.end();

			if (!unpack_ok)
				on_unpack_error(); //the user will decide whether to reset the unpacker or not in this callback

			if (this->handle_msg(temp_msg_can))
				do_recv_msg(); //receive msg in sequence
		}
		else
			this->on_recv_error(ec);
	}

	bool do_send_msg(bool in_strand)
	{
		if (!in_strand && this->sending)
			return true;

		std::list<asio::const_buffer> bufs;
		{
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
			const size_t max_send_size = 1;
#else
			const size_t max_send_size = asio::detail::default_max_transfer_size;
#endif
			size_t size = 0;
			typename super::in_msg msg;
			auto end_time = statistic::now();

			typename super::in_queue_type::lock_guard lock(this->send_msg_buffer);
			while (this->send_msg_buffer.try_dequeue_(msg))
			{
				this->stat.send_delay_sum += end_time - msg.begin_time;
				size += msg.size();
				last_send_msg.emplace_back(std::move(msg));
				bufs.emplace_back(last_send_msg.back().data(), last_send_msg.back().size());
				if (size >= max_send_size)
					break;
			}
		}

		if ((this->sending = !bufs.empty()))
		{
			last_send_msg.front().restart();
			asio::async_write(this->next_layer(), bufs, make_strand_handler(strand,
				this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);})));
			return true;
		}

		return false;
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			this->stat.last_send_time = time(nullptr);

			this->stat.send_byte_sum += bytes_transferred;
			this->stat.send_time_sum += statistic::now() - last_send_msg.front().begin_time;
			this->stat.send_msg_sum += last_send_msg.size();
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
			this->on_msg_send(last_send_msg.front());
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
			if (this->send_msg_buffer.empty())
				this->on_all_msg_send(last_send_msg.back());
#endif
			last_send_msg.clear();
			if (!do_send_msg(true) && !this->send_msg_buffer.empty()) //send msg in sequence
				do_send_msg(true); //just make sure no pending msgs
		}
		else
		{
			this->on_send_error(ec);
			last_send_msg.clear(); //clear sending messages after on_send_error, then user can decide how to deal with them in on_send_error

			this->sending = false;
		}
	}

	bool async_shutdown_handler(size_t loop_num)
	{
		if (link_status::GRACEFUL_SHUTTING_DOWN == status)
		{
			--loop_num;
			if (loop_num > 0)
			{
				this->update_timer_info(TIMER_ASYNC_SHUTDOWN, 10, [loop_num, this](typename super::tid id)->bool {return this->async_shutdown_handler(loop_num);});
				return true;
			}
			else
			{
				unified_out::info_out("failed to graceful shutdown within %d seconds", ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION);
				on_async_shutdown_error();
			}
		}

		return false;
	}

protected:
	list<typename super::in_msg> last_send_msg;
	std::shared_ptr<i_unpacker<out_msg_type>> unpacker_;

	volatile link_status status;

private:
	asio::io_context::strand strand;
};

}} //namespace

#endif /* _ASCS_TCP_SOCKET_H_ */
