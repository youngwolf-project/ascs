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

#include <boost/asio.hpp>

#include "config.h"

namespace ascs
{

class executor
{
protected:
	virtual ~executor() {}
	executor(boost::asio::io_context& _io_context_) : io_context_(_io_context_) {}

public:
	bool stopped() const {return io_context_.stopped();}

#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {boost::asio::post(io_context_, std::forward<F>(handler));}
	template<typename F> void defer(F&& handler) {boost::asio::defer(io_context_, std::forward<F>(handler));}
	template<typename F> void dispatch(F&& handler) {boost::asio::dispatch(io_context_, std::forward<F>(handler));}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, F&& handler) {boost::asio::post(strand, std::forward<F>(handler));}
	template<typename F> void defer_strand(boost::asio::io_context::strand& strand, F&& handler) {boost::asio::defer(strand, std::forward<F>(handler));}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, F&& handler) {boost::asio::dispatch(strand, std::forward<F>(handler));}
#else
	template<typename F> void post(F&& handler) {io_context_.post(std::forward<F>(handler));}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch(std::forward<F>(handler));}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, F&& handler) {strand.post(std::forward<F>(handler));}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, F&& handler) {strand.dispatch(std::forward<F>(handler));}
#endif

	template<typename F> inline F&& make_handler_error(F&& f) const {return std::forward<F>(f);}
	template<typename F> inline F&& make_handler_error_size(F&& f) const {return std::forward<F>(f);}

protected:
	boost::asio::io_context& io_context_;
};

} //namespace

#endif /* _ASCS_EXECUTOR_H_ */
