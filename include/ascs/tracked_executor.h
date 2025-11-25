/*
 * stracked_executor.h
 *
 *  Created on: 2018-4-19
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef _ASCS_TRACKED_EXECUTOR_H_
#define _ASCS_TRACKED_EXECUTOR_H_

#include "executor.h"

namespace ascs
{
	
#if 0 == ASCS_DELAY_CLOSE
class tracked_executor
{
protected:
	virtual ~tracked_executor() {}
	tracked_executor(boost::asio::io_context& _io_context_) : io_context_(_io_context_) {}

public:
	typedef std::function<void(const boost::system::error_code&)> handler_with_error;
	typedef std::function<void(const boost::system::error_code&, size_t)> handler_with_error_size;

	bool stopped() const {return io_context_.stopped();}

#if (_MSVC_LANG > 201103L || __cplusplus > 201103L)
	#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {boost::asio::post(io_context_, [ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void defer(F&& handler) {boost::asio::defer(io_context_, [ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch(F&& handler) {boost::asio::dispatch(io_context_, [ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, F&& handler) {boost::asio::post(strand, [ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void defer_strand(boost::asio::io_context::strand& strand, F&& handler) {boost::asio::defer(strand, [ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, F&& handler) {boost::asio::dispatch(strand, [ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	#else
	template<typename F> void post(F&& handler) {io_context_.post([ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch([ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, F&& handler) {strand.post([ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, F&& handler) {strand.dispatch([ref_holder(aci), handler(std::forward<F>(handler))]() {handler();});}
	#endif

	template<typename F> handler_with_error make_handler_error(F&& handler) const {return [ref_holder(aci), handler(std::forward<F>(handler))](const auto& ec) {handler(ec);};}
	template<typename F> handler_with_error_size make_handler_error_size(F&& handler) const
		{return [ref_holder(aci), handler(std::forward<F>(handler))](const auto& ec, auto bytes_transferred) {handler(ec, bytes_transferred);};}
#else
	#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(const F& handler) {auto ref_holder(aci); boost::asio::post(io_context_, [=]() {(void) ref_holder; handler();});}
	template<typename F> void defer(const F& handler) {auto ref_holder(aci); boost::asio::defer(io_context_, [=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch(const F& handler) {auto ref_holder(aci); boost::asio::dispatch(io_context_, [=]() {(void) ref_holder; handler();});}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); boost::asio::post(strand, [=]() {(void) ref_holder; handler();});}
	template<typename F> void defer_strand(boost::asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); boost::asio::defer(strand, [=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); boost::asio::dispatch(strand, [=]() {(void) ref_holder; handler();});}
	#else
	template<typename F> void post(const F& handler) {auto ref_holder(aci); io_context_.post([=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch(const F& handler) {auto ref_holder(aci); io_context_.dispatch([=]() {(void) ref_holder; handler();});}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); strand.post([=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); strand.dispatch([=]() {(void) ref_holder; handler();});}
	#endif

	template<typename F> handler_with_error make_handler_error(const F& handler) const {auto ref_holder(aci); return [=](const boost::system::error_code& ec) {(void) ref_holder; handler(ec);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{auto ref_holder(aci); return [=](const boost::system::error_code& ec, size_t bytes_transferred) {(void) ref_holder; handler(ec, bytes_transferred);};}
#endif

#if _MSVC_LANG >= 201703L
	bool is_async_calling() const {return aci.use_count() > 1;}
#else
	bool is_async_calling() const {return !aci.unique();}
#endif
	bool is_last_async_call() const {return aci.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

protected:
	boost::asio::io_context& io_context_;

private:
	std::shared_ptr<char> aci{std::make_shared<char>('\0')}; //asynchronous calling indicator
};
#else
class tracked_executor : public executor
{
protected:
	tracked_executor(boost::asio::io_context& io_context_) : executor(io_context_) {}

public:
	inline bool is_async_calling() const {return aci;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {aci = value;}

private:
	bool aci{false}; //asynchronous calling indicator
};
#endif

} //namespace

#endif /* _ASCS_TRACKED_EXECUTOR_H_ */
