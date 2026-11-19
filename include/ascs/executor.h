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

class async_call_tracker
{
public:
	bool is_async_calling() const {return aci.use_count() > 1;}
	bool is_last_async_call() const {return aci.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

protected:
	async_call_tracker() = default;
	~async_call_tracker() = default;

#if (_MSVC_LANG > 201103L || __cplusplus > 201103L)
	template<typename F> auto track_async_call(F&& handler) const -> decltype(auto)
		{return [ref_holder(aci), handler(std::forward<F>(handler))](auto&&... args) -> decltype(auto) {return handler(std::forward<decltype(args)>(args)...);};}
#else
	template<typename F, typename = typename std::enable_if<!std::is_reference<F>::value>::type> struct forwarder
	{
		std::shared_ptr<char> aci;
		F handler;
		template<typename... Args> auto operator()(Args&&... args) -> decltype(handler(std::forward<Args>(args)...)) {return handler(std::forward<Args>(args)...);}
	};
	template<typename F> forwarder<F> track_async_call(F&& handler) const {return {aci, std::forward<F>(handler)};}
#endif

private:
	std::shared_ptr<char> aci{std::make_shared<char>('\0')}; //asynchronous calling indicator
};

class dummy_async_call_tracker
{
public:
	inline constexpr bool is_async_calling() const {return aci;}
	inline constexpr bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {aci = value;}

protected:
	dummy_async_call_tracker() = default;
	~dummy_async_call_tracker() = default;

	template<typename F> auto track_async_call(F&& handler) const -> decltype(std::forward<F>(handler)) {return std::forward<F>(handler);}

private:
	bool aci{false}; //asynchronous calling indicator
};

template<typename Tracker> class executor_base : protected Tracker
{
protected:
	executor_base(boost::asio::io_context& _io_context_) : io_context_(_io_context_) {}
	virtual ~executor_base() {}

public:
	bool stopped() const {return io_context_.stopped();}

#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {boost::asio::post(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void defer(F&& handler) {boost::asio::defer(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch(F&& handler) {boost::asio::dispatch(io_context_, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void post_strand(const boost::asio::io_context::strand& strand, F&& handler) const
		{assert(&io_context_ == &strand.context()); boost::asio::post(strand, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void defer_strand(const boost::asio::io_context::strand& strand, F&& handler) const
		{assert(&io_context_ == &strand.context()); boost::asio::defer(strand, this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch_strand(const boost::asio::io_context::strand& strand, F&& handler) const
		{assert(&io_context_ == &strand.context()); boost::asio::dispatch(strand, this->track_async_call(std::forward<F>(handler)));}
#else
	template<typename F> void post(F&& handler) {io_context_.post(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, F&& handler)
		{assert(&io_context_ == &strand.get_io_service()); strand.post(this->track_async_call(std::forward<F>(handler)));}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, F&& handler)
		{assert(&io_context_ == &strand.get_io_service()); strand.dispatch(this->track_async_call(std::forward<F>(handler)));}
#endif

protected:
	boost::asio::io_context& io_context_;
};
using executor = executor_base<dummy_async_call_tracker>;

} //namespace

#endif /* _ASCS_EXECUTOR_H_ */
