/*
 * socket.h
 *
 *  Created on: 2013-8-4
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint, and in both TCP and UDP socket
 */

#ifndef _ASCS_SOCKET_H_
#define _ASCS_SOCKET_H_

#include <iomanip>

#include "base.h"
#include "timer.h"

namespace ascs
{

template<typename Socket, typename Packer, typename Unpacker, typename InMsgType, typename OutMsgType>
class socket: public timer
{
public:
	struct statistic
	{
#ifdef ASCS_FULL_STATISTIC
		static bool enabled() {return true;}
		typedef std::chrono::system_clock::time_point stat_time;
		static stat_time now() {return std::chrono::system_clock::now();}
		typedef std::chrono::system_clock::duration stat_duration;
#else
		struct dummy_duration {const dummy_duration& operator +=(const dummy_duration& other) {return *this;}}; //not a real duration, just satisfy compiler(d1 += d2)
		struct dummy_time {dummy_duration operator -(const dummy_time& other) {return dummy_duration();}}; //not a real time, just satisfy compiler(t1 - t2)

		static bool enabled() {return false;}
		typedef dummy_time stat_time;
		static stat_time now() {return stat_time();}
		typedef dummy_duration stat_duration;
#endif
		statistic() {reset();}

		void reset_number() {send_msg_sum = send_byte_sum = 0; recv_msg_sum = recv_byte_sum = 0;}
#ifdef ASCS_FULL_STATISTIC
		void reset() {reset_number(); reset_duration();}
		void reset_duration()
		{
			send_delay_sum = send_time_sum = stat_duration(0);

			dispatch_dealy_sum = recv_idle_sum = stat_duration(0);
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
			handle_time_1_sum = stat_duration(0);
#endif
			handle_time_2_sum = stat_duration(0);
		}
#else
		void reset() {reset_number();}
#endif

		statistic& operator +=(const struct statistic& other)
		{
			send_msg_sum += other.send_msg_sum;
			send_byte_sum += other.send_byte_sum;
			send_delay_sum += other.send_delay_sum;
			send_time_sum += other.send_time_sum;

			recv_msg_sum += other.recv_msg_sum;
			recv_byte_sum += other.recv_byte_sum;
			dispatch_dealy_sum += other.dispatch_dealy_sum;
			recv_idle_sum += other.recv_idle_sum;
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
			handle_time_1_sum += other.handle_time_1_sum;
#endif
			handle_time_2_sum += other.handle_time_2_sum;

			return *this;
		}

		std::string to_string() const
		{
			std::ostringstream s;
#ifdef ASCS_FULL_STATISTIC
			s << "send corresponding statistic:\n"
				<< "message sum: " << send_msg_sum << std::endl
				<< "size in bytes: " << send_byte_sum << std::endl
				<< "send delay: " << std::chrono::duration_cast<std::chrono::duration<float>>(send_delay_sum).count() << std::endl
				<< "send duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(send_time_sum).count() << std::endl
				<< "\nrecv corresponding statistic:\n"
				<< "message sum: " << recv_msg_sum << std::endl
				<< "size in bytes: " << recv_byte_sum << std::endl
				<< "dispatch delay: " << std::chrono::duration_cast<std::chrono::duration<float>>(dispatch_dealy_sum).count() << std::endl
				<< "recv idle duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(recv_idle_sum).count() << std::endl
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
				<< "on_msg duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(handle_time_1_sum).count() << std::endl
#endif
				<< "on_msg_handle duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(handle_time_2_sum).count();
#else
			s << std::setfill('0') << "send corresponding statistic:\n"
				<< "message sum: " << send_msg_sum << std::endl
				<< "size in bytes: " << send_byte_sum << std::endl
				<< "\nrecv corresponding statistic:\n"
				<< "message sum: " << recv_msg_sum << std::endl
				<< "size in bytes: " << recv_byte_sum;
#endif
			return s.str();
		}

