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

#include <boost/asio.hpp>

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
	executor_base(boost::asio::io_context& _io_context_) : io_context_(_io_context_) {}
	virtual ~executor_base() {}

	void set_single_thread() {single_thread = true;}
	bool is_single_thread() const {return single_thread;}

public:
	bool stopped() const {return io_context_.stopped();}

#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) const {boost::asio::post(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void defer(F&& handler) const {boost::asio::defer(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch(F&& handler) const {boost::asio::dispatch(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void post_strand(const boost::asio::io_context::strand& strand, F&& handler) const
	{
		assert(&io_context_ == &strand.context());
		single_thread ? post(std::forward<F>(handler)) : boost::asio::post(strand, this->track_async_call(std::forward<F>(handler)));
	}
	template<typename F> void defer_strand(const boost::asio::io_context::strand& strand, F&& handler) const
	{
		assert(&io_context_ == &strand.context());
		single_thread ? defer(std::forward<F>(handler)) : boost::asio::defer(strand, this->track_async_call(std::forward<F>(handler)));
	}
	template<typename F> void dispatch_strand(const boost::asio::io_context::strand& strand, F&& handler) const
	{
		assert(&io_context_ == &strand.context());
		single_thread ? dispatch(std::forward<F>(handler)) : boost::asio::dispatch(strand, this->track_async_call(std::forward<F>(handler)));
	}
#else
	template<typename F> void post(F&& handler) {io_context_.post(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, F&& handler)
	{
		assert(&io_context_ == &strand.get_io_service());
		single_thread ? post(std::forward<F>(handler)) : strand.post(this->track_async_call(std::forward<F>(handler)));
	}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, F&& handler)
	{
		assert(&io_context_ == &strand.get_io_service());
		single_thread ? dispatch(std::forward<F>(handler)) : strand.dispatch(this->track_async_call(std::forward<F>(handler)));
	}
#endif

protected:
	boost::asio::io_context& io_context_;
	bool single_thread{false};
};
using executor = executor_base<forwarder>;

} //namespace

#endif /* _ASCS_EXECUTOR_H_ */
