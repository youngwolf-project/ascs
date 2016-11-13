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

protected:
	typedef socket<Socket, Packer, Unpacker, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer> super;
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	enum shutdown_states {NONE, FORCE, GRACEFUL};

	socket_base(asio::io_service& io_service_) : super(io_service_), unpacker_(std::make_shared<Unpacker>()),
		shutdown_state(shutdown_states::NONE), shutdown_atomic(0) {}
	template<typename Arg> socket_base(asio::io_service& io_service_, Arg& arg) : super(io_service_, arg), unpacker_(std::make_shared<Unpacker>()),
		shutdown_state(shutdown_states::NONE), shutdown_atomic(0) {}

public:
	virtual bool obsoleted() {return !is_shutting_down() && super::obsoleted();}

	//reset all, be ensure that there's no any operations performed on this tcp::socket_base when invoke it
	void reset() {reset_state(); shutdown_state = shutdown_states::NONE; super::reset();}
	void reset_state()
	{
		unpacker_->reset_state();
		super::reset_state();
	}

	bool is_shutting_down() const {return shutdown_states::NONE != shutdown_state;}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	std::shared_ptr<i_unpacker<out_msg_type>> inner_unpacker() {return unpacker_;}
	std::shared_ptr<const i_unpacker<out_msg_type>> inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const std::shared_ptr<i_unpacker<out_msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}

	using super::send_msg;
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
	void force_shutdown() {if (shutdown_states::FORCE != shutdown_state) shutdown();}
	bool graceful_shutdown(bool sync) //will block until shutdown success or time out if sync equal to true
	{
		if (is_shutting_down())
			return false;
		else
			shutdown_state = shutdown_states::GRACEFUL;

		asio::error_code ec;
		this->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
		if (ec) //graceful shutdown is impossible
		{
			shutdown();
			return false;
		}

		if (sync)
		{
			auto loop_num = ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100; //seconds to 10 milliseconds
			while (--loop_num >= 0 && is_shutting_down())
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			if (loop_num < 0) //graceful shutdown is impossible
			{
				unified_out::info_out("failed to graceful shutdown within %d seconds", ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION);
				shutdown();
			}
		}

		return !sync;
	}

	//ascs::socket will guarantee not call this function in more than one thread concurrently.
	//return false if send buffer is empty or sending not allowed or io_service stopped
	virtual bool do_send_msg()
	{
		if (is_send_allowed() && !this->stopped() && !this->send_msg_buffer.empty())
		{
			std::vector<asio::const_buffer> bufs;
			{
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
				const size_t max_send_size = 0;
#else
				const size_t max_send_size = asio::detail::default_max_transfer_size;
#endif
				size_t size = 0;
				typename super::in_msg msg;
				auto end_time = statistic::now();

				typename super::in_container_type::lock_guard lock(this->send_msg_buffer);
				while (this->send_msg_buffer.try_dequeue_(msg))
				{
					this->stat.send_delay_sum += end_time - msg.begin_time;
					size += msg.size();
					last_send_msg.push_back(std::move(msg));
					bufs.push_back(asio::buffer(last_send_msg.back().data(), last_send_msg.back().size()));
					if (size >= max_send_size)
						break;
				}
			}

			if (!bufs.empty())
			{
				last_send_msg.front().restart();
				asio::async_write(this->next_layer(), bufs,
					this->make_handler_error_size([this](const auto& ec, auto bytes_transferred) {this->send_handler(ec, bytes_transferred);}));

				return true;
			}
		}

		return false;
	}

	virtual void do_recv_msg()
	{
		auto recv_buff = unpacker_->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);

		asio::async_read(this->next_layer(), recv_buff,
			[this](const auto& ec, auto bytes_transferred)->size_t {return this->unpacker_->completion_condition(ec, bytes_transferred);},
			this->make_handler_error_size([this](const auto& ec, auto bytes_transferred) {this->recv_handler(ec, bytes_transferred);}));
	}

	virtual bool is_send_allowed() {return !is_shutting_down() && super::is_send_allowed();}
	//can send data or not(just put into send buffer)

	//msg can not be unpacked
	//the link is still available, so don't need to shutdown this tcp::socket_base at both client and server endpoint
	virtual void on_unpack_error() = 0;

#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" ASCS_SF "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {unified_out::debug_out("recv(" ASCS_SF "): %s", msg.size(), msg.data()); return true;}

	void shutdown()
	{
		scope_atomic_lock<> lock(shutdown_atomic);
		if (!lock.locked())
			return;

		shutdown_state = shutdown_states::FORCE;
		this->stop_all_timer();

		if (this->lowest_layer().is_open())
		{
			asio::error_code ec;
			this->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		}

		this->close(); //call this at the end of 'shutdown', it's very important
	}

private:
	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			typename Unpacker::container_type temp_msg_can;
			auto unpack_ok = unpacker_->parse_msg(bytes_transferred, temp_msg_can);
			auto msg_num = temp_msg_can.size();
			if (msg_num > 0)
			{
				this->stat.recv_msg_sum += msg_num;
				this->temp_msg_buffer.resize(this->temp_msg_buffer.size() + msg_num);
				auto op_iter = this->temp_msg_buffer.rbegin();
				for (auto iter = temp_msg_can.rbegin(); iter != temp_msg_can.rend();)
				{
					this->stat.recv_byte_sum += (++iter).base()->size();
					(++op_iter).base()->swap(*iter.base());
				}
			}
			this->handle_msg();

			if (!unpack_ok)
			{
				on_unpack_error();
				//reset unpacker's state after on_unpack_error(), so user can get the left half-baked msg in on_unpack_error()
				unpacker_->reset_state();
			}
		}
		else
			this->on_recv_error(ec);
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			this->stat.send_time_sum += statistic::now() - last_send_msg.front().begin_time;
			this->stat.send_byte_sum += bytes_transferred;
			this->stat.send_msg_sum += last_send_msg.size();
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
			this->on_msg_send(last_send_msg.front());
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
			if (this->send_msg_buffer.empty())
				this->on_all_msg_send(last_send_msg.back());
#endif
		}
		else
			this->on_send_error(ec);
		last_send_msg.clear();

		if (ec)
			this->sending = false;
		else if (!do_send_msg()) //send msg sequentially, which means second sending only after first sending success
		{
			this->sending = false;
			if (!this->send_msg_buffer.empty())
				this->send_msg(); //just make sure no pending msgs
		}
	}

protected:
	list<typename super::in_msg> last_send_msg;
	std::shared_ptr<i_unpacker<out_msg_type>> unpacker_;

	shutdown_states shutdown_state;
	std::atomic_size_t shutdown_atomic;
};

}} //namespace

#endif /* _ASCS_TCP_SOCKET_H_ */
