/*
 * single_service.h
 *
 *  Created on: 2019-5-14
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * one service_pump for one service.
 */

#ifndef _ASCS_SINGLE_SERVICE_H_
#define _ASCS_SINGLE_SERVICE_H_

#include "service_pump.h"

namespace ascs
{

template<typename Service> class single_service : public service_pump, public Service
{

public:
	using service_pump::start_service;
	using service_pump::stop_service;

public:
#if ASIO_VERSION >= 101200
	single_service(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) : service_pump(concurrency_hint), Service((service_pump&) *this) {}
#else
	single_service() : Service((service_pump&) *this) {}
#endif
};

} //namespace

#endif /* _ASCS_SINGLE_SERVICE_H_ */
