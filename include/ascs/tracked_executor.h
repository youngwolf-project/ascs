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

#include "config.h"
#include "executor.h"

namespace ascs
{
	
#if 0 == ASCS_DELAY_CLOSE
class async_call_tracker
{
protected:
	bool is_async_calling() const {return aci.use_count() > 1;}
	bool is_last_async_call() const {return aci.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

#if (_MSVC_LANG > 201103L || __cplusplus > 201103L)
	template<typename F> auto track_async_call(F&& handler) const -> decltype(auto)
		{return [ref_holder(aci), handler(std::forward<F>(handler))](auto&&... args) -> decltype(auto) {return handler(std::forward<decltype(args)>(args)...);};}
#else
	template<typename F, typename = typename std::enable_if<!std::is_reference<F>::value>::type> struct track_forwarder
	{
		std::shared_ptr<char> aci;
		F handler;
		template<typename... Args> auto operator()(Args&&... args) -> decltype(handler(std::forward<Args>(args)...)) {return handler(std::forward<Args>(args)...);}
	};
	template<typename F> track_forwarder<F> track_async_call(F&& handler) const {return {aci, std::forward<F>(handler)};}
#endif

private:
	std::shared_ptr<char> aci{std::make_shared<char>('\0')}; //asynchronous calling indicator
};
using tracked_executor = executor_base<async_call_tracker>;
#else
class dummy_async_call_tracker : public forwarder
{
protected:
	dummy_async_call_tracker() = default;
	~dummy_async_call_tracker() = default;

	inline constexpr bool is_async_calling() const {return aci;}
	inline constexpr bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {aci = value;}

private:
	bool aci{false}; //asynchronous calling indicator
};
using tracked_executor = executor_base<dummy_async_call_tracker>;
#endif

} //namespace

#endif /* _ASCS_TRACKED_EXECUTOR_H_ */
