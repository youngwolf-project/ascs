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

#include "object.h"

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
class timer : public object
{
public:
	typedef std::chrono::milliseconds milliseconds;
#ifdef ASCS_USE_STEADY_TIMER
	typedef asio::steady_timer timer_type;
#else
	typedef asio::system_timer timer_type;
#endif

	typedef unsigned char tid;
	static const tid TIMER_END = 0; //user timer's id must begin from parent class' TIMER_END

	struct timer_info
	{
		enum timer_status {TIMER_FAKE, TIMER_OK, TIMER_CANCELED};

		tid id;
		timer_status status;
		size_t interval_ms;
		std::function<bool(tid)> call_back; //return true from call_back to continue the timer, or the timer will stop
		std::shared_ptr<timer_type> timer;

		timer_info() : id(0), status(TIMER_FAKE), interval_ms(0) {}
	};

	typedef const timer_info timer_cinfo;
	typedef std::vector<timer_info> container_type;

	timer(asio::io_service& _io_service_) : object(_io_service_), timer_can(256) {tid id = -1; do_something_to_all([&id](timer_info& item) {item.id = ++id;});}

	void update_timer_info(tid id, size_t interval, std::function<bool(tid)>&& call_back, bool start = false)
	{
		timer_info& ti = timer_can[id];

		if (timer_info::TIMER_FAKE == ti.status)
			ti.timer = std::make_shared<timer_type>(io_service_);
		ti.status = timer_info::TIMER_OK;
		ti.interval_ms = interval;
		ti.call_back.swap(call_back);

		if (start)
			start_timer(ti);
	}
	void update_timer_info(tid id, size_t interval, const std::function<bool(tid)>& call_back, bool start = false)
		{update_timer_info(id, interval, std::function<bool(tid)>(call_back), start);}

	void change_timer_status(tid id, timer_info::timer_status status) {timer_can[id].status = status;}
	void change_timer_interval(tid id, size_t interval) {timer_can[id].interval_ms = interval;}

	void change_timer_call_back(tid id, std::function<bool(tid)>&& call_back) {timer_can[id].call_back.swap(call_back);}
	void change_timer_call_back(tid id, const std::function<bool(tid)>& call_back) {change_timer_call_back(id, std::function<bool(tid)>(call_back));}

	void set_timer(tid id, size_t interval, std::function<bool(tid)>&& call_back) {update_timer_info(id, interval, std::move(call_back), true);}
	void set_timer(tid id, size_t interval, const std::function<bool(tid)>& call_back) {update_timer_info(id, interval, call_back, true);}

	bool start_timer(tid id)
	{
		timer_info& ti = timer_can[id];

		if (timer_info::TIMER_FAKE == ti.status)
			return false;

		ti.status = timer_info::TIMER_OK;
		start_timer(ti); //if timer already started, this will cancel it first

		return true;
	}

	timer_info find_timer(tid id) const {return timer_can[id];}
	bool is_timer(tid id) const {return timer_info::TIMER_OK == timer_can[id].status;}
	void stop_timer(tid id) {stop_timer(timer_can[id]);}
	void stop_all_timer() {do_something_to_all([this](timer_info& item) {this->stop_timer(item);});}
	void stop_all_timer(tid excepted_id) {do_something_to_all([=](timer_info& item) {if (excepted_id != item.id) this->stop_timer(item);});}

	DO_SOMETHING_TO_ALL(timer_can)
	DO_SOMETHING_TO_ONE(timer_can)

protected:
	void start_timer(timer_info& ti)
	{
		assert(timer_info::TIMER_OK == ti.status);

#if ASIO_VERSION >= 101100
		ti.timer->expires_after(milliseconds(ti.interval_ms));
#else
		ti.timer->expires_from_now(milliseconds(ti.interval_ms));
#endif
		ti.timer->async_wait(make_handler_error([this, &ti](const asio::error_code& ec) {
			if (!ec && ti.call_back(ti.id) && timer_info::TIMER_OK == ti.status)
				this->start_timer(ti);
			else
				ti.status = timer_info::TIMER_CANCELED;
		}));
	}

	void stop_timer(timer_info& ti)
	{
		if (timer_info::TIMER_OK == ti.status) //enable stopping timers that has been stopped
		{
			try {ti.timer->cancel();} catch (const asio::system_error& e) {}
			ti.status = timer_info::TIMER_CANCELED;
		}
	}

protected:
	container_type timer_can;

private:
	using object::io_service_;
};

} //namespace

#endif /* _ASCS_TIMER_H_ */

