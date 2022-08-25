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

#include "tracked_executor.h"
#include "timer.h"
#include "container.h"

namespace ascs
{

template<typename Socket, typename Packer, typename Unpacker, typename InMsgType, typename OutMsgType,
	template<typename> class InQueue, template<typename> class InContainer, template<typename> class OutQueue, template<typename> class OutContainer>
class socket : public timer<tracked_executor>
{
private:
	typedef timer<tracked_executor> super;

public:
	static const tid TIMER_BEGIN = super::TIMER_END;
	static const tid TIMER_CHECK_RECV = TIMER_BEGIN;
	static const tid TIMER_DISPATCH_MSG = TIMER_BEGIN + 1;
	static const tid TIMER_DELAY_CLOSE = TIMER_BEGIN + 2;
	static const tid TIMER_HEARTBEAT_CHECK = TIMER_BEGIN + 3;
	static const tid TIMER_END = TIMER_BEGIN + 10;

protected:
	socket(asio::io_context& io_context_) : super(io_context_), rw_strand(io_context_), next_layer_(io_context_), dis_strand(io_context_) {first_init();}
	template<typename Arg> socket(asio::io_context& io_context_, Arg&& arg) :
		super(io_context_), rw_strand(io_context_), next_layer_(io_context_, std::forward<Arg>(arg)), dis_strand(io_context_) {first_init();}

	//helper function, just call it in constructor
	void first_init()
	{
		_id = -1;
		packer_ = std::make_shared<Packer>();
		unpacker_ = std::make_shared<Unpacker>();
		clear_sending();
#ifdef ASCS_PASSIVE_RECV
		clear_reading();
#endif
#ifdef ASCS_SYNC_RECV
		sr_status = sync_recv_status::NOT_REQUESTED;
#endif
		started_ = false;
		obsoleted_ = false;
		dispatching = false;
		recv_idle_began = false;
		send_buf_size_ = ASCS_MAX_SEND_BUF;
		recv_buf_size_ = ASCS_MAX_RECV_BUF;
		msg_resuming_interval_ = ASCS_MSG_RESUMING_INTERVAL;
		msg_handling_interval_ = ASCS_MSG_HANDLING_INTERVAL;
		start_atomic.clear(std::memory_order_relaxed);
	}

	//guarantee no operations (include asynchronous operations) be performed on this socket during call following reset_next_layer functions.
#if ASIO_VERSION < 101100
	void reset_next_layer() {reset_next_layer(next_layer_.get_io_service());}
	template<typename Arg> void reset_next_layer(Arg&& arg) {reset_next_layer(next_layer_.get_io_service(), std::forward<Arg>(arg));}
#elif ASIO_VERSION < 101300
	void reset_next_layer() {reset_next_layer(next_layer_.get_executor().context());}
	template<typename Arg>
	void reset_next_layer(Arg&& arg) {reset_next_layer(next_layer_.get_executor().context(), std::forward<Arg>(arg));}
#else
	void reset_next_layer() {reset_next_layer((const asio::any_io_executor&) next_layer_.get_executor());}
	template<typename Arg> void reset_next_layer(Arg&& arg) {reset_next_layer(next_layer_.get_executor(), std::forward<Arg>(arg));}
#endif

	void reset()
	{
		assert(!is_timer(TIMER_DELAY_CLOSE));
		if (is_timer(TIMER_DELAY_CLOSE))
			throw std::runtime_error("invalid resetting or object reusing");

		reset_io_context_refs();
		stop_all_timer(); //just in case, theoretically, timer TIMER_DELAY_CLOSE and TIMER_ASYNC_SHUTDOWN (used by tcp::socket_base) can left behind.

		stat.reset();
		packer_->reset();
		unpacker_->reset();
		clear_sending();
#ifdef ASCS_PASSIVE_RECV
		clear_reading();
#endif
#ifdef ASCS_SYNC_RECV
		sr_status = sync_recv_status::NOT_REQUESTED;
#endif
		obsoleted_ = false;
		dispatching = false;
		recv_idle_began = false;
		clear_buffer();
	}

	void clear_buffer()
	{
#ifndef ASCS_DISPATCH_BATCH_MSG
		dispatching_msg.clear();
#endif
		send_buffer.clear();
		recv_buffer.clear();
	}

	//execute in the IO strand -- rw_strand
	void post_in_io_strand(const std::function<void()>& handler) {post_strand(rw_strand, handler);}
	//execute in the IO strand -- rw_strand, or current thead, use it carefully
	void dispatch_in_io_strand(const std::function<void()>& handler) {dispatch_strand(rw_strand, handler);}

