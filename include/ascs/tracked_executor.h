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
class tracked_executor : public executor
{
protected:
	tracked_executor(asio::io_context& io_context_) : executor(io_context_), aci(std::make_shared<char>('\0')) {}

public:
	typedef std::function<void(const asio::error_code&)> handler_with_error;
	typedef std::function<void(const asio::error_code&, size_t)> handler_with_error_size;

#if (defined(_MSC_VER) && _MSC_VER > 1800) || (defined(__cplusplus) && __cplusplus > 201103L)
	#if ASIO_VERSION >= 101100
	template<typename F> void post(F&& handler) {asio::post(io_context_, [ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void defer(F&& handler) {asio::defer(io_context_, [ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch(F&& handler) {asio::dispatch(io_context_, [ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler) {asio::post(strand, [ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void defer_strand(asio::io_context::strand& strand, F&& handler) {asio::defer(strand, [ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler) {asio::dispatch(strand, [ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	#else
	template<typename F> void post(F&& handler) {io_context_.post([ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch(F&& handler) {io_context_.dispatch([ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void post_strand(asio::io_context::strand& strand, F&& handler) {strand.post([ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, F&& handler) {strand.dispatch([ref_holder(this->aci), handler(std::forward<F>(handler))]() {handler();});}
	#endif

	template<typename F> handler_with_error make_handler_error(F&& handler) const {return [ref_holder(this->aci), handler(std::forward<F>(handler))](const auto& ec) {handler(ec);};}
	template<typename F> handler_with_error_size make_handler_error_size(F&& handler) const
		{return [ref_holder(this->aci), handler(std::forward<F>(handler))](const auto& ec, auto bytes_transferred) {handler(ec, bytes_transferred);};}
#else
	#if ASIO_VERSION >= 101100
	template<typename F> void post(const F& handler) {auto ref_holder(aci); asio::post(io_context_, [=]() {(void) ref_holder; handler();});}
	template<typename F> void defer(const F& handler) {auto ref_holder(aci); asio::defer(io_context_, [=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch(const F& handler) {auto ref_holder(aci); asio::dispatch(io_context_, [=]() {(void) ref_holder; handler();});}
	template<typename F> void post_strand(asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); asio::post(strand, [=]() {(void) ref_holder; handler();});}
	template<typename F> void defer_strand(asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); asio::defer(strand, [=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); asio::dispatch(strand, [=]() {(void) ref_holder; handler();});}
	#else
	template<typename F> void post(const F& handler) {auto ref_holder(aci); io_context_.post([=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch(const F& handler) {auto ref_holder(aci); io_context_.dispatch([=]() {(void) ref_holder; handler();});}
	template<typename F> void post_strand(asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); strand.post([=]() {(void) ref_holder; handler();});}
	template<typename F> void dispatch_strand(asio::io_context::strand& strand, const F& handler) {auto ref_holder(aci); strand.dispatch([=]() {(void) ref_holder; handler();});}
	#endif

	template<typename F> handler_with_error make_handler_error(const F& handler) const {auto ref_holder(aci); return [=](const asio::error_code& ec) {(void) ref_holder; handler(ec);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{auto ref_holder(aci); return [=](const asio::error_code& ec, size_t bytes_transferred) {(void) ref_holder; handler(ec, bytes_transferred);};}
#endif

	bool is_async_calling() const {return !aci.unique();}
	bool is_last_async_call() const {return aci.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

private:
	std::shared_ptr<char> aci; //asynchronous calling indicator
};
#else
class tracked_executor : public executor
{
protected:
	tracked_executor(asio::io_context& io_context_) : executor(io_context_), async_calling(false) {}

public:
	inline bool is_async_calling() const {return async_calling;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {async_calling = value;}

private:
	bool async_calling;
};
#endif

} //namespace

#endif /* _ASCS_TRACKED_EXECUTOR_H_ */
