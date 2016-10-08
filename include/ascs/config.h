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
 * 2016.9.25	version 1.0.0
 * Based on st_asio_wrapper 1.2.0.
 * Directory structure refactoring.
 * Classes renaming, remove 'st_', 'tcp_' and 'udp_' prefix.
 * File renaming, remove 'st_asio_wrapper_' prefix.
 * Distinguish TCP and UDP related classes and files by tcp/udp namespace and tcp/udp directory.
 * Need c++14, if your compiler detected duplicated 'shared_mutex' definition, please define ASCS_HAS_STD_SHARED_MUTEX macro.
 * Need to define ASIO_STANDALONE and ASIO_HAS_STD_CHRONO macros.
 *
 * 2016.10.8	version 1.1.0
 * Support concurrent queue (https://github.com/cameron314/concurrentqueue), it's lock-free.
 * Define ASCS_USE_CONCURRENT_QUEUE macro to use your personal message queue. 
 * Define ASCS_USE_CONCURRE macro to use concurrent queue, otherwise ascs::list will be used as the message queue.
 * Drop original congestion control (because it cannot totally resolve dead loop) and add a semi-automatic congestion control.
 * Demonstrate how to use the new semi-automatic congestion control (echo_server, echo_client, pingpong_server and pingpong_client).
 * Drop post_msg_buffer and corresponding functions (like post_msg()) and timer (ascs::socket::TIMER_HANDLE_POST_BUFFER).
 * Optimize locks on message sending and dispatching.
 * Add enum shutdown_states.
 * Rename class ascs::std_list to ascs::list.
 * ascs::timer now can be used independently.
 * Add a new type ascs::st_timer::tid to represent timer ID.
 * Add a new packer--fixed_length_packer.
 * Add a new class--message_queue.
 *
 */

#ifndef _ASCS_CONFIG_H_
#define _ASCS_CONFIG_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#define ASCS_VER		10100	//[x]xyyzz -> [x]x.[y]y.[z]z
#define ASCS_VERSION	"1.1.0"

//asio and compiler check
#ifdef _MSC_VER
	#define ASCS_SF "%Iu" //printing format for 'size_t'
	static_assert(_MSC_VER >= 1900, "ascs need Visual C++ 14.0 or higher.");
#elif defined(__GNUC__)
	#define ASCS_SF "%zu" //printing format for 'size_t'
	#ifdef __clang__
		static_assert(__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 4), "ascs need Clang 3.4 or higher.");
	#else
		static_assert(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9), "ascs need GCC 4.9 or higher.");
		#if __GNUC__ > 5
		#define ASCS_HAS_STD_SHARED_MUTEX
		#endif
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
//will be hooked by ascs::object, this can avoid the object been freed during asynchronous call.
//#define ASCS_ENHANCED_STABILITY

//if defined, asio::steady_timer will be used in ascs::timer, otherwise, asio::system_timer will be used.
//#define ASCS_USE_STEADY_TIMER

//after this duration, this socket can be freed from the heap or reused,
//you must define this macro as a value, not just define it, the value means the duration, unit is second.
//if macro ST_ASIO_ENHANCED_STABILITY been defined, this macro will always be zero.
#ifdef ASCS_ENHANCED_STABILITY
#if defined(ASCS_DELAY_CLOSE) && ASCS_DELAY_CLOSE != 0
#warning ASCS_DELAY_CLOSE will always be zero if ASCS_ENHANCED_STABILITY macro been defined.
#endif
#undef ASCS_DELAY_CLOSE
#define ASCS_DELAY_CLOSE 0
#else
#ifndef ASCS_DELAY_CLOSE
#define ASCS_DELAY_CLOSE	5 //seconds
#endif
static_assert(ASCS_DELAY_CLOSE > 0, "ASCS_DELAY_CLOSE must be bigger than zero.");
#endif

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
#ifndef ASCS_REUSE_OBJECT
	#ifndef ASCS_FREE_OBJECT_INTERVAL
	#define ASCS_FREE_OBJECT_INTERVAL	60 //seconds
	#elif ST_ASIO_FREE_OBJECT_INTERVAL <= 0
		#error free object interval must be bigger than zero.
	#endif
#endif

//define ASCS_CLEAR_OBJECT_INTERVAL macro to let object_pool to invoke clear_obsoleted_object() automatically and periodically
//this feature may affect performance with huge number of objects, so re-write tcp::server_socket_base::on_recv_error and invoke object_pool::del_object()
//is recommended for long-term connection system, but for short-term connection system, you are recommended to open this feature.
//you must define this macro as a value, not just define it, the value means the interval, unit is second
//#define ASCS_CLEAR_OBJECT_INTERVAL		60 //seconds
#if defined(ASCS_CLEAR_OBJECT_INTERVAL) && ASCS_CLEAR_OBJECT_INTERVAL <= 0
	#error clear object interval must be bigger than zero.
#endif

//IO thread number
//listening, msg sending and receiving, msg handling(on_msg_handle() and on_msg()), all timers(include user timers) and other asynchronous calls(ascs::object::post())
//will use these threads, so keep big enough, no empirical value I can suggest, you must try to find it out in your own environment
#ifndef ASCS_SERVICE_THREAD_NUM
#define ASCS_SERVICE_THREAD_NUM	8
#endif
static_assert(ASCS_SERVICE_THREAD_NUM > 0, "service thread number be bigger than zero.");

//graceful shutdown must finish within this duration, otherwise, socket will be forcedly shut down.
#ifndef ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION
#define ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION	5 //seconds
#endif
static_assert(ASCS_GRACEFUL_SHUTDOWN_MAX_DURATION > 0, "graceful shutdown duration must be bigger than zero.");

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

//ConcurrentQueue is lock-free, please refer to https://github.com/cameron314/concurrentqueue
#ifdef ASCS_USE_CUSTOM_QUEUE
#elif defined(ASCS_USE_CONCURRENT_QUEUE) //if ASCS_USE_CONCURRENT_QUEUE macro not defined, ascs will use 'list' as the message queue, it's not thread safe, so need locks.
#include <concurrentqueue.h>
#endif
//configurations

#endif /* _ASCS_CONFIG_H_ */