	//execute in the dispatch strand -- dis_strand
	void post_in_dis_strand(const std::function<void()>& handler) {post_strand(dis_strand, handler);}
	//execute in the dispatch strand -- dis_strand, or current thead, use it carefully
	void dispatch_in_dis_strand(const std::function<void()>& handler) {dispatch_strand(dis_strand, handler);}

public:
#ifdef ASCS_SYNC_SEND
	typedef obj_with_begin_time_promise<InMsgType> in_msg;
#else
	typedef obj_with_begin_time<InMsgType> in_msg;
#endif
	typedef obj_with_begin_time<OutMsgType> out_msg;
	typedef InContainer<in_msg> in_container_type;
	typedef OutContainer<out_msg> out_container_type;
	typedef InQueue<in_container_type> in_queue_type;
	typedef OutQueue<out_container_type> out_queue_type;

	uint_fast64_t id() const {return _id;}
	bool is_equal_to(uint_fast64_t id) const {return _id == id;}
	//the id can not be changed except this socket is not managed by object_pool,
	//if this socket is managed by single_socket_service, you can change it freely via function void id(uint_fast64_t),
	//otherwise, we have only one chance to change it via this function.
	bool try_change_id(uint_fast64_t id) {if ((uint_fast64_t) -1 != _id) return false; _id = id; return true;}

	Socket& next_layer() {return next_layer_;}
	const Socket& next_layer() const {return next_layer_;}
	typename Socket::lowest_layer_type& lowest_layer() {return next_layer().lowest_layer();}
	const typename Socket::lowest_layer_type& lowest_layer() const {return next_layer().lowest_layer();}

	virtual bool obsoleted() {return obsoleted_ && !is_async_calling();}
	virtual bool is_ready() = 0; //is ready for sending and receiving messages
	virtual void send_heartbeat() = 0;
	virtual const char* type_name() const = 0;
	virtual int type_id() const = 0;

	bool started() const {return started_;}
	void start()
	{
		if (!started_ && !is_timer(TIMER_DELAY_CLOSE) && !stopped())
		{
			scope_atomic_lock lock(start_atomic);
			if (!started_ && lock.locked())
				started_ = do_start();
			else
				unified_out::error_out(ASCS_LLF " starting failed.", id());
		}
		else
			unified_out::error_out(ASCS_LLF " starting failed because of already stared or closing timer exists or service_pump stopped.", id());
	}

#ifdef ASCS_PASSIVE_RECV
	bool is_reading() const {return 1 == reading.load(std::memory_order_relaxed);}
	void recv_msg() {if (is_ready() && !is_reading()) dispatch_in_io_strand([this]() {this->do_recv_msg();});}
#else
private:
	void recv_msg() {dispatch_in_io_strand([this]() {this->do_recv_msg();});}
public:
#endif
#ifndef ASCS_EXPOSE_SEND_INTERFACE
protected:
#endif
#ifdef ASCS_ARBITRARY_SEND
	void send_msg() {_send_msg();}
#else
	void send_msg() {if (is_ready() && 1 != sending.load(std::memory_order_acquire)) _send_msg();} //here we cannot use is_sending(), because we need memory fence
#endif

public:
	void start_heartbeat(int interval, int max_absence = ASCS_HEARTBEAT_MAX_ABSENCE)
	{
		assert(interval > 0 && max_absence > 0);

		if (!is_timer(TIMER_HEARTBEAT_CHECK))
			set_timer(TIMER_HEARTBEAT_CHECK, interval * 1000, ASCS_COPY_ALL_AND_THIS(tid id)->bool {return this->check_heartbeat(interval, max_absence);});
	}

	//interval's unit is second
	//if macro ASCS_HEARTBEAT_INTERVAL been defined and is bigger than zero, start_heartbeat will be called automatically with interval equal to ASCS_HEARTBEAT_INTERVAL,
	//and max_absence equal to ASCS_HEARTBEAT_MAX_ABSENCE (so check_heartbeat will be called regularly). otherwise, you can call check_heartbeat with you own logic.
	//return false for timeout (timeout check will only be performed on valid links), otherwise true (even the link has not established yet).
	bool check_heartbeat(int interval, int max_absence = ASCS_HEARTBEAT_MAX_ABSENCE)
	{
		assert(interval > 0 && max_absence > 0);

		if (stat.last_recv_time > 0 && is_ready()) //check of last_recv_time is essential, because user may call check_heartbeat before do_start
		{
			auto now = time(nullptr);
			if (now - stat.last_recv_time >= interval * max_absence)
				if (!on_heartbeat_error())
					return false;

#ifndef ASCS_ALWAYS_SEND_HEARTBEAT
			if (now - stat.last_send_time >= interval && !is_sending()) //don't need to send heartbeat if we're sending messages
#endif
				send_heartbeat();
		}

		return true;
	}

