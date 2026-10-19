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

#ifdef ASCS_USE_STEADY_TIMER
#include <asio/steady_timer.hpp>
#else
#include <asio/system_timer.hpp>
#endif

#include "base.h"

//If you inherit a class from class X, your own timer ids must begin from X::TIMER_END
namespace ascs
{

//timers are identified by id.
//for the same timer in the same timer object, any manipulations are not thread safe, please pay special attention.
//to resolve this defect, we must add a mutex member variable to timer_info, it's not worth. otherwise, they are thread safe.
//
//suppose you have more than one service thread(see service_pump for service thread number controlling), then:
//for same timer object: same timer, on_timer is called in sequence
//for same timer object: distinct timer, on_timer is called concurrently
//for distinct timer object: on_timer is always called concurrently
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
		timer_type timer;
		handler_with_tid handler; //return true from handler to continue the timer, or the timer will stop

		timer_info(tid id_, asio::io_context& io_context_) : id(id_), timer(io_context_) {}
		bool operator ==(const timer_info& other) {return id == other.id;}
		bool operator ==(tid id_) {return id == id_;}
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

		ti->interval_ms = interval;
		ti->handler = std::forward<T>(handler);

		if (start)
			start_timer(*ti);

		return true;
	}
	bool change_timer_status(tid id, typename timer_info::timer_status status) {auto ti = find_timer(id); return nullptr != ti ? ti->status = status, true : false;}
	bool change_timer_interval(tid id, unsigned interval) {auto ti = find_timer(id); return nullptr != ti ? ti->interval_ms = interval, true : false;}
	template<typename T> bool change_timer_handler(tid id, T&& handler) {auto ti = find_timer(id); return nullptr != ti ? ti->handler = std::forward<T>(handler), true : false;}
	template<typename T> bool set_timer(tid id, unsigned interval, T&& handler) {return create_or_update_timer(id, interval, std::forward<T>(handler), true);}

	timer_info* find_timer(tid id)
	{
		std::lock_guard<std::mutex> lock(timer_can_mutex);
		auto iter = std::find(std::begin(timer_can), std::end(timer_can), id);
		if (iter != std::end(timer_can))
			return &*iter;

		return nullptr;
	}

	bool is_timer(tid id) {auto ti = find_timer(id); return nullptr != ti ? timer_info::TIMER_STARTED == ti->status : false;}
	bool start_timer(tid id) {auto ti = find_timer(id); return nullptr != ti ? start_timer(*ti) : false;}
	void stop_timer(tid id) {auto ti = find_timer(id); if (nullptr != ti) stop_timer(*ti);}
	void stop_all_timer() {do_something_to_all([this](timer_info& item) {stop_timer(item);});}
	void stop_all_timer(tid excepted_id) {do_something_to_all(ASCS_COPY_ALL_AND_THIS(timer_info& item) {if (excepted_id != item.id) stop_timer(item);});}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex, std::lock_guard<std::mutex>)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex, std::lock_guard<std::mutex>)

protected:
	bool start_timer(timer_info& ti, unsigned interval_ms)
	{
		if (!ti.handler)
			return false;

		ti.status = timer_info::TIMER_STARTED;
#if ASIO_VERSION >= 101100
		ti.timer.expires_after(std::chrono::milliseconds(interval_ms));
#else
		ti.timer.expires_from_now(std::chrono::milliseconds(interval_ms));
#endif

		//if timer already started, this will cancel it first
#if (_MSVC_LANG > 201103L || __cplusplus > 201103L)
		ti.timer.async_wait(this->make_handler_error([this, &ti, prev_seq(++ti.seq)](const asio::error_code& ec) {
#else
		auto prev_seq = ++ti.seq;
		ti.timer.async_wait(this->make_handler_error([this, &ti, prev_seq](const asio::error_code& ec) {
#endif
			//the first 'timer_info::TIMER_STARTED == ti.status' judgement means stop_timer can also invalidate cumulative timer callbacks
			//the second 'timer_info::TIMER_STARTED == ti.status' judgement is used to exclude a particular situation--stop the same timer in handler and return true
#ifdef ASCS_ALIGNED_TIMER
			auto begin_time = std::chrono::system_clock::now();
			if (timer_info::TIMER_STARTED == ti.status && !ec && ti.handler(ti.id) && timer_info::TIMER_STARTED == ti.status)
			{
				auto elapsed_ms = (unsigned) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - begin_time).count();
				if (elapsed_ms > ti.interval_ms)
					elapsed_ms %= ti.interval_ms;

				start_timer(ti, ti.interval_ms - elapsed_ms);
			}
#else
			if (timer_info::TIMER_STARTED == ti.status && !ec && ti.handler(ti.id) && timer_info::TIMER_STARTED == ti.status)
				start_timer(ti);
#endif
			else if (prev_seq == ti.seq) //exclude a particular situation--start the same timer in handler and return false
				ti.status = timer_info::TIMER_CANCELED;
		}));

		return true;
	}
	bool start_timer(timer_info& ti) {return start_timer(ti, ti.interval_ms);}

	void stop_timer(timer_info& ti)
	{
		if (timer_info::TIMER_STARTED == ti.status) //enable stopping timers that has been stopped
		{
			ti.status = timer_info::TIMER_CANCELED; //invalidate cumulative timer callbacks first
			try {ti.timer.cancel();}
			catch (const asio::system_error& e) {unified_out::error_out("cannot stop timer %d (%d %s)", ti.id, e.code().value(), e.what());}
		}
	}

	void reset_io_context_refs() {if (0 == io_context_refs) add_io_context_refs(1);}
	virtual void attach_io_context(asio::io_context& io_context_, unsigned refs) {}
	virtual void detach_io_context(asio::io_context& io_context_, unsigned refs) {}

private:
	typedef std::list<timer_info> container_type;
	container_type timer_can;
	std::mutex timer_can_mutex;

	using Executor::io_context_;
	unsigned io_context_refs{1};
};

} //namespace

#endif /* _ASCS_TIMER_H_ */
