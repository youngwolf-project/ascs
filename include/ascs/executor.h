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

#include <asio.hpp>

#include "config.h"

namespace ascs
{

class executor
{
protected:
	virtual ~executor() {}
	executor(asio::io_context& _io_context_) : io_context_(_io_context_) {}

public:
	bool stopped() const {return io_context_.stopped();}

#if ASIO_VERSION >= 101100
	template <typename F> inline static asio::executor_binder<typename asio::decay<F>::type, asio::io_context::strand> make_strand(asio::io_context::strand& strand, ASIO_MOVE_ARG(F) f)
		{return asio::bind_executor(strand, ASIO_MOVE_CAST(F)(f));}

	template<typename F> void post(F&& handler) {asio::post(io_context_, std::forward<F>(handler));}
	template<typename F> void defer(F&& handler) {asio::defer(io_context_, std::forward<F>(handler));}
	template<typename F> void dispatch(F&& handler) {asio::dispatch(io_context_, std::forward<F>(handler));}
	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler) {asio::post(strand, std::forward<F>(handler));}
	template<typename F> void defer_strand(asio::io_context::strand& strand, F&& handler) {asio::defer(strand, std::forward<F>(handler));}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler) {asio::dispatch(strand, std::forward<F>(handler));}
#else
	template <typename F> static asio::detail::wrapped_handler<asio::io_context::strand, F, asio::detail::is_continuation_if_running> make_strand(asio::io_context::strand& strand, F f)
		{return strand.wrap(f);}

	template<typename F> void post(F&& handler) {io_context_.post(std::forward<F>(handler));}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch(std::forward<F>(handler));}
	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler) {strand.post(std::forward<F>(handler));}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler) {strand.dispatch(std::forward<F>(handler));}
#endif

	template<typename F> inline F&& make_handler_error(F&& f) const {return std::forward<F>(f);}
	template<typename F> inline F&& make_handler_error_size(F&& f) const {return std::forward<F>(f);}

protected:
	asio::io_context& io_context_;
};

} //namespace

#endif /* _ASCS_EXECUTOR_H_ */