	bool is_sending() const {return 1 == sending.load(std::memory_order_relaxed);}
	bool is_dispatching() const {return dispatching;}
	bool is_recv_idle() const {return recv_idle_began;}

	void send_buf_size(size_t size) {if (size > 0) send_buf_size_ = size;}
	size_t send_buf_size() const {return send_buf_size_;}
	float send_buf_usage() const {return (float) send_buffer.size_in_byte() / send_buf_size_;}

	void recv_buf_size(size_t size) {if (size > 0) recv_buf_size_ = size;}
	size_t recv_buf_size() const {return recv_buf_size_;}
	float recv_buf_usage() const {return (float) recv_buffer.size_in_byte() / recv_buf_size_;}

	void msg_resuming_interval(unsigned interval) {msg_resuming_interval_ = interval;}
	unsigned msg_resuming_interval() const {return msg_resuming_interval_;}

	void msg_handling_interval(size_t interval) {msg_handling_interval_ = interval;}
	size_t msg_handling_interval() const {return msg_handling_interval_;}

	//in ascs, it's thread safe to access stat without mutex, because for a specific member of stat, ascs will never access it concurrently.
	//but user can access stat out of ascs via get_statistic function, although user can only read it, there's still a potential risk (especially
	// on 32 bit system, most likely, it will not be thread safe), so whether it's thread safe or not depends on std::chrono::system_clock::duration.
	//i can make it thread safe in ascs, but is it worth to do so? because it affect performance a lot, this is a problem.
	//by the way, only with macro ASCS_FULL_STATISTIC, above potential risk can happen, even it happens, it just corrupt data, not memory, so just
	// logic error, no memory error (such as segment fault, bad memory, etc.) will occur.
	const struct statistic& get_statistic() const {return stat;}

	//get or change the packer at runtime
	//changing packer at runtime is not thread-safe (if we're sending messages concurrently), please pay special attention,
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not commonly needed and you know how to avoid
	// race condition between message sending and packer replacement (because ascs never send messages automatically except with macro
	// ASCS_HEARTBEAT_INTERVAL, please note).
	const std::shared_ptr<i_packer<typename Packer::msg_type>>& packer() {return packer_;}
	const std::shared_ptr<const i_packer<typename Packer::msg_type>>& packer() const {return packer_;}
	void packer(const std::shared_ptr<i_packer<typename Packer::msg_type>>& _packer_) {packer_ = _packer_;}

	//get or change the unpacker at runtime
	const std::shared_ptr<i_unpacker<typename Unpacker::msg_type>>& unpacker() {return unpacker_;}
	const std::shared_ptr<const i_unpacker<typename Unpacker::msg_type>>& unpacker() const {return unpacker_;}
#ifdef ASCS_PASSIVE_RECV
	//changing unpacker must before calling ascs::socket::recv_msg, and define ASCS_PASSIVE_RECV macro.
	void unpacker(const std::shared_ptr<i_unpacker<typename Unpacker::msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}
#endif

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed no matter the sending buffer is overflow or not,
	//this can exhaust all virtual memory, please pay special attentions.
	bool is_send_buffer_available() const {return send_buffer.size_in_byte() < send_buf_size_;}

	//if you define macro ASCS_PASSIVE_RECV and call recv_msg greedily, the receiving buffer may overflow, this can exhaust all virtual memory,
	//to avoid this problem, call recv_msg only if is_recv_buffer_available() returns true.
	bool is_recv_buffer_available() const {return recv_buffer.size_in_byte() < recv_buf_size_;}

