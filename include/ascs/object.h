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
	template<typename CallbackHandler>
	void post(const CallbackHandler& handler) {auto unused(ASCS_THIS async_call_indicator); io_service_.post([=]() {handler();});}
	template<typename CallbackHandler>
	void post(CallbackHandler&& handler) {auto unused(ASCS_THIS async_call_indicator); io_service_.post([=]() {handler();});}
	bool is_async_calling() const {return !async_call_indicator.unique();}

	template<typename CallbackHandler>
	std::function<void(const asio::error_code&)> make_handler_error(CallbackHandler&& handler) const
		{std::shared_ptr<char>unused(async_call_indicator); return [=](const asio::error_code& ec) {handler(ec);};}
	template<typename CallbackHandler>
	std::function<void(const asio::error_code&)> make_handler_error(const CallbackHandler& handler) const
		{std::shared_ptr<char>unused(async_call_indicator); return [=](const asio::error_code& ec) {handler(ec);};}

	template<typename CallbackHandler>
	std::function<void(const asio::error_code&, size_t)> make_handler_error_size(CallbackHandler&& handler) const
		{std::shared_ptr<char>unused(async_call_indicator); return [=](const asio::error_code& ec, size_t bytes_transferred) {handler(ec, bytes_transferred);};}
	template<typename CallbackHandler>
	std::function<void(const asio::error_code&, size_t)> make_handler_error_size(CallbackHandler& handler) const
		{std::shared_ptr<char>unused(async_call_indicator); return [=](const asio::error_code& ec, size_t bytes_transferred) {handler(ec, bytes_transferred);};}

protected:
	void reset() {async_call_indicator = std::make_shared<char>('\0');}

protected:
	std::shared_ptr<char> async_call_indicator;
#else
	template<typename CallbackHandler>
	void post(const CallbackHandler& handler) {io_service_.post(handler);}
	template<typename CallbackHandler>
	void post(CallbackHandler&& handler) {io_service_.post(std::move(handler));}
	bool is_async_calling() const {return false;}

	template<typename F>
	inline F&& make_handler_error(F&& f) const {return std::move(f);}
	template<typename F>
	inline const F& make_handler_error(const F& f) const {return f;}

	template<typename F>
	inline F&& make_handler_error_size(F&& f) const {return std::move(f);}
	template<typename F>
	inline const F& make_handler_error_size(const F& f) const {return f;}

protected:
	void reset() {}
#endif

protected:
	asio::io_service& io_service_;
};

} //namespace

#endif /* _ASCS_OBJECT_H_ */
