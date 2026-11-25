/*
 * timer.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * timer base class
 */

#ifndef _ASCS_TIMER_H_
#define _ASCS_TIMER_H_

#include <functional>

#ifdef ASCS_USE_STEADY_TIMER
#include <asio/steady_timer.hpp>
#else
#include <asio/system_timer.hpp>
#endif

#include "base.h"

namespace ascs
{

//timers are identified by id and thread safe
//if you inherit a class from class X, your own timer ids must begin from X::TIMER_END
//suppose you have more than one service thread (see service_pump for service thread number controlling), then:
//in same timer object: same timer, callback is called in sequence, but:
// if you set the same timer during the execution of the callback (for example, in its callback), and timeout happens before the callback returns,
// then the two callbacks will be performed concurrently.
//in same timer object: distinct timer, callback is called concurrently
//in distinct timer objects: callback is always called concurrently
template<typename Executor>
class timer : public Executor
{
public:
#ifdef ASCS_USE_STEADY_TIMER
	typedef asio::steady_timer timer_type;
#else
	typedef asio::system_timer timer_type;
#endif

	typedef unsigned short tid;
	typedef std::function<bool(tid)> handler_with_tid;
	static const tid TIMER_END = 0; //subclass' id must begin from parent class' TIMER_END

	struct timer_info
	{
		enum timer_status : char {TIMER_CREATED, TIMER_STARTED, TIMER_CANCELED};

		tid id;
		unsigned char seq = -1;
		timer_status status{TIMER_CREATED};
		unsigned interval_ms{0};
		mutable std::mutex mutex;
		timer_type timer;
		handler_with_tid handler; //return true from handler to continue the timer, or the timer will stop

		timer_info(tid id_, asio::io_context& io_context_) : id(id_), timer(io_context_) {}
		bool operator ==(const timer_info& other) const {return id == other.id;}
		bool operator ==(tid id_) const {return id == id_;}
	};
	typedef const timer_info timer_cinfo;

	using Executor::Executor;
	~timer() {stop_all_timer();}

	unsigned get_io_context_refs() const {return io_context_refs;}
	void add_io_context_refs(unsigned count)
	{
		if (count > 0)
		{
			io_context_refs += count;
			attach_io_context(io_context_, count);
		}
	}
	void sub_io_context_refs(unsigned count)
	{
		if (count > 0 && io_context_refs >= count)
		{
			io_context_refs -= count;
			detach_io_context(io_context_, count);
		}
	}
	void clear_io_context_refs() {sub_io_context_refs(io_context_refs);}

	template<typename T> bool create_or_update_timer(tid id, unsigned interval, T&& handler, bool start = false)
	{
		timer_info* ti = nullptr;
		{
			std::lock_guard<std::mutex> lock(timer_can_mutex);
			auto iter = std::find(std::begin(timer_can), std::end(timer_can), id);
			if (iter == std::end(timer_can))
			{
				try {timer_can.emplace_back(id, io_context_); ti = &timer_can.back();}
				catch (const std::exception& e) {unified_out::error_out("cannot create timer %d (%s)", id, e.what()); return false;}
			}
			else
				ti = &*iter;
		}
		assert (nullptr != ti);

		std::unique_lock<std::mutex> lock(ti->mutex);
		ti->interval_ms = interval;
		ti->handler = std::forward<T>(handler);

		if (start)
			start_timer(*ti);

		return true;
	}
	template<typename T> bool set_timer(tid id, unsigned interval, T&& handler) {return create_or_update_timer(id, interval, std::forward<T>(handler), true);}

	template<typename T> bool change_timer(tid id, const T& setter)
	{
		auto ti = find_timer(id);
		if (nullptr == ti)
			return false;

		std::lock_guard<std::mutex> lock(ti->mutex);
		return setter(ti);
	}
	bool change_timer_status(tid id, typename timer_info::timer_status status) {return change_timer(id, [=](timer_info* ti) {return ti->status = status, true;});}
	bool change_timer_interval(tid id, unsigned interval) {return change_timer(id, [=](timer_info* ti) {return ti->interval_ms = interval, true;});}
	template<typename T> bool change_timer_handler(tid id, T&& handler) {return change_timer(id, [&](timer_info* ti) {return ti->handler = std::forward<T>(handler), true;});}

	timer_cinfo* find_timer(tid id) const
	{
		std::lock_guard<std::mutex> lock(timer_can_mutex);
		auto iter = std::find(std::begin(timer_can), std::end(timer_can), id);
		if (iter != std::end(timer_can))
			return &*iter;

		return nullptr;
	}