	//don't use the packer but insert into send buffer directly
	template<typename T> bool direct_send_msg(T&& msg, bool can_overflow = false, bool prior = false)
		{return can_overflow || shrink_send_buffer() ? do_direct_send_msg(std::forward<T>(msg), prior) : false;}
	bool direct_send_msg(std::list<InMsgType>& msg_can, bool can_overflow = false, bool prior = false)
		{return can_overflow || shrink_send_buffer() ? do_direct_send_msg(msg_can, prior) : false;}

#ifdef ASCS_SYNC_SEND
	//don't use the packer but insert into send buffer directly, then wait the sending to finish, unit of the duration is millisecond, 0 means wait infinitely
	template<typename T> sync_call_result direct_sync_send_msg(T&& msg, unsigned duration = 0, bool can_overflow = false, bool prior = false)
		{return can_overflow || shrink_send_buffer() ? do_direct_sync_send_msg(std::forward<T>(msg), duration, prior) : sync_call_result::NOT_APPLICABLE;}
	sync_call_result direct_sync_send_msg(std::list<InMsgType>& msg_can, unsigned duration = 0, bool can_overflow = false, bool prior = false)
		{return can_overflow || shrink_send_buffer() ? do_direct_sync_send_msg(msg_can, duration, prior) : sync_call_result::NOT_APPLICABLE;}
#endif

#ifdef ASCS_SYNC_RECV
	sync_call_result sync_recv_msg(std::list<OutMsgType>& msg_can, unsigned duration = 0) //unit of the duration is millisecond, 0 means wait infinitely
	{
		if (stopped())
			return sync_call_result::NOT_APPLICABLE;

		std::unique_lock<std::mutex> lock(sync_recv_mutex);
		if (sync_recv_status::NOT_REQUESTED != sr_status)
			return sync_call_result::DUPLICATE;

#ifdef ASCS_PASSIVE_RECV
		recv_msg();
#endif
		sr_status = sync_recv_status::REQUESTED;
		auto re = sync_recv_waiting(lock, duration);
		if (sync_call_result::SUCCESS == re)
			msg_can.splice(std::end(msg_can), temp_msg_can);

		sr_status = sync_recv_status::NOT_REQUESTED;
		sync_recv_cv.notify_one();

		return re;
	}
#endif

	//how many msgs waiting for sending or dispatching
	GET_PENDING_MSG_SIZE(get_pending_send_msg_size, send_buffer)
	GET_PENDING_MSG_SIZE(get_pending_recv_msg_size, recv_buffer)

#ifdef ASCS_SYNC_SEND
	POP_FIRST_PENDING_MSG_NOTIFY(pop_first_pending_send_msg, send_buffer, in_msg)
	POP_ALL_PENDING_MSG_NOTIFY(pop_all_pending_send_msg, send_buffer, in_container_type)
#else
	POP_FIRST_PENDING_MSG(pop_first_pending_send_msg, send_buffer, in_msg)
	POP_ALL_PENDING_MSG(pop_all_pending_send_msg, send_buffer, in_container_type)
#endif

	POP_FIRST_PENDING_MSG(pop_first_pending_recv_msg, recv_buffer, out_msg)
	POP_ALL_PENDING_MSG(pop_all_pending_recv_msg, recv_buffer, out_container_type)

protected:
	virtual bool do_start()
	{
		stat.last_recv_time = time(nullptr);
#if ASCS_HEARTBEAT_INTERVAL > 0
		start_heartbeat(ASCS_HEARTBEAT_INTERVAL);
#endif
		send_msg(); //send buffer may have msgs, send them
#if !defined(ASCS_PASSIVE_RECV) || !defined(ASCS_SYNC_RECV)
		recv_msg();
#endif

		return true;
	}
	virtual bool on_heartbeat_error() = 0; //heartbeat timed out, return true to continue heartbeat function (useful for UDP)

	//if ASCS_DELAY_CLOSE is equal to zero, in this callback, socket guarantee that there's no any other async call associated it,
	// include user timers(created by set_timer()) and user async calls(started via post(), dispatch() or defer()), this means you can clean up any resource
	// in this socket except this socket itself, because this socket maybe is being maintained by object_pool.
	//otherwise (bigger than zero), socket simply call this callback ASCS_DELAY_CLOSE seconds later after link down, no any guarantees.
	//if you overwrote this callback, do not forget to call parent class' on_close at the end.
	virtual void on_close() {unified_out::info_out(ASCS_LLF " on_close()", id());}
	virtual void after_close() {} //a good case for using this is to reconnect the server, please refer to client_socket_base.

#ifdef ASCS_SYNC_DISPATCH
	//return positive value if handled some messages (include all messages), if some msg left behind, socket will re-dispatch them asynchronously
	//notice: using inconstant reference is for the ability of swapping
	virtual size_t on_msg(std::list<OutMsgType>& msg_can)
	{
		//it's always thread safe in this virtual function, because it blocks message receiving
		ascs::do_something_to_all(msg_can, [this](OutMsgType& msg) {
			unified_out::debug_out(ASCS_LLF " recv(" ASCS_SF "): %s", this->id(), msg.size(), msg.data());
		});
		msg_can.clear(); //have handled all messages

		return 1;
	}
#endif
#ifdef ASCS_DISPATCH_BATCH_MSG
	//return positive value if handled some messages (include all messages), if some msg left behind, socket will re-dispatch them asynchronously
	//notice: using inconstant reference is for the ability of swapping
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		out_container_type tmp_can;
		msg_can.swap(tmp_can); //must be thread safe, or avoid race condition from your business logic

