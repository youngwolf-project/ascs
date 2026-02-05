/*
 * single_service_pump.h
 *
 *  Created on: 2019-5-14
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * one service_pump for one service.
 */

#ifndef _ASCS_SINGLE_SERVICE_PUMP_H_
#define _ASCS_SINGLE_SERVICE_PUMP_H_

#include "service_pump.h"

namespace ascs
{

template<typename Service> class single_service_pump : public service_pump, public Service
{

public:
	using service_pump::start_service;
	using service_pump::stop_service;

public:
#if ASIO_VERSION >= 101200
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	single_service_pump(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) : service_pump(concurrency_hint), Service((service_pump&) *this) {}
	template<typename Arg> single_service_pump(Arg&& arg, int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) :
		service_pump(concurrency_hint), Service((service_pump&) *this, std::forward<Arg>(arg)) {}
#else
	//single_service_pump always think it's using multiple io_context
	single_service_pump(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) : service_pump(concurrency_hint, true), Service((service_pump&) *this) {}
	template<typename Arg> single_service_pump(Arg&& arg, int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) :
		service_pump(concurrency_hint, true), Service((service_pump&) *this, std::forward<Arg>(arg)) {}
#endif
#else
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	using Service::Service;
#else
	//single_service_pump always think it's using multiple io_context
	single_service_pump() : service_pump(true), Service((service_pump&) *this) {}
	template<typename Arg> single_service_pump(Arg&& arg) : service_pump(true), Service((service_pump&) *this, std::forward<Arg>(arg)) {}
#endif
#endif
};

} //namespace

#endif /* _ASCS_SINGLE_SERVICE_PUMP_H_ */