		//send corresponding statistic
		uint_fast64_t send_msg_sum; //not counted msgs in sending buffer
		uint_fast64_t send_byte_sum; //not counted msgs in sending buffer
		stat_duration send_delay_sum; //from send_(native_)msg (exclude msg packing) to asio::async_write
		stat_duration send_time_sum; //from asio::async_write to send_handler
		//above two items indicate your network's speed or load

		//recv corresponding statistic
		uint_fast64_t recv_msg_sum; //include msgs in receiving buffer
		uint_fast64_t recv_byte_sum; //include msgs in receiving buffer
		stat_duration dispatch_dealy_sum; //from parse_msg(exclude msg unpacking) to on_msg_handle
		stat_duration recv_idle_sum;
		//during this duration, socket suspended msg reception (receiving buffer overflow, msg dispatching suspended or doing congestion control)
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
		stat_duration handle_time_1_sum; //on_msg consumed time, this indicate the efficiency of msg handling
#endif
		stat_duration handle_time_2_sum; //on_msg_handle consumed time, this indicate the efficiency of msg handling
	};

protected:
	template<typename T>
	struct obj_with_begin_time : public T
	{
		obj_with_begin_time() {restart();}
		obj_with_begin_time(T&& msg) : T(std::move(msg)) {restart();}
		void restart() {restart(statistic::now());}
		void restart(const typename statistic::stat_time& begin_time_) {begin_time = begin_time_;}
		using T::swap;
		void swap(obj_with_begin_time& other) {T::swap(other); std::swap(begin_time, other.begin_time);}

		typename statistic::stat_time begin_time;
	};

	typedef obj_with_begin_time<InMsgType> in_msg;
	typedef obj_with_begin_time<OutMsgType> out_msg;
	typedef message_queue<in_msg> in_container_type;
	typedef message_queue<out_msg> out_container_type;

	static const tid TIMER_BEGIN = timer::TIMER_END;
	static const tid TIMER_HANDLE_MSG = TIMER_BEGIN;
	static const tid TIMER_DISPATCH_MSG = TIMER_BEGIN + 1;
	static const tid TIMER_DELAY_CLOSE = TIMER_BEGIN + 2;
	static const tid TIMER_END = TIMER_BEGIN + 10;

	socket(asio::io_service& io_service_) : timer(io_service_), _id(-1), next_layer_(io_service_), packer_(std::make_shared<Packer>()), started_(false) {reset_state();}
	template<typename Arg>
	socket(asio::io_service& io_service_, Arg& arg) : timer(io_service_), _id(-1), next_layer_(io_service_, arg), packer_(std::make_shared<Packer>()), started_(false) {reset_state();}

	void reset()
	{
		reset_state();
		clear_buffer();
		stat.reset();

		timer::reset();
	}

	void reset_state()
	{
		packer_->reset_state();

		sending = paused_sending = false;
		dispatching = paused_dispatching = congestion_controlling = false;
#ifndef ASCS_ENHANCED_STABILITY
		closing = false;
#endif
//		started_ = false;
	}

	void clear_buffer()
	{
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();

		last_dispatch_msg.clear();
	}

public:
	//please do not change id at runtime via the following function, except this socket is not managed by object_pool,
	//it should only be used by object_pool when reusing or creating new socket.
	void id(uint_fast64_t id) {assert(!started_); if (started_) unified_out::error_out("id is unchangeable!"); else _id = id;}
	uint_fast64_t id() const {return _id;}
	bool is_equal_to(uint_fast64_t id) const {return _id == id;}

	Socket& next_layer() {return next_layer_;}
	const Socket& next_layer() const {return next_layer_;}
	typename Socket::lowest_layer_type& lowest_layer() {return next_layer().lowest_layer();}
	const typename Socket::lowest_layer_type& lowest_layer() const {return next_layer().lowest_layer();}

