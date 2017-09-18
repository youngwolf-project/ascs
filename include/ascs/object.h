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
	virtual ~object() {}

public:
	bool stopped() const {return io_context_.stopped();}

#if 0 == ASCS_DELAY_CLOSE
	typedef std::function<void(const asio::error_code&)> handler_with_error;
	typedef std::function<void(const asio::error_code&, size_t)> handler_with_error_size;

#if (defined(_MSC_VER) && _MSC_VER > 1800) || (defined(__cplusplus) && __cplusplus > 201103L)
	#if ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {asio::post(io_context_, [unused(this->async_call_indicator), handler(std::move(handler))]() {handler();});}
	template<typename F> void post(const F& handler) {asio::post(io_context_, [unused(this->async_call_indicator), handler]() {handler();});}
	//Don't use defer function unless you fully understand asio::defer, which was introduced in asio 1.11
	template<typename F> void defer(F&& handler) {asio::defer(io_context_, [unused(this->async_call_indicator), handler(std::move(handler))]() {handler();});}
	template<typename F> void defer(const F& handler) {asio::defer(io_context_, [unused(this->async_call_indicator), handler]() {handler();});}
	#else
	template<typename F> void post(F&& handler) {io_context_.post([unused(this->async_call_indicator), handler(std::move(handler))]() {handler();});}
	template<typename F> void post(const F& handler) {io_context_.post([unused(this->async_call_indicator), handler]() {handler();});}
	#endif

	template<typename F> handler_with_error make_handler_error(F&& handler) const {return [unused(this->async_call_indicator), handler(std::move(handler))](const auto& ec) {handler(ec);};}
	template<typename F> handler_with_error make_handler_error(const F& handler) const {return [unused(this->async_call_indicator), handler](const auto& ec) {handler(ec);};}

	template<typename F> handler_with_error_size make_handler_error_size(F&& handler) const
		{return [unused(this->async_call_indicator), handler(std::move(handler))](const auto& ec, auto bytes_transferred) {handler(ec, bytes_transferred);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{return [unused(this->async_call_indicator), handler](const auto& ec, auto bytes_transferred) {handler(ec, bytes_transferred);};}
#else
	#if ASIO_VERSION >= 101100
	template<typename F> void post(const F& handler) {auto unused(async_call_indicator); asio::post(io_context_, [=]() {handler();});}
	//Don't use defer function unless you fully understand asio::defer, which was introduced in asio 1.11
	template<typename F> void defer(const F& handler) {auto unused(async_call_indicator); asio::defer(io_context_, [=]() {handler();});}
	#else
	template<typename F> void post(const F& handler) {auto unused(async_call_indicator); io_context_.post([=]() {handler();});}
	#endif
	template<typename F> handler_with_error make_handler_error(const F& handler) const {auto unused(async_call_indicator); return [=](const asio::error_code& ec) {handler(ec);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{auto unused(async_call_indicator); return [=](const asio::error_code& ec, size_t bytes_transferred) {handler(ec, bytes_transferred);};}
#endif

	bool is_async_calling() const {return !async_call_indicator.unique();}
	bool is_last_async_call() const {return async_call_indicator.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

protected:
	object(asio::io_context& _io_context_) : async_call_indicator(std::make_shared<char>('\0')), io_context_(_io_context_) {}
	std::shared_ptr<char> async_call_indicator;
#else
	#if ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {asio::post(io_context_, std::move(handler));}
	template<typename F> void post(const F& handler) {asio::post(io_context_, handler);}
	//Don't use defer function unless you fully understand asio::defer, which was introduced in asio 1.11
	template<typename F> void defer(F&& handler) {asio::defer(io_context_, std::move(handler));}
	template<typename F> void defer(const F& handler) {asio::defer(io_context_, handler);}
	#else
	template<typename F> void post(F&& handler) {io_context_.post(std::move(handler));}
	template<typename F> void post(const F& handler) {io_context_.post(handler);}
	#endif

	template<typename F> inline F&& make_handler_error(F&& f) const {return std::move(f);}
	template<typename F> inline const F& make_handler_error(const F& f) const {return f;}

	template<typename F> inline F&& make_handler_error_size(F&& f) const {return std::move(f);}
	template<typename F> inline const F& make_handler_error_size(const F& f) const {return f;}

	inline bool is_async_calling() const {return async_calling;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {async_calling = value;}

protected:
	object(asio::io_context& _io_context_) : async_calling(false), io_context_(_io_context_) {}
	bool async_calling;
#endif

	asio::io_context& io_context_;
};

} //namespace

#endif /* _ASCS_OBJECT_H_ */