		ascs::do_something_to_all(tmp_can, [this](OutMsgType& msg) {
			unified_out::debug_out(ASCS_LLF " recv(" ASCS_SF "): %s", this->id(), msg.size(), msg.data());
		});
		return 1;
	}
#else
	//return true means msg been handled, false means msg cannot be handled right now, and socket will re-dispatch it asynchronously
	virtual bool on_msg_handle(OutMsgType& msg)
		{unified_out::debug_out(ASCS_LLF " recv(" ASCS_SF "): %s", id(), msg.size(), msg.data()); return true;}
#endif

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg
	//notice: the msg is packed, using inconstant reference is for the ability of swapping
	virtual void on_msg_send(InMsgType& msg) = 0;
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty
	//notice: the msg is packed, using inconstant reference is for the ability of swapping
	virtual void on_all_msg_send(InMsgType& msg) = 0;
#endif

	//return true means send buffer becomes available
#ifdef ASCS_SHRINK_SEND_BUFFER
	virtual size_t calc_shrink_size(size_t current_size) {return current_size / 3;}
	virtual void on_msg_discard(in_container_type& msg_can) {}

	bool shrink_send_buffer()
	{
		send_buffer.lock();
		auto size = send_buffer.size_in_byte();
		if (size < send_buf_size_)
		{
			send_buffer.unlock();
			return true;
		}
		else if (0 == (size = calc_shrink_size(size)))
		{
			send_buffer.unlock();
			return false;
		}

		in_container_type msg_can;
		send_buffer.move_items_out_(size, msg_can);
		send_buffer.unlock();

		on_msg_discard(msg_can);
		return true;
	}
#else
	bool shrink_send_buffer() const {return is_send_buffer_available();}
#endif

#ifdef ASCS_PASSIVE_RECV
	void clear_reading() {reading.store(0, std::memory_order_release);}
	bool test_and_set_reading() {return 1 == reading.exchange(1, std::memory_order_acq_rel);}
#endif

	void clear_sending() {sending.store(0, std::memory_order_release);}
	bool test_and_set_sending() {return 1 == sending.exchange(1, std::memory_order_acq_rel);}

	//subclass notify shutdown event
	bool close(bool use_close = false) //if not use_close, shutdown (both direction) will be used
	{
		scope_atomic_lock lock(start_atomic);
		while (!lock.locked())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			lock.lock();
		}
		if (!started_)
			return false;

		started_ = false;
#ifdef ASCS_SYNC_RECV
		sync_recv_cv.notify_all();
#endif
		stop_all_timer();

		if (lowest_layer().is_open())
		{
			asio::error_code ec;
			use_close ? lowest_layer().close(ec) : lowest_layer().shutdown(asio::socket_base::shutdown_both, ec);

			stat.break_time = time(nullptr);
		}

		if (stopped())
		{
			unpacker_->reset(); //very important, otherwise, the unpacker will never be able to parse any more messages if its buffer has legacy data
			on_close();
			after_close();
			obsoleted_ = true;
		}
		else
		{
			set_async_calling(true);
			set_timer(TIMER_DELAY_CLOSE, ASCS_DELAY_CLOSE * 1000 + 50, [this](tid id)->bool {return this->timer_handler(id);});
		}