	virtual bool obsoleted()
	{
#ifndef ASCS_ENHANCED_STABILITY
		return started() || closing || this->is_async_calling() ? false : recv_msg_buffer.empty() && recv_msg_buffer.idle();
#else
		return !started() && !this->is_async_calling();
#endif
	}

	bool started() const {return started_;}
	void start()
	{
		std::unique_lock<std::shared_mutex> lock(start_mutex);
		if (!started_)
			started_ = do_start();
	}

	//return false if send buffer is empty or sending not allowed or io_service stopped
	bool send_msg()
	{
		if (!sending)
		{
			std::unique_lock<std::shared_mutex> lock(send_mutex);
			if (!sending)
			{
				sending = true;
				lock.unlock();

				if (!do_send_msg())
					sending = false;
			}
		}

		return sending;
	}

	void suspend_send_msg(bool suspend) {if (!(paused_sending = suspend)) send_msg();}
	bool suspend_send_msg() const {return paused_sending;}

	void suspend_dispatch_msg(bool suspend) {if (!(paused_dispatching = suspend)) dispatch_msg();}
	bool suspend_dispatch_msg() const {return paused_dispatching;}

	void congestion_control(bool enable) {congestion_controlling = enable; unified_out::warning_out("%s congestion control.", enable ? "open" : "close");}
	bool congestion_control() const {return congestion_controlling;}

	const struct statistic& get_statistic() const {return stat;}

	//get or change the packer at runtime
	//changing packer at runtime is not thread-safe, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	std::shared_ptr<i_packer<typename Packer::msg_type>> inner_packer() {return packer_;}
	std::shared_ptr<const i_packer<typename Packer::msg_type>> inner_packer() const {return packer_;}
	void inner_packer(const std::shared_ptr<i_packer<typename Packer::msg_type>>& _packer_) {packer_ = _packer_;}

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed no matter the sending buffer is available or not,
	//this can exhaust all virtual memory, please pay special attentions.
	bool is_send_buffer_available() const {return send_msg_buffer.size() < ASCS_MAX_MSG_NUM;}

	//don't use the packer but insert into send buffer directly
	bool direct_send_msg(const InMsgType& msg, bool can_overflow = false) {return direct_send_msg(InMsgType(msg), can_overflow);}
	bool direct_send_msg(InMsgType&& msg, bool can_overflow = false) {return can_overflow || is_send_buffer_available() ? do_direct_send_msg(std::move(msg)) : false;}

	//how many msgs waiting for sending or dispatching
	GET_PENDING_MSG_NUM(get_pending_send_msg_num, send_msg_buffer)
	GET_PENDING_MSG_NUM(get_pending_recv_msg_num, recv_msg_buffer)

	POP_FIRST_PENDING_MSG(pop_first_pending_send_msg, send_msg_buffer, InMsgType)
	POP_FIRST_PENDING_MSG(pop_first_pending_recv_msg, recv_msg_buffer, OutMsgType)

	//clear all pending msgs
	POP_ALL_PENDING_MSG(pop_all_pending_send_msg, send_msg_buffer, in_container_type)
	POP_ALL_PENDING_MSG(pop_all_pending_recv_msg, recv_msg_buffer, out_container_type)

protected:
	virtual bool do_start() = 0;
	virtual bool do_send_msg() = 0; //ascs::socket will guarantee not call this function in more than one thread concurrently.
	virtual void do_recv_msg() = 0;

	virtual bool is_closable() {return true;}
	virtual bool is_send_allowed() {return !paused_sending;} //can send msg or not(just put into send buffer)

