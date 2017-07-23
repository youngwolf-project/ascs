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

	socket_base(asio::io_service& io_service_) : super(io_service_) {first_init();}
	template<typename Arg> socket_base(asio::io_service& io_service_, Arg& arg) : super(io_service_, arg) {first_init();}

	//helper function, just call it in constructor
	void first_init() {status = link_status::BROKEN; unpacker_ = std::make_shared<Unpacker>();}

public:
	static const timer::tid TIMER_BEGIN = super::TIMER_END;
	static const timer::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const timer::tid TIMER_END = TIMER_BEGIN + 10;

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

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	std::shared_ptr<i_unpacker<out_msg_type>> unpacker() {return unpacker_;}
	std::shared_ptr<const i_unpacker<out_msg_type>> unpacker() const {return unpacker_;}
	void unpacker(const std::shared_ptr<i_unpacker<out_msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}

	using super::send_msg;
	///////////////////////////////////////////////////
	//msg sending interface
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//send message with sync mode
	//return 0 means empty message or this socket is busy on sending messages
	//return -1 means error occurred, otherwise the number of bytes been sent
	TCP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	size_t direct_sync_send_msg(in_msg_ctype& msg)
	{
		if (msg.empty())
			unified_out::error_out("empty message, will not send it.");
		else if (this->lock_sending_flag())
			return do_sync_send_msg(msg);

		return 0;
	}
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
				this->set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](timer::tid id)->bool {return this->async_shutdown_handler(ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
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

	//send message with sync mode
	//return -1 means error occurred, otherwise the number of bytes been sent
	size_t do_sync_send_msg(in_msg_ctype& msg)
	{
		asio::error_code ec;
		auto_duration dur(this->stat.send_time_sum);
		auto send_size = asio::write(this->next_layer(), ASCS_SEND_BUFFER_TYPE(msg.data(), msg.size()), ec);
		dur.end();

		send_handler(ec, send_size);
		return ec ? -1 : send_size;
	}

	//return false if send buffer is empty
	virtual bool do_send_msg()
	{
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

			typename super::in_container_type::lock_guard lock(this->send_msg_buffer);
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

		if (bufs.empty())
			return false;

		last_send_msg.front().restart();
		asio::async_write(this->next_layer(), bufs,
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));
		return true;
	}

	virtual bool do_send_msg(in_msg_type&& msg)
	{
		auto buf = ASCS_SEND_BUFFER_TYPE(msg.data(), msg.size());
		last_send_msg.emplace_back(std::move(msg));
		asio::async_write(this->next_layer(), buf,
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));
		return true;
	}

	virtual void do_recv_msg()
	{
		auto recv_buff = unpacker_->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);

		asio::async_read(this->next_layer(), recv_buff,
			[this](const asio::error_code& ec, size_t bytes_transferred)->size_t {return this->completion_checker(ec, bytes_transferred);},
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);}));
	}

	//msg can not be unpacked
	//the link is still available, so don't need to shutdown this tcp::socket_base at both client and server endpoint
	virtual void on_unpack_error() = 0;
	virtual void on_async_shutdown_error() = 0;

#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" ASCS_SF "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg) {unified_out::debug_out("recv(" ASCS_SF "): %s", msg.size(), msg.data()); return true;}

private:
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

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			this->last_recv_time = time(nullptr);

			typename Unpacker::container_type temp_msg_can;
			auto_duration dur(this->stat.unpack_time_sum);
			auto unpack_ok = unpacker_->parse_msg(bytes_transferred, temp_msg_can);
			dur.end();

			auto msg_num = temp_msg_can.size();
			if (msg_num > 0)
			{
				this->stat.recv_msg_sum += msg_num;
				this->temp_msg_buffer.resize(this->temp_msg_buffer.size() + msg_num);
				auto op_iter = this->temp_msg_buffer.rbegin();
				for (auto iter = temp_msg_can.rbegin(); iter != temp_msg_can.rend(); ++op_iter, ++iter)
				{
					this->stat.recv_byte_sum += iter->size();
					op_iter->swap(*iter);
				}
			}
			this->handle_msg();

			if (!unpack_ok)
				on_unpack_error(); //the user will decide whether to reset the unpacker or not in this callback
		}
		else
			this->on_recv_error(ec);
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			this->last_send_time = time(nullptr);

			this->stat.send_byte_sum += bytes_transferred;
			if (last_send_msg.empty()) //send message with sync mode
				++this->stat.send_msg_sum;
			else
			{
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
			}

			if (!do_send_msg()) //send msg in sequence
			{
				this->sending = false;
				if (!this->send_msg_buffer.empty())
					this->send_msg(); //just make sure no pending msgs
			}
		}
		else
		{
			this->on_send_error(ec);
			last_send_msg.clear(); //clear sending messages after on_send_error, then user can decide how to deal with them in on_send_error

			this->sending = false; //must after the erasure of last_send_msg to avoid race condition
		}
	}

	bool async_shutdown_handler(size_t loop_num)
	{
		if (link_status::GRACEFUL_SHUTTING_DOWN == this->status)
		{
			--loop_num;
			if (loop_num > 0)
			{
				this->update_timer_info(TIMER_ASYNC_SHUTDOWN, 10, [loop_num, this](timer::tid id)->bool {return this->async_shutdown_handler(loop_num);});
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
};

}} //namespace

#endif /* _ASCS_TCP_SOCKET_H_ */
