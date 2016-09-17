/*
 * config.h
 *
 *  Created on: 2016-9-14
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * ascs top header file.
 *
 * license: www.boost.org/LICENSE_1_0.txt
 *
 * 2016.9.13	version 1.0.0
 * Based on st_asio_wrapper 1.1.3.
 * Directory structure refactoring.
 * Classes renaming, remove 'st_', 'tcp_' and 'udp_' prefix.
 * File renaming, remove 'st_asio_wrapper_' prefix.
 * Distinguish TCP and UDP related classes and files by tcp/udp namespace and tcp/udp directory.
 * Need c++14, if your compiler detected duplicated 'shared_mutex' definition, please define ASCS_HAS_STD_SHARED_MUTEX macro.
 * Need to define ASIO_STANDALONE and ASIO_HAS_STD_CHRONO macros.
 *
 */

#ifndef _ASCS_CONFIG_H_
#define _ASCS_CONFIG_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#define ASCS_VER		10000	//[x]xyyzz -> [x]x.[y]y.[z]z
#define ASCS_VERSION	"1.0.0"

//asio and compiler check
#ifdef _MSC_VER
	static_assert(_MSC_VER >= 1900, "ascs need Visual C++ 14.0 or higher.");
#elif defined(__GNUC__)
	#ifdef __clang__
		static_assert(__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 4), "ascs need Clang 3.4 or higher.");
	#else
		static_assert(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9), "ascs need GCC 4.9 or higher.");
	#endif

	#if !defined(__cplusplus) || __cplusplus <= 201103L
		#error ascs at least need c++14.
	#endif
#else
	#error ascs only support Visual C++, GCC and Clang.
#endif

static_assert(ASIO_VERSION >= 101001, "ascs need asio 1.10.1 or higher.");
//asio and compiler check

//configurations
#ifndef ASCS_SERVER_IP
#define ASCS_SERVER_IP			"127.0.0.1"
#endif
#ifndef ASCS_SERVER_PORT
#define ASCS_SERVER_PORT		5050
#endif
static_assert(ASCS_SERVER_PORT > 0, "server port must be bigger than zero.");

//msg send and recv buffer's maximum size (list::size()), corresponding buffers are expanded dynamically, which means only allocate memory when needed.
#ifndef ASCS_MAX_MSG_NUM
#define ASCS_MAX_MSG_NUM		1024
#endif
static_assert(ASCS_MAX_MSG_NUM > 0, "message capacity must be bigger than zero.");

//buffer (on stack) size used when writing logs.
#ifndef ASCS_UNIFIED_OUT_BUF_NUM
#define ASCS_UNIFIED_OUT_BUF_NUM	2048
#endif

//use customized log system (you must provide unified_out::fatal_out/error_out/warning_out/info_out/debug_out)
//#define ASCS_CUSTOM_LOG

//don't write any logs.
//#define ASCS_NO_UNIFIED_OUT

//if defined, service_pump will catch exceptions for asio::io_service::run(), and all function objects in asynchronous calls
//will be hooked by object, this can avoid the object been freed during asynchronous call.
//#define ASCS_ENHANCED_STABILITY

//if defined, asio::steady_timer will be used in ascs::timer, otherwise, asio::system_timer will be used.
//#define ASCS_USE_STEADY_TIMER

//full statistic include time consumption, or only numerable informations will be gathered
//#define ASCS_FULL_STATISTIC

//when got some msgs, not call on_msg(), but asynchronously dispatch them, on_msg_handle() will be called later.
//#define ASCS_FORCE_TO_USE_MSG_RECV_BUFFER

//after every msg sent, call ascs::socket::on_msg_send()
//#define ASCS_WANT_MSG_SEND_NOTIFY

//after sending buffer became empty, call ascs::socket::on_all_msg_send()
//#define ASCS_WANT_ALL_MSG_SEND_NOTIFY

//when link down, msgs in receiving buffer (already unpacked) will be discarded.
//#define ASCS_DISCARD_MSG_WHEN_LINK_DOWN

//max number of objects object_pool can hold.
#ifndef ASCS_MAX_OBJECT_NUM
#define ASCS_MAX_OBJECT_NUM	4096
#endif
static_assert(ASCS_MAX_OBJECT_NUM > 0, "object capacity must be bigger than zero.");

//if defined, objects will never be freed, but remain in object_pool waiting for reuse.
//#define ASCS_REUSE_OBJECT