	//generally, you don't have to rewrite this to maintain the status of connections(TCP)
	virtual void on_send_error(const asio::error_code& ec) {unified_out::error_out("send msg error (%d %s)", ec.value(), ec.message().data());}
	//receiving error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const asio::error_code& ec) = 0;
	//if ASCS_ENHANCED_STABILITY macro been defined, in this callback, socket guarantee that there's no any async call associated it,
	//include user timers(created by set_timer()) and user async calls(started via post()),
	//this means you can clean up any resource in this socket except this socket itself, because this socket maybe is being maintained by object_pool.
	//if ASCS_ENHANCED_STABILITY macro not defined, socket simply call this callback ASCS_DELAY_CLOSE seconds later after link down, no any guarantees.
	virtual void on_close() {unified_out::info_out("on_close()");}

#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own receive buffer, you can move the msg to your own receive buffer, then handle them as your own strategy(may be you'll need a msg dispatch thread),
	//or you can handle the msg at here, but this will reduce efficiency because this msg handling will block the next msg receiving on the same socket,
	//but if you can handle the msg very fast, you are recommended to handle them at here, which will inversely more efficient,
	//because msg receive buffer and msg dispatching are not needed any more.
	//
	//return true means msg been handled, socket will not maintain it anymore, return false means msg cannot be handled right now, you must handle it in on_msg_handle()
	//notice: on_msg_handle() will not be invoked from within this function
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg(OutMsgType& msg) = 0;
#endif

	//handling msg in om_msg_handle() will not block msg receiving on the same socket
	//return true means msg been handled, false means msg cannot be handled right now, and socket will re-dispatch it asynchronously
	//if link_down is true, no matter return true or false, socket will not maintain this msg anymore, and continue dispatch the next msg continuously
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg_handle(OutMsgType& msg, bool link_down) = 0;

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg
	//notice: the msg is packed, using inconstant is for the convenience of swapping
	virtual void on_msg_send(InMsgType& msg) {}
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty
	//notice: the msg is packed, using inconstant is for the convenience of swapping
	virtual void on_all_msg_send(InMsgType& msg) {}
#endif

	//subclass notify socket the shutdown event.
	void close()
	{
		if (is_closable())
		{
#ifndef ASCS_ENHANCED_STABILITY
			closing = true;
#endif
			set_timer(TIMER_DELAY_CLOSE, ASCS_DELAY_CLOSE * 1000 + 50, [this](auto id)->bool {return this->timer_handler(id);});
		}
	}

	//call this in subclasses' recv_handler only
	//subclasses must guarantee not call this function in more than one thread concurrently.
	void handle_msg()
	{
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
		decltype(temp_msg_buffer) temp_buffer;
		if (!temp_msg_buffer.empty() && !paused_dispatching && !congestion_controlling)
		{
			auto begin_time = statistic::now();
			for (auto iter = std::begin(temp_msg_buffer); !paused_dispatching && !congestion_controlling && iter != std::end(temp_msg_buffer);)
				if (on_msg(*iter))
					temp_msg_buffer.erase(iter++);
				else
					temp_buffer.splice(std::end(temp_buffer), temp_msg_buffer, iter++);

			stat.handle_time_1_sum += statistic::now() - begin_time;
		}
#else
		auto temp_buffer(std::move(temp_msg_buffer));
#endif

		if (!temp_buffer.empty())
		{
			recv_msg_buffer.move_items_in(temp_buffer, -1);
			dispatch_msg();
		}

		if (temp_msg_buffer.empty() && recv_msg_buffer.size() < ASCS_MAX_MSG_NUM)
			do_recv_msg(); //receive msg sequentially, which means second receiving only after first receiving success
		else
		{
			recv_idle_begin_time = statistic::now();
			set_timer(TIMER_HANDLE_MSG, 50, [this](auto id)->bool {return this->timer_handler(id);});
		}
	}

	//return false if receiving buffer is empty or dispatching not allowed or io_service stopped
	bool dispatch_msg()
	{
		if (!dispatching)
		{
			std::unique_lock<std::shared_mutex> lock(dispatch_mutex);
			if (!dispatching)
			{
				dispatching = true;
				lock.unlock();

				if (!do_dispatch_msg())
					dispatching = false;
			}
		}

		return dispatching;
	}

