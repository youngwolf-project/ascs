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
	typedef std::chrono::milliseconds milliseconds;
#ifdef ASCS_USE_STEADY_TIMER
	typedef asio::steady_timer timer_type;
#else
	typedef asio::system_timer timer_type;
#endif

	typedef unsigned char tid; //the default range is [0, 40)
	static const tid TIMER_END = 0; //user timer's id must begin from parent class' TIMER_END

	struct timer_info
	{
		enum timer_status : char {TIMER_FAKE, TIMER_CREATED, TIMER_STARTED, TIMER_CANCELED};

		unsigned char seq;
		timer_status status;
		unsigned interval_ms;
		timer_type timer;
		std::function<bool(tid)> call_back; //return true from call_back to continue the timer, or the timer will stop

		timer_info(asio::io_context& io_context_) : seq(-1), status(TIMER_FAKE), interval_ms(0), timer(io_context_) {}
	};

	typedef const timer_info timer_cinfo;
	typedef std::vector<timer_info*> container_type;

	timer(asio::io_context& io_context_) : Executor(io_context_), timer_can(ASCS_MAX_TIMER) {}
	~timer() {stop_all_timer(); do_something_to_all([](timer_info* item) {delete item;});}

	bool update_timer_info(tid id, unsigned interval, std::function<bool(tid)>&& call_back, bool start = false)
	{
		id %= ASCS_MAX_TIMER;
		if (!create_timer(id))
			return false;

		assert(nullptr != timer_can[id]);
		timer_can[id]->status = timer_info::TIMER_CREATED;
		timer_can[id]->interval_ms = interval;
		timer_can[id]->call_back.swap(call_back);

		if (start)
			start_timer(id);

		return true;
	}
	bool update_timer_info(tid id, unsigned interval, const std::function<bool(tid)>& call_back, bool start = false)
		{return update_timer_info(id, interval, std::function<bool(tid)>(call_back), start);}

	void change_timer_status(tid id, typename timer_info::timer_status status) {id %= ASCS_MAX_TIMER; if (create_timer(id)) timer_can[id]->status = status;}
	void change_timer_interval(tid id, unsigned interval) {id %= ASCS_MAX_TIMER; if (create_timer(id)) timer_can[id]->interval_ms = interval;}

	void change_timer_call_back(tid id, std::function<bool(tid)>&& call_back) {id %= ASCS_MAX_TIMER; if (create_timer(id)) timer_can[id]->call_back.swap(call_back);}
	void change_timer_call_back(tid id, const std::function<bool(tid)>& call_back) {change_timer_call_back(id, std::function<bool(tid)>(call_back));}

	bool set_timer(tid id, unsigned interval, std::function<bool(tid)>&& call_back) {return update_timer_info(id, interval, std::move(call_back), true);}
	bool set_timer(tid id, unsigned interval, const std::function<bool(tid)>& call_back) {return update_timer_info(id, interval, call_back, true);}

	bool start_timer(tid id)
	{
		id %= ASCS_MAX_TIMER;
		if (nullptr == timer_can[id] || timer_info::TIMER_FAKE == timer_can[id]->status || !timer_can[id]->call_back)
			return false;

		timer_can[id]->status = timer_info::TIMER_STARTED;
#if ASIO_VERSION >= 101100
		timer_can[id]->timer.expires_after(milliseconds(timer_can[id]->interval_ms));
#else
		timer_can[id]->timer.expires_from_now(milliseconds(timer_can[id]->interval_ms));
#endif

		//if timer already started, this will cancel it first
#if (defined(_MSC_VER) && _MSC_VER > 1800) || (defined(__cplusplus) && __cplusplus > 201103L)
		timer_can[id]->timer.async_wait(this->make_handler_error([=, prev_seq(++timer_can[id]->seq)](const asio::error_code& ec) {
#else
		auto prev_seq = ++timer_can[id]->seq;
		timer_can[id]->timer.async_wait(this->make_handler_error([=](const asio::error_code& ec) {
#endif
			if (!ec && this->timer_can[id]->call_back(id) && timer_info::TIMER_STARTED == this->timer_can[id]->status)
				this->start_timer(id);
			else if (prev_seq == this->timer_can[id]->seq) //exclude a particular situation--start the same timer in call_back and return false
				this->timer_can[id]->status = timer_info::TIMER_CANCELED;
		}));

		return true;
	}

	timer_info* find_timer(tid id) const {id %= ASCS_MAX_TIMER; return timer_can[id];}
	bool is_timer(tid id) const {id %= ASCS_MAX_TIMER; return nullptr != timer_can[id] && timer_info::TIMER_STARTED == timer_can[id]->status;}
	void stop_timer(tid id)
	{
		id %= ASCS_MAX_TIMER;
		if (nullptr != timer_can[id] && timer_info::TIMER_STARTED == timer_can[id]->status) //enable stopping timers that has been stopped
		{
			try {timer_can[id]->timer.cancel();}
			catch (const asio::system_error& e) {unified_out::error_out("cannot stop timer %d (%d %s)", id, e.code().value(), e.what());}
			timer_can[id]->status = timer_info::TIMER_CANCELED;
		}
	}
	void stop_all_timer() {tid id = -1; do_something_to_all([this, &id](timer_info* item) {this->stop_timer(++id);});}
	void stop_all_timer(tid excepted_id) {tid id = -1; do_something_to_all([=, &id](timer_info* item) {if (excepted_id != ++id) this->stop_timer(id);});}

	DO_SOMETHING_TO_ALL(timer_can)
	DO_SOMETHING_TO_ONE(timer_can)

protected:
	bool create_timer(tid id)
	{
		id %= ASCS_MAX_TIMER;
		if (nullptr == timer_can[id])
		{
			try {timer_can[id] = new timer_info(io_context_);}
			catch (const std::exception& e) {unified_out::error_out("cannot create timer %d (%s)", id, e.what());}
		}
		assert(nullptr != timer_can[id]);

		return nullptr != timer_can[id];
	}

private:
	container_type timer_can;
	using Executor::io_context_;
};

} //namespace

#endif /* _ASCS_TIMER_H_ */