		return true;
	}

	void handle_error()
	{
#ifdef ASCS_SYNC_RECV
		std::unique_lock<std::mutex> lock(sync_recv_mutex);
		if (sync_recv_status::REQUESTED == sr_status)
		{
			sr_status = sync_recv_status::RESPONDED_FAILURE;
			sync_recv_cv.notify_one();

			sync_recv_cv.wait(lock, [this]() {return !this->started_ || sync_recv_status::RESPONDED_FAILURE != this->sr_status;});
		}
#endif
	}

	bool handle_msg()
	{
		size_t size = 0;
		auto size_in_byte = ascs::get_size_in_byte(temp_msg_can, size);
		stat.recv_msg_sum += size;
		stat.recv_byte_sum += size_in_byte;
#ifdef ASCS_SYNC_RECV
		std::unique_lock<std::mutex> lock(sync_recv_mutex);
		if (sync_recv_status::REQUESTED == sr_status)
		{
			sr_status = sync_recv_status::RESPONDED;
			sync_recv_cv.notify_one();

			sync_recv_cv.wait(lock, [this]() {return !this->started_ || sync_recv_status::RESPONDED != this->sr_status;});
			if (sync_recv_status::RESPONDED == sr_status) //eliminate race condition on temp_msg_can with sync_recv_msg
				return false;
			else if (temp_msg_can.empty())
				return handled_msg(); //sync_recv_msg() has consumed temp_msg_can
		}
		lock.unlock();
#endif
		auto empty = temp_msg_can.empty();
#ifdef ASCS_SYNC_DISPATCH
#ifndef ASCS_PASSIVE_RECV
		if (!empty)
#endif
		{
			auto_duration dur(stat.handle_time_sum);
			if (on_msg(temp_msg_can) > 0)
			{
				size_in_byte = 0; //to re-calculate size_in_byte
				empty = temp_msg_can.empty();
			}
		}
#elif defined(ASCS_PASSIVE_RECV)
		if (empty)
		{
			empty = false;
			temp_msg_can.emplace_back(); //empty message, let user always having the chance to call recv_msg()
		}
#endif
		if (!empty)
		{
			out_container_type temp_buffer;
			for (auto iter = temp_msg_can.begin(); iter != temp_msg_can.end(); ++iter)
				temp_buffer.emplace_back(std::move(*iter));
			temp_msg_can.clear();

			recv_buffer.move_items_in(temp_buffer, size_in_byte);
			dispatch_msg();
		}

		return handled_msg();
	}

	template<typename T> bool do_direct_send_msg(T&& msg, bool prior = false)
	{
		if (msg.empty())
			unified_out::error_out(ASCS_LLF " found an empty message, please check your packer.", id());
		else if (prior ? send_buffer.enqueue_front(std::forward<T>(msg)) : send_buffer.enqueue(std::forward<T>(msg)))
			send_msg();

		//even if we meet an empty message (because of too big message or insufficient memory, most likely), we still return true, why?
		//please think about the function safe_send_(native_)msg, if we keep returning false, it will enter a dead loop.
		//the packer provider has the responsibility to write detailed reasons down when packing message failed.
		return true;
	}

	bool do_direct_send_msg(std::list<InMsgType>& msg_can, bool prior = false)
	{
		size_t size_in_byte = 0;
		in_container_type temp_buffer;
		ascs::do_something_to_all(msg_can, [&](InMsgType& msg) {size_in_byte += msg.size(); temp_buffer.emplace_back(std::move(msg));});
		prior ? send_buffer.move_items_in_front(temp_buffer, size_in_byte) : send_buffer.move_items_in(temp_buffer, size_in_byte);
		send_msg();

		return true;
	}

#ifdef ASCS_SYNC_SEND
	template<typename T> sync_call_result do_direct_sync_send_msg(T&& msg, unsigned duration = 0, bool prior = false)
	{
		if (stopped())
			return sync_call_result::NOT_APPLICABLE;
		else if (msg.empty())
		{
			unified_out::error_out(ASCS_LLF " found an empty message, please check your packer.", id());
			return sync_call_result::SUCCESS;
		}

		auto unused = in_msg(std::forward<T>(msg), true);
		auto p = unused.p;
		auto f = p->get_future();
		if (!(prior ? send_buffer.enqueue_front(std::move(unused)) : send_buffer.enqueue(std::move(unused))))
			return sync_call_result::NOT_APPLICABLE;

		send_msg();
		return 0 == duration || std::future_status::ready == f.wait_for(std::chrono::milliseconds(duration)) ? f.get() : sync_call_result::TIMEOUT;
	}

	sync_call_result do_direct_sync_send_msg(std::list<InMsgType>& msg_can, unsigned duration = 0, bool prior = false)
	{
		if (stopped())
			return sync_call_result::NOT_APPLICABLE;
		else if (msg_can.empty())
			return sync_call_result::SUCCESS;

		size_t size_in_byte = 0;
		in_container_type temp_buffer;
		ascs::do_something_to_all(msg_can, [&](InMsgType& msg) {size_in_byte += msg.size(); temp_buffer.emplace_back(std::move(msg));});

		temp_buffer.back().check_and_create_promise(true);
		auto p = temp_buffer.back().p;
		auto f = p->get_future();
		prior ? send_buffer.move_items_in_front(temp_buffer, size_in_byte) : send_buffer.move_items_in(temp_buffer, size_in_byte);

		send_msg();
		return 0 == duration || std::future_status::ready == f.wait_for(std::chrono::milliseconds(duration)) ? f.get() : sync_call_result::TIMEOUT;
	}