	//return false if receiving buffer is empty or dispatching not allowed or io_service stopped
	bool do_dispatch_msg()
	{
		if (paused_dispatching)
			;
		else if (stopped())
		{
#ifndef ASCS_DISCARD_MSG_WHEN_LINK_DOWN
			if (!last_dispatch_msg.empty())
			{
				on_msg_handle(last_dispatch_msg, true);
				last_dispatch_msg.clear();
			}

			out_msg msg;
			typename out_container_type::lock_guard lock(recv_msg_buffer);
			while (recv_msg_buffer.try_dequeue_(msg))
				on_msg_handle(msg, true);
#endif
		}
		else if (!last_dispatch_msg.empty() || recv_msg_buffer.try_dequeue(last_dispatch_msg))
		{
			post([this]() {this->msg_handler();});
			return true;
		}

		return false;
	}

	bool do_direct_send_msg(InMsgType&& msg)
	{
		if (!msg.empty())
		{
			send_msg_buffer.enqueue(in_msg(std::move(msg)));
			send_msg();
		}

		return true;
	}

private:
	bool timer_handler(tid id)
	{
		switch (id)
		{
		case TIMER_HANDLE_MSG:
			stat.recv_idle_sum += statistic::now() - recv_idle_begin_time;
			handle_msg();
			break;
		case TIMER_DISPATCH_MSG:
			dispatch_msg();
			break;
		case TIMER_DELAY_CLOSE:
			if (!this->is_last_async_call())
				return true;
			else if (lowest_layer().is_open())
			{
				asio::error_code ec;
				lowest_layer().close(ec);
			}
			on_close();
#ifndef ASCS_ENHANCED_STABILITY
			closing = false;
#endif
			break;
		default:
			assert(false);
			break;
		}

		return false;
	}

	void msg_handler()
	{
		auto begin_time = statistic::now();
		stat.dispatch_dealy_sum += begin_time - last_dispatch_msg.begin_time;
		bool re = on_msg_handle(last_dispatch_msg, false); //must before next msg dispatching to keep sequence
		auto end_time = statistic::now();
		stat.handle_time_2_sum += end_time - begin_time;

		if (!re) //dispatch failed, re-dispatch
		{
			last_dispatch_msg.restart(end_time);
			dispatching = false;
			set_timer(TIMER_DISPATCH_MSG, 50, [this](auto id)->bool {return this->timer_handler(id);});
		}
		else //dispatch msg sequentially, which means second dispatching only after first dispatching success
		{
			last_dispatch_msg.clear();
			if (!do_dispatch_msg())
			{
				dispatching = false;
				dispatch_msg(); //just make sure no pending msgs
			}
		}
	}

protected:
	uint_fast64_t _id;
	Socket next_layer_;

	out_msg last_dispatch_msg;
	std::shared_ptr<i_packer<typename Packer::msg_type>> packer_;

	in_container_type send_msg_buffer;
	out_container_type recv_msg_buffer;
	list<out_msg> temp_msg_buffer;
	//ascs::tcp::socket will invoke handle_msg() when got some msgs. if these msgs can't be pushed into recv_msg_buffer because of:
	// 1. msg dispatching suspended;
	// 2. congestion control opened;
	//ascs::socket will delay 50 milliseconds(non-blocking) to invoke handle_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.

	bool sending, paused_sending;
	std::shared_mutex send_mutex;
	bool dispatching, paused_dispatching, congestion_controlling;
	std::shared_mutex dispatch_mutex;
#ifndef ASCS_ENHANCED_STABILITY
	bool closing;
#endif

	bool started_; //has started or not
	std::shared_mutex start_mutex;

	struct statistic stat;
	typename statistic::stat_time recv_idle_begin_time;
};

} //namespace

#endif /* _ASCS_SOCKET_H_ */