	bool is_timer(tid id) const
	{
		auto ti = find_timer(id);
		if (nullptr == ti)
			return false;

		std::lock_guard<std::mutex> lock(ti->mutex);
		return timer_info::TIMER_STARTED == ti->status;
	}

	bool start_timer(tid id) {auto ti = find_timer(id); if (nullptr == ti) return false; std::lock_guard<std::mutex> lock(ti->mutex); return start_timer(*ti);}
	void stop_timer(tid id) {auto ti = find_timer(id); if (nullptr != ti) stop_timer(*ti);}
	void stop_all_timer() {do_something_to_all([this](timer_info& item) {stop_timer(item);});}
	void stop_all_timer(tid excepted_id) {do_something_to_all(ASCS_COPY_ALL_AND_THIS(timer_info& item) {if (excepted_id != item.id) stop_timer(item);});}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex, std::lock_guard<std::mutex>)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex, std::lock_guard<std::mutex>)

protected:
	void reset_io_context_refs() {if (0 == io_context_refs) add_io_context_refs(1);}
	virtual void attach_io_context(asio::io_context& io_context_, unsigned refs) {}
	virtual void detach_io_context(asio::io_context& io_context_, unsigned refs) {}

private:
	timer_info* find_timer(tid id) {return const_cast<timer_info*>(static_cast<const timer*>(this)->find_timer(id));}

	bool start_timer(timer_info& ti) {return start_timer(ti, ti.interval_ms);}
	bool start_timer(timer_info& ti, unsigned interval_ms)
	{
		ti.status = timer_info::TIMER_STARTED;
#if ASIO_VERSION >= 101100
		ti.timer.expires_after(std::chrono::milliseconds(interval_ms));
#else
		ti.timer.expires_from_now(std::chrono::milliseconds(interval_ms));
#endif
		//if timer already started, this will cancel it first
#if (_MSVC_LANG > 201103L || __cplusplus > 201103L)
		ti.timer.async_wait(this->track_async_call([this, &ti, prev_seq(++ti.seq)](const asio::error_code& ec) {
#else
		auto prev_seq = ++ti.seq;
		ti.timer.async_wait(this->track_async_call([this, &ti, prev_seq](const asio::error_code& ec) {
#endif
#ifdef ASCS_ALIGNED_TIMER
			auto begin_time = std::chrono::system_clock::now();
#endif
			std::unique_lock<std::mutex> lock(ti.mutex);
			//the same timer is started again, or
			//the timer has been stopped, which means stop_timer can also invalidate cumulative timer callbacks
			if (prev_seq != ti.seq || timer_info::TIMER_STARTED != ti.status)
				;
			else if (ec || !ti.handler)
			{
				ti.status = timer_info::TIMER_CANCELED;
				ti.handler = nullptr;
			}
			else
			{
				auto handler = ti.handler; //in case the handler is changed in the execution of the handler (for example, via set_timer)
				lock.unlock();
				auto re = handler(ti.id);
				lock.lock();

				//the same timer is started or stopped in the handler or during the execution of the handler
				if (prev_seq != ti.seq || timer_info::TIMER_STARTED != ti.status)
					;
				else if (!re)
				{
					ti.status = timer_info::TIMER_CANCELED;
					ti.handler = nullptr;
				}
				else
				{
#ifdef ASCS_ALIGNED_TIMER
					auto elapsed_ms = (unsigned) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - begin_time).count();
					if (elapsed_ms > ti.interval_ms)
						elapsed_ms %= ti.interval_ms;

					start_timer(ti, ti.interval_ms - elapsed_ms);
#else
					start_timer(ti);
#endif
				}
			}
		}));

		return true;
	}

	void stop_timer(timer_info& ti)
	{
		std::lock_guard<std::mutex> lock(ti.mutex);
		if (timer_info::TIMER_STARTED == ti.status) //enable stopping timers that has been stopped
		{
			ti.status = timer_info::TIMER_CANCELED; //invalidate cumulative timer callbacks first
			ti.handler = nullptr;
			try {ti.timer.cancel();}
			catch (const asio::system_error& e) {unified_out::error_out("cannot stop timer %d (%d %s)", ti.id, e.code().value(), e.what());}
		}
	}

private:
	typedef std::list<timer_info> container_type;
	container_type timer_can;
	mutable std::mutex timer_can_mutex;

	using Executor::io_context_;
	unsigned io_context_refs{1};
};

} //namespace

#endif /* _ASCS_TIMER_H_ */