#endif

private:
	virtual void do_recv_msg() = 0;
	virtual bool do_send_msg(bool in_strand = false) = 0;

	//please do not change id at runtime via the following function, except this socket is not managed by object_pool,
	//it should only be used by object_pool when reusing or creating new socket.
	template<typename> friend class object_pool;
	template<typename> friend class single_socket_service;
	void id(uint_fast64_t id) {_id = id;}

	void reset_next_layer(asio::io_context& io_context) {(&next_layer_)->~Socket(); new (&next_layer_) Socket(io_context);}
	template<typename Arg>
	void reset_next_layer(asio::io_context& io_context, Arg&& arg) {(&next_layer_)->~Socket(); new (&next_layer_) Socket(io_context, std::forward<Arg>(arg));}
#if ASIO_VERSION >= 101300
	void reset_next_layer(const asio::any_io_executor& executor) {(&next_layer_)->~Socket(); new (&next_layer_) Socket(executor);}
	template<typename Arg>
	void reset_next_layer(const asio::any_io_executor& executor, Arg&& arg) {(&next_layer_)->~Socket(); new (&next_layer_) Socket(executor, std::forward<Arg>(arg));}
#endif

	void _send_msg() {dispatch_in_io_strand([this]() {this->do_send_msg();});}

#ifdef ASCS_SYNC_RECV
	sync_call_result sync_recv_waiting(std::unique_lock<std::mutex>& lock, unsigned duration)
	{
		auto pred = [this]() {return !this->started_ || sync_recv_status::REQUESTED != this->sr_status;};
		if (0 == duration)
			sync_recv_cv.wait(lock, std::move(pred));
		else if (!sync_recv_cv.wait_for(lock, std::chrono::milliseconds(duration), std::move(pred)))
			return sync_call_result::TIMEOUT;

		return sync_recv_status::RESPONDED == sr_status ? sync_call_result::SUCCESS : sync_call_result::NOT_APPLICABLE;
	}
