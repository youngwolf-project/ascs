/*
 * object.h
 *
 *  Created on: 2016-6-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef _ASCS_OBJECT_H_
#define _ASCS_OBJECT_H_

#include "base.h"

namespace ascs
{

class object
{
protected:
	object(asio::io_service& _io_service_) : io_service_(_io_service_) {reset();}
	virtual ~object() {}

public:
	bool stopped() const {return io_service_.stopped();}

#ifdef ASCS_ENHANCED_STABILITY
	template<typename F> void post(const F& handler) {auto ref_holder(aci); io_service_.post([=]() {(void) ref_holder; handler();});}
	template<typename F> void post(F&& handler) {auto ref_holder(aci); io_service_.post([=]() {(void) ref_holder; handler();});}
	bool is_async_calling() const {return !aci.unique();}
	bool is_last_async_call() const {return aci.use_count() <= 2;} //can only be called in callbacks

	typedef std::function<void(const asio::error_code&)> handler_with_error;
	template<typename F> handler_with_error make_handler_error(F&& handler) const {auto ref_holder(aci); return [=](const auto& ec) {(void) ref_holder; handler(ec);};}
	template<typename F> handler_with_error make_handler_error(const F& handler) const {auto ref_holder(aci); return [=](const auto& ec) {(void) ref_holder; handler(ec);};}

	typedef std::function<void(const asio::error_code&, size_t)> handler_with_error_size;
	template<typename F> handler_with_error_size make_handler_error_size(F&& handler) const
		{auto ref_holder(aci); return [=](const auto& ec, auto bytes_transferred) {(void) ref_holder; handler(ec, bytes_transferred);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{auto ref_holder(aci); return [=](const auto& ec, auto bytes_transferred) {(void) ref_holder; handler(ec, bytes_transferred);};}

protected:
	void reset() {aci = std::make_shared<char>('\0');}

protected:
	std::shared_ptr<char> aci; //asynchronous calling indicator
#else
	template<typename F> void post(const F& handler) {io_service_.post(handler);}
	template<typename F> void post(F&& handler) {io_service_.post(std::move(handler));}
	bool is_async_calling() const {return false;}
	bool is_last_async_call() const {return true;}

	template<typename F> inline F&& make_handler_error(F&& f) const {return std::move(f);}
	template<typename F> inline const F& make_handler_error(const F& f) const {return f;}

	template<typename F> inline F&& make_handler_error_size(F&& f) const {return std::move(f);}
	template<typename F> inline const F& make_handler_error_size(const F& f) const {return f;}

protected:
	void reset() {}
#endif

protected:
	asio::io_service& io_service_;
};

} //namespace

#endif /* _ASCS_OBJECT_H_ */
