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

#include <set>
#include <chrono>

#include "object.h"

//If you inherit a class from class X, your own timer ids must begin from X::TIMER_END
namespace ascs
{

//timers are identified by id.
//for the same timer in the same timer, set_timer and stop_timer are not thread safe, please pay special attention.
//to resolve this defect, we must add a mutex member variable to timer_info, it's not worth
//
//suppose you have more than one service thread(see service_pump for service thread number control), then:
//same timer, same timer, on_timer is called sequentially
//same timer, different timer, on_timer is called concurrently
//different timer, on_timer is always called concurrently
class timer : public object
{
protected:
	typedef std::chrono::milliseconds milliseconds;
#ifdef ASCS_USE_STEADY_TIMER
	typedef asio::steady_timer timer_type;
#else
	typedef asio::system_timer timer_type;
#endif

	struct timer_info
	{
		enum timer_status {TIMER_OK, TIMER_CANCELED};

		unsigned char id;
		timer_status status;
		size_t milliseconds;
		std::function<bool(unsigned char)> call_back;
		std::shared_ptr<timer_type> timer;

		bool operator <(const timer_info& other) const {return id < other.id;}
	};

	static const unsigned char TIMER_END = 0; //user timer's id must begin from parent class' TIMER_END

	timer(asio::io_service& _io_service_) : object(_io_service_) {}

public:
	typedef const timer_info timer_cinfo;
	typedef std::set<timer_info> container_type;

	void update_timer_info(unsigned char id, size_t milliseconds, std::function<bool(unsigned char)>&& call_back, bool start = false)
	{
		timer_info ti = {id};

		std::unique_lock<std::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
		{
			iter = timer_can.insert(ti).first;
			const_cast<timer_info*>(&*iter)->timer = std::make_shared<timer_type>(io_service_);
		}
		lock.unlock();

		//items in timer_can not locked
		const_cast<timer_info*>(&*iter)->status = timer_info::TIMER_OK;
		const_cast<timer_info*>(&*iter)->milliseconds = milliseconds;
		const_cast<timer_info*>(&*iter)->call_back.swap(call_back);

		if (start)
			start_timer(*iter);
	}
	void update_timer_info(unsigned char id, size_t milliseconds, const std::function<bool(unsigned char)>& call_back, bool start = false)
		{update_timer_info(id, milliseconds, std::function<bool(unsigned char)>(call_back), start);}

	void set_timer(unsigned char id, size_t milliseconds, std::function<bool(unsigned char)>&& call_back) {update_timer_info(id, milliseconds, std::move(call_back), true);}
	void set_timer(unsigned char id, size_t milliseconds, const std::function<bool(unsigned char)>& call_back) {update_timer_info(id, milliseconds, call_back, true);}

	timer_info find_timer(unsigned char id)
	{
		timer_info ti = {id, timer_info::TIMER_CANCELED, 0};

		std::shared_lock<std::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
			return *iter;
		else
			return ti;
	}

	bool start_timer(unsigned char id)
	{
		timer_info ti = {id};

		std::shared_lock<std::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
			return false;
		lock.unlock();

		start_timer(*iter);
		return true;
	}

	void stop_timer(unsigned char id)
	{
		timer_info ti = {id};

		std::shared_lock<std::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter != std::end(timer_can))
		{
			lock.unlock();
			stop_timer(*const_cast<timer_info*>(&*iter));
		}
	}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex)

	void stop_all_timer() {do_something_to_all([this](const auto& item) {ASCS_THIS stop_timer(*const_cast<timer_info*>(&item));});}

protected:
	void reset() {object::reset();}

	void start_timer(timer_cinfo& ti)
	{
		ti.timer->expires_from_now(milliseconds(ti.milliseconds));
		//return true from call_back to continue the timer, or the timer will stop
		ti.timer->async_wait(
			make_handler_error([this, &ti](const auto& ec) {if (!ec && ti.call_back(ti.id) && timer::timer_info::TIMER_OK == ti.status) ASCS_THIS start_timer(ti);}));
	}

	void stop_timer(timer_info& ti)
	{
		asio::error_code ec;
		ti.timer->cancel(ec);
		ti.status = timer_info::TIMER_CANCELED;
	}

	container_type timer_can;
	std::shared_mutex timer_can_mutex;

private:
	using object::io_service_;
};

} //namespace

#endif /* _ASCS_TIMER_H_ */