//define ASCS_REUSE_OBJECT macro will enable object pool, all objects in invalid_object_can will never be freed, but kept for reuse,
//otherwise, object_pool will free objects in invalid_object_can automatically and periodically, ASCS_FREE_OBJECT_INTERVAL means the interval, unit is second,
//see invalid_object_can at the end of object_pool class for more details.
//please note that even if you defined ASCS_REUSE_OBJECT macro, ASCS_FREE_OBJECT_INTERVAL macro is still useful, it will make object_pool
//to close (just close, not free, Object must has close function which takes no parameter) objects automatically and periodically for saving SOCKET handles.
#ifndef ASCS_REUSE_OBJECT
	#ifndef ASCS_FREE_OBJECT_INTERVAL
	#define ASCS_FREE_OBJECT_INTERVAL	10 //seconds
	#endif
#endif
#if defined(ASCS_FREE_OBJECT_INTERVAL) && ASCS_FREE_OBJECT_INTERVAL <= 0
	#error free/close object interval must be bigger than zero.
#endif

//define ASCS_CLEAR_OBJECT_INTERVAL macro to let object_pool to invoke clear_obsoleted_object() automatically and periodically
//this feature may affect performance with huge number of objects, so re-write tcp::server_socket_base::on_recv_error and invoke object_pool::del_object()
//is recommended for long-term connection system, but for short-term connection system, you are recommended to open this feature.
//you must define this macro as a value, not just define it, the value means the interval, unit is second
//#define ASCS_CLEAR_OBJECT_INTERVAL		60 //seconds
#if defined(ASCS_CLEAR_OBJECT_INTERVAL) && ASCS_CLEAR_OBJECT_INTERVAL <= 0
	#error clear object interval must be bigger than zero.
#endif

//after this duration, corresponding objects in invalid_object_can can be freed from the heap or reused,
//you must define this macro as a value, not just define it, the value means the duration, unit is second.
//if macro ASCS_ENHANCED_STABILITY been defined, this macro is useless, object's life time is always zero.
#ifdef ASCS_ENHANCED_STABILITY
	#if defined(ASCS_OBSOLETED_OBJECT_LIFE_TIME) && ASCS_OBSOLETED_OBJECT_LIFE_TIME != 0
		#warning ASCS_OBSOLETED_OBJECT_LIFE_TIME will always be zero if ASCS_ENHANCED_STABILITY macro been defined.
	#endif
#else
	#ifndef ASCS_OBSOLETED_OBJECT_LIFE_TIME
	#define ASCS_OBSOLETED_OBJECT_LIFE_TIME	5 //seconds
	#endif
	static_assert(ASCS_OBSOLETED_OBJECT_LIFE_TIME > 0, "obsoleted object's life time must be bigger than zero.");
#endif

//IO thread number
//listen, msg send and receive, msg handle(on_msg_handle() and on_msg()) will use these threads
//keep big enough, no empirical value I can suggest, you must try to find it in your own environment
#ifndef ASCS_SERVICE_THREAD_NUM
#define ASCS_SERVICE_THREAD_NUM	8
#endif
static_assert(ASCS_SERVICE_THREAD_NUM > 0, "service thread number be bigger than zero.");

//graceful closing must finish within this duration, otherwise, socket will be forcedly closed.
#ifndef ASCS_GRACEFUL_CLOSE_MAX_DURATION
#define ASCS_GRACEFUL_CLOSE_MAX_DURATION	5 //seconds
#endif
static_assert(ASCS_GRACEFUL_CLOSE_MAX_DURATION > 0, "graceful close duration must be bigger than zero.");

//if connecting (or reconnecting) failed, delay how much milliseconds before reconnecting, negative value means stop reconnecting,
//you can also rewrite ascs::tcp::connector_base::prepare_reconnect(), and return a negative value.
#ifndef ASCS_RECONNECT_INTERVAL
#define ASCS_RECONNECT_INTERVAL	500 //millisecond(s)
#endif

//how many async_accept delivery concurrently
#ifndef ASCS_ASYNC_ACCEPT_NUM
#define ASCS_ASYNC_ACCEPT_NUM	1
#endif
static_assert(ASCS_ASYNC_ACCEPT_NUM > 0, "async accept number must be bigger than zero.");

//in set_server_addr, if the IP is empty, ASCS_TCP_DEFAULT_IP_VERSION will define the IP version, or the IP version will be deduced by the IP address.
//asio::ip::tcp::v4() means ipv4 and asio::ip::tcp::v6() means ipv6.
#ifndef ASCS_TCP_DEFAULT_IP_VERSION
#define ASCS_TCP_DEFAULT_IP_VERSION asio::ip::tcp::v4()
#endif
#ifndef ASCS_UDP_DEFAULT_IP_VERSION
#define ASCS_UDP_DEFAULT_IP_VERSION asio::ip::udp::v4()
#endif

//close port reuse
//#define ASCS_NOT_REUSE_ADDRESS

//If your compiler detected duplicated 'shared_mutex' definition, please define this macro.
#ifndef ASCS_HAS_STD_SHARED_MUTEX
namespace std {typedef shared_timed_mutex shared_mutex;}
#endif
//configurations

#endif /* _ASCS_CONFIG_H_ */
