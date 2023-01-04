/*
 * executor.h
 *
 *  Created on: 2016-6-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef _ASCS_EXECUTOR_H_
#define _ASCS_EXECUTOR_H_

#include <functional>

#include <asio.hpp>

#include "config.h"

namespace ascs
{

class executor
{
protected:
	virtual ~executor() {}
	executor(asio::io_context& _io_context_) : io_context_(_io_context_), single_thread(false) {}

	void set_single_thread() {single_thread = true;}
	bool is_single_thread() const {return single_thread;}

public:
	typedef std::function<void(const asio::error_code&)> handler_with_error;
	typedef std::function<void(const asio::error_code&, size_t)> handler_with_error_size;

	bool stopped() const {return io_context_.stopped();}

#if ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {asio::post(io_context_, std::forward<F>(handler));}
	template<typename F> void defer(F&& handler) {asio::defer(io_context_, std::forward<F>(handler));}
	template<typename F> void dispatch(F&& handler) {asio::dispatch(io_context_, std::forward<F>(handler));}

	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler)
		{single_thread ? post(std::forward<F>(handler)) : asio::post(strand, std::forward<F>(handler));}
	template<typename F> void defer_strand(asio::io_context::strand& strand, F&& handler)
		{single_thread ? defer(std::forward<F>(handler)) : asio::defer(strand, std::forward<F>(handler));}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler)
		{single_thread ? dispatch(std::forward<F>(handler)) : asio::dispatch(strand, std::forward<F>(handler));}
#else
	template<typename F> void post(F&& handler) {io_context_.post(std::forward<F>(handler));}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch(std::forward<F>(handler));}

	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler)
		{single_thread ? post(std::forward<F>(handler)) : strand.post(std::forward<F>(handler));}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler)
		{single_thread ? dispatch(std::forward<F>(handler)) : strand.dispatch(std::forward<F>(handler));}
#endif

	template<typename F> inline handler_with_error make_handler_error(F&& f) const {return std::forward<F>(f);}
	template<typename F> inline handler_with_error_size make_handler_error_size(F&& f) const {return std::forward<F>(f);}

protected:
	asio::io_context& io_context_;
	bool single_thread;
};

} //namespace

#endif /* _ASCS_EXECUTOR_H_ */