#endif

	bool check_receiving(bool raise_recv)
	{
		if (is_recv_buffer_available())
		{
			if (recv_idle_began)
			{
				recv_idle_began = false;
				stat.recv_idle_sum += statistic::now() - recv_idle_begin_time;
			}

			if (raise_recv)
				recv_msg(); //receive msg in sequence

			return true;
		}
		else if (!recv_idle_began)
		{
			recv_idle_began = true;
			recv_idle_begin_time = statistic::now();
		}

		return false;
	}

	bool handled_msg()
	{
#ifndef ASCS_PASSIVE_RECV
		if (check_receiving(false))
			return true;

		set_timer(TIMER_CHECK_RECV, msg_resuming_interval_, [this](tid id)->bool {return !this->check_receiving(true);});
#endif
		return false;
	}

	//do not use dispatch_strand/dispatch_in_dis_strand at here, because the handler (do_dispatch_msg) may call this function, which can lead stack overflow.
	void dispatch_msg() {if (!dispatching) post_in_dis_strand([this]() {this->do_dispatch_msg();});}
	void do_dispatch_msg()
	{
#ifdef ASCS_DISPATCH_BATCH_MSG
		if (!recv_buffer.empty())
		{
			dispatching = true;
			auto begin_time = statistic::now();
#ifdef ASCS_FULL_STATISTIC
			recv_buffer.do_something_to_all([&](out_msg& msg) {this->stat.dispatch_delay_sum += begin_time - msg.begin_time;});
#endif
			auto re = on_msg_handle(recv_buffer);
			auto end_time = statistic::now();
			stat.handle_time_sum += end_time - begin_time;

			if (0 == re) //dispatch failed, re-dispatch
			{
#ifdef ASCS_FULL_STATISTIC
				recv_buffer.do_something_to_all([&](out_msg& msg) {msg.restart(end_time);});
#endif
				set_timer(TIMER_DISPATCH_MSG, msg_handling_interval_, [this](tid id)->bool {return this->timer_handler(id);}); //hold dispatching
			}
			else
			{
#else
		if (dispatching || recv_buffer.try_dequeue(dispatching_msg))
		{
			dispatching = true;
			auto begin_time = statistic::now();
			stat.dispatch_delay_sum += begin_time - dispatching_msg.begin_time;
			auto re = on_msg_handle(dispatching_msg); //must before next msg dispatching to keep sequence
			auto end_time = statistic::now();
			stat.handle_time_sum += end_time - begin_time;

			if (!re) //dispatch failed, re-dispatch
			{
				dispatching_msg.restart(end_time);
				set_timer(TIMER_DISPATCH_MSG, msg_handling_interval_, [this](tid id)->bool {return this->timer_handler(id);}); //hold dispatching
			}
			else
			{
				dispatching_msg.clear();
#endif
				dispatching = false;
				post_in_dis_strand([this]() {this->do_dispatch_msg();}); //dispatch msg in sequence
			}
		}
		else
			dispatching = false;
	}

	bool timer_handler(tid id)
	{
		switch (id)
		{
		case TIMER_DISPATCH_MSG:
			post_in_dis_strand([this]() {this->do_dispatch_msg();});
			break;
		case TIMER_DELAY_CLOSE:
			if (!is_last_async_call())
			{
				stop_all_timer(id);
				return true;
			}
			else if (lowest_layer().is_open())
			{
				asio::error_code ec;
				lowest_layer().close(ec);
			}
			unpacker_->reset(); //very important, otherwise, the unpacker will never be able to parse any more messages if its buffer has legacy data
			on_close();
			change_timer_status(id, timer_info::TIMER_CANCELED);
			after_close();
			set_async_calling(false);
			obsoleted_ = true;
			break;
		default:
			assert(false);
			break;
		}

		return false;
	}

protected:
	struct statistic stat;
	std::list<OutMsgType> temp_msg_can;

	in_queue_type send_buffer;
	asio::io_context::strand rw_strand;

private:
	std::shared_ptr<i_packer<typename Packer::msg_type>> packer_;
	std::shared_ptr<i_unpacker<typename Unpacker::msg_type>> unpacker_;

	bool recv_idle_began;
	volatile bool started_; //has started or not
	volatile bool obsoleted_;

	volatile bool dispatching;
#ifndef ASCS_DISPATCH_BATCH_MSG
	out_msg dispatching_msg;
#endif

	typename statistic::stat_time recv_idle_begin_time;
	out_queue_type recv_buffer;

	uint_fast64_t _id;
	Socket next_layer_;

#ifdef ASCS_PASSIVE_RECV
	std::atomic_size_t reading;
#endif
	std::atomic_size_t sending;
	std::atomic_flag start_atomic;
	asio::io_context::strand dis_strand;

#ifdef ASCS_SYNC_RECV
	enum sync_recv_status {NOT_REQUESTED, REQUESTED, RESPONDED, RESPONDED_FAILURE};
	sync_recv_status sr_status;

	std::mutex sync_recv_mutex;
	std::condition_variable sync_recv_cv;
#endif

	size_t send_buf_size_, recv_buf_size_;
	unsigned msg_resuming_interval_, msg_handling_interval_;
};

template<typename Socket, typename Packer, typename Unpacker,
	template<typename> class InQueue, template<typename> class InContainer, template<typename> class OutQueue, template<typename> class OutContainer>
using socket2 = socket<Socket, Packer, Unpacker, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer>;

template<typename Socket, typename Family, typename Packer, typename Unpacker, template<typename, typename> class InMsgWrapper, template<typename, typename> class OutMsgWrapper,
	template<typename> class InQueue, template<typename> class InContainer, template<typename> class OutQueue, template<typename> class OutContainer>
using socket3 = socket<Socket, Packer, Unpacker, InMsgWrapper<typename Packer::msg_type, Family>, OutMsgWrapper<typename Unpacker::msg_type, Family>, InQueue, InContainer, OutQueue, OutContainer>;

template<typename Socket, typename Family, typename Packer, typename Unpacker, template<typename, typename> class MsgWrapper,
	template<typename> class InQueue, template<typename> class InContainer, template<typename> class OutQueue, template<typename> class OutContainer>
using socket4 = socket3<Socket, Family, Packer, Unpacker, MsgWrapper, MsgWrapper, InQueue, InContainer, OutQueue, OutContainer>;

} //namespace

#endif /* _ASCS_SOCKET_H_ */
