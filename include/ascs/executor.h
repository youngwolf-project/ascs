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

namespace ascs
{

class forwarder
{
protected:
	template<typename F> auto track_async_call(F&& handler) const -> decltype(std::forward<F>(handler)) {return std::forward<F>(handler);}
};

template<typename Tracker> class executor_base : public Tracker
{
protected:
	executor_base(asio::io_context& _io_context_) : io_context_(_io_context_) {}
	virtual ~executor_base() {}

public:
	bool stopped() const {return io_context_.stopped();}

#if ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) const {asio::post(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void defer(F&& handler) const {asio::defer(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch(F&& handler) const {asio::dispatch(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void post_strand(const asio::io_context::strand& strand, F&& handler) const
		{assert(&io_context_ == &strand.context()); asio::post(strand, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void defer_strand(const asio::io_context::strand& strand, F&& handler) const
		{assert(&io_context_ == &strand.context()); asio::defer(strand, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch_strand(const asio::io_context::strand& strand, F&& handler) const
		{assert(&io_context_ == &strand.context()); asio::dispatch(strand, this->track_async_call(std::forward<F>(handler)));}
#else
	template<typename F> void post(F&& handler) {io_context_.post(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler)
		{assert(&io_context_ == &strand.get_io_service()); strand.post(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler)
		{assert(&io_context_ == &strand.get_io_service()); strand.dispatch(this->track_async_call(std::forward<F>(handler)));}
#endif

protected:
	asio::io_context& io_context_;
};
using executor = executor_base<forwarder>;

} //namespace

#endif /* _ASCS_EXECUTOR_H_ */
