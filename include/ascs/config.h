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
 * license: http://think-async.com/ (the current is www.boost.org/LICENSE_1_0.txt)
 *
 * Known issues:
 * 1. since 1.1.0, concurrentqueue is not a FIFO queue (it is by design), navigate to the following links for more deatils:
 *  https://github.com/cameron314/concurrentqueue/issues/6
 *  https://github.com/cameron314/concurrentqueue/issues/52
 *  if you're using concurrentqueue, please play attention, this is by design.
 * 2. since 1.1.5 until 1.2, heartbeat function cannot work properly between windows (at least win-10) and Ubuntu (at least Ubuntu-16.04).
 * 3. since 1.1.5 until 1.2, UDP doesn't support heartbeat because UDP doesn't support OOB data.
 * 4. since 1.1.5 until 1.2, SSL doesn't support heartbeat because SSL doesn't support OOB data.
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
 * Add a new type ascs::timer::tid to represent timer ID.
 * Add a new packer--fixed_length_packer.
 * Add a new class--message_queue.
 *
 * 2016.10.16	version 1.1.1
 * Support non-lock queue, it's totally not thread safe and lock-free, it can improve IO throughput with particular business.
 * Demonstrate how and when to use non-lock queue as the input and output message buffer.
 * Queues (and their internal containers) used as input and output message buffer are now configurable (by macros or template arguments).
 * New macros--ASCS_INPUT_QUEUE, ASCS_INPUT_CONTAINER, ASCS_OUTPUT_QUEUE and ASCS_OUTPUT_CONTAINER.
 * Drop macro ASCS_USE_CONCURRENT_QUEUE, rename macro ASCS_USE_CONCURRE to ASCS_HAS_CONCURRENT_QUEUE.
 * In contrast to non_lock_queue, split message_queue into lock_queue and lock_free_queue.
 * Move container related classes and functions from base.h to container.h.
 * Improve efficiency in scenarios of low throughput like pingpong test.
 * Replaceable packer/unpacker now support replaceable_buffer (an alias of auto_buffer) and shared_buffer to be their message type.
 * Move class statistic and obj_with_begin_time out of ascs::socket to reduce template tiers.
 *
 * 2016.11.1	version 1.1.2
 * Fix bug: ascs::list cannot be moved properly via moving constructor.
 * Use ASCS_DELAY_CLOSE instead of ASCS_ENHANCED_STABILITY macro to control delay close duration,
 *  0 is an equivalent of defining ASCS_ENHANCED_STABILITY, other values keep the same meanings as before.
 * Move ascs::socket::closing related logic to ascs::object.
 * Make ascs::socket::id(uint_fast64_t) private to avoid changing IDs by users.
 * Call close at the end of shutdown function, just for safety.
 * Add move capture in lambda.
 * Optimize lambda expressions.
 *
 * 2016.11.13	version 1.1.3
 * Introduce lock-free mechanism for some appropriate logics (many requesters, only one can succeed, others will fail rather than wait).
 * Remove all mutex (except mutex in object_pool, service_pump, lock_queue and udp::socket).
 * Sharply simplified timer class.
 *
 * 2016.12.6	version 1.1.4
 * Drop unnecessary macro definition (ASIO_HAS_STD_CHRONO).
 * Simplify header files' dependence.
 * Add Visual C++ solution and project files (Visual C++ 14.0).
 * Monitor time consumptions for message packing and unpacking.
 * Fix bug: pop_first_pending_send_msg and pop_first_pending_recv_msg cannot work.
 *
 * 2017.1.1		version 1.1.5
 * Support heartbeat (via OOB data), see ASCS_HEARTBEAT_INTERVAL macro for more details.
 * Support scatter-gather buffers when receiving messages, this feature needs modification of i_unpacker, you must explicitly define
 *  ASCS_SCATTERED_RECV_BUFFER macro to open it, this is just for compatibility.
 * Simplify lock-free mechanism and use std::atomic_flag instead of std::atomic_size_t.
 * Optimize container insertion (use series of emplace functions instead).
 * Demo echo_client support alterable number of sending thread (before, it's a hard code 16).
 * Fix bug: In extreme cases, messages may get starved in receive buffer and will not be dispatched until arrival of next message.
 * Fix bug: In extreme cases, messages may get starved in send buffer and will not be sent until arrival of next message.
 * Fix bug: Sometimes, connector_base cannot reconnect to the server after link broken.
 *
 * ===============================================================
 * 2017.5.30		version 1.2.0
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * Virtual function reset_state in i_packer and i_unpacker have been renamed to reset.
 * Virtual function is_send_allowed has been renamed to is_ready, it also means ready to receive messages
 *  since message sending is not suspendable any more.
 * Virtual function on_msg_handle has been changed, the link_down variable will not be presented any more.
 * Interface i_server::del_client has been renamed to i_server::del_socket.
 * Function inner_packer and inner_unpacker have been renamed to packer and unpacker.
 * All add_client functions have been renamed to add_socket.
 *
 * HIGHLIGHT:
 * Support object restoration (on server side), see macro ASCS_RESTORE_OBJECT for more details.
 * Re-implement heartbeat function, use user data (so need packer and unpacker's support) instead of OOB, now there will be no any
 *  limitations for using heartbeat.
 * Refactor and optimize ssl objects, now ssl::connector_base and ssl::server_socket_base are reusable,
 *  just need you to define macro ASCS_REUSE_SSL_STREAM.
 *
 * FIX:
 * Before on_close() to be called, socket::start becomes available (so user can call it falsely).
 * If a timer failed or stopped by callback, its status not set properly (should be set to TIMER_CANCELED).
 * Make ssl shutting down thread safe.
 *
 * ENHANCEMENTS:
 * Virtual function i_packer::pack_heartbeat been introduced to support heartbeat function.
 * Interface i_server::restore_socket been added, see macro ASCS_RESTORE_OBJECT for more details.
 * Be able to manually start heartbeat function without defining macro ASCS_HEARTBEAT_INTERVAL, see demo echo_server for more details.
 * Support sync mode when sending messages, it's also a type of congestion control like safe_send_msg.
 * Expand enum tcp::socket::shutdown_states, now it's able to represent all SOCKET status (connected, shutting down and broken),
 *  so rename it to link_status.
 * Enhance class timer (function is_timer and another stop_all_timer been added).
 * Support all buffer types that asio supported when receiving messages, use macro ASCS_RECV_BUFFER_TYPE (it's the only way) to define the buffer type,
 *  it's effective for both TCP and UDP.
 *
 * DELETION:
 * Drop ASCS_HAS_STD_SHARED_MUTEX macro.
 * Drop ASCS_DISCARD_MSG_WHEN_LINK_DOWN macro and related logic, because it brings complexity and race condition,
 *  and are not very useful.
 * Not support pausing message sending and dispatching any more, because they bring complexity and race condition,
 *  and are not very useful.
 *
 * REFACTORING:
 * Move heartbeat function from connector_base and server_socket_base to socket, so introduce new virtual function
 *  virtual void on_heartbeat_error() to socket, subclass need to implement it.
 * Move async shutdown function from connector_base and server_socket_base to socket, so introduce new virtual function
 *  virtual void on_async_shutdown_error() to socket, subclass need to implement it.
 * Move handshake from ssl::server_base to ssl::server_socket_base.
 *
 * REPLACEMENTS:
 * Use std::mutex instead of std::shared_mutex (the former is more efficient in ascs' usage scenario), thus c++11 is enough for ascs.
 * Move macro ASCS_SCATTERED_RECV_BUFFER from ascs to ascs::ext, because it doesn't belong to ascs any more after introduced macro ASCS_RECV_BUFFER_TYPE.
 * Move directory include/ascs/ssl into directory include/ascs/tcp/, because ssl is based on ascs::tcp.
 *
 */

#ifndef _ASCS_CONFIG_H_
#define _ASCS_CONFIG_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#define ASCS_VER		10200	//[x]xyyzz -> [x]x.[y]y.[z]z
#define ASCS_VERSION	"1.2.0"

//asio and compiler check
#ifdef _MSC_VER
	#define ASCS_SF "%Iu" //format used to print 'size_t'
	static_assert(_MSC_VER >= 1700, "ascs needs Visual C++ 11.0 or higher.");

	#if _MSC_VER >= 1800
		#define ASCS_HAS_TEMPLATE_USING
	#endif
#elif defined(__GNUC__)
	#define ASCS_SF "%zu" //format used to print 'size_t'
	#ifdef __x86_64__
	#define ASCS_LLF "%lu" //format used to print 'uint_fast64_t'
	#endif
	#ifdef __clang__
		static_assert(__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 1), "ascs needs Clang 3.1 or higher.");
	#else
		static_assert(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6), "ascs needs GCC 4.6 or higher.");
	#endif

	#if !defined(__GXX_EXPERIMENTAL_CXX0X__) && (!defined(__cplusplus) || __cplusplus < 201103L)
		#error ascs needs c++11 or higher.
	#endif

	#define ASCS_HAS_TEMPLATE_USING
#else
	#error ascs only support Visual C++, GCC and Clang.
#endif

#ifndef ASCS_LLF
#define ASCS_LLF "%llu" //format used to print 'uint_fast64_t'
#endif

static_assert(ASIO_VERSION >= 101001, "ascs needs asio 1.10.1 or higher.");
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
//a value equal to zero will cause ascs to use a mechanism to guarantee 100% safety when reusing or freeing this socket,
//ascs will hook all async calls to avoid this socket to be reused or freed before all async calls finish
//or been interrupted (of course, this mechanism will slightly impact efficiency).
#ifndef ASCS_DELAY_CLOSE
#define ASCS_DELAY_CLOSE	0 //seconds, guarantee 100% safety when reusing or freeing this socket
#endif
static_assert(ASCS_DELAY_CLOSE >= 0, "delay close duration must be bigger than or equal to zero.");

//full statistic include time consumption, or only numerable informations will be gathered
//#define ASCS_FULL_STATISTIC

//when got some msgs, not call on_msg(), but asynchronously dispatch them, on_msg_handle() will be called later.
//#define ASCS_FORCE_TO_USE_MSG_RECV_BUFFER

//after every msg sent, call ascs::socket::on_msg_send()
//#define ASCS_WANT_MSG_SEND_NOTIFY

//after sending buffer became empty, call ascs::socket::on_all_msg_send()
//#define ASCS_WANT_ALL_MSG_SEND_NOTIFY

//max number of objects object_pool can hold.
#ifndef ASCS_MAX_OBJECT_NUM
#define ASCS_MAX_OBJECT_NUM	4096
#endif
static_assert(ASCS_MAX_OBJECT_NUM > 0, "object capacity must be bigger than zero.");

//if defined, objects will never be freed, but remain in object_pool waiting for reuse.
//#define ASCS_REUSE_OBJECT

//this macro has the same effects as macro ASCS_REUSE_OBJECT (it will overwrite the latter), except:
//reuse will not happen when create new connections, but just happen when invoke i_server::restore_socket.
//you may ask, for what purpose we introduced this feature?
//consider following situation:
//if a specific link is down, and the client has reconnected to the server, on the server side, how does the new server_socket_base
//restore all user data (because you don't want to nor need to reestablish them) and keep its id?
//before this feature been introduced, it's almost impossible.
//according to above explanation, we know that:
//1. like object pool, only objects in invalid_object_can can be restored;
//2. client need to inform server_socket_base the former id (or something else which can be used to calculate the former id
//   on the server side) after reconnected to the server;
//3. this feature needs user's support (send former id to server side on client side, invoke i_server::restore_socket in server_socket_base);
//4. do not define this macro on client side nor for UDP.
//#define ASCS_RESTORE_OBJECT

//define ASCS_REUSE_OBJECT or ASCS_RESTORE_OBJECT macro will enable object pool, all objects in invalid_object_can will
// never be freed, but kept for reuse, otherwise, object_pool will free objects in invalid_object_can automatically and periodically,
//ASCS_FREE_OBJECT_INTERVAL means the interval, unit is second, see invalid_object_can in object_pool class for more details.
#if !defined(ASCS_REUSE_OBJECT) && !defined(ASCS_RESTORE_OBJECT)
	#ifndef ASCS_FREE_OBJECT_INTERVAL
	#define ASCS_FREE_OBJECT_INTERVAL	60 //seconds
	#elif ASCS_FREE_OBJECT_INTERVAL <= 0
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
#define ASCS_ASYNC_ACCEPT_NUM	16
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

//ConcurrentQueue is lock-free, please refer to https://github.com/cameron314/concurrentqueue
#ifdef ASCS_HAS_CONCURRENT_QUEUE
	#include <concurrentqueue.h>

	#ifdef ASCS_HAS_TEMPLATE_USING
	template<typename T> using concurrent_queue = moodycamel::ConcurrentQueue<T>;
	#else
	template<typename T> class concurrent_queue : public moodycamel::ConcurrentQueue<T>
	{
	public:
		concurrent_queue() {}
		explicit concurrent_queue(size_t capacity) : moodycamel::ConcurrentQueue<T>(capacity) {}
	};
	#endif

	#ifndef ASCS_INPUT_QUEUE
	#define ASCS_INPUT_QUEUE lock_free_queue
	#endif
	#ifndef ASCS_INPUT_CONTAINER
	#define ASCS_INPUT_CONTAINER concurrent_queue
	#endif
	#ifndef ASCS_OUTPUT_QUEUE
	#define ASCS_OUTPUT_QUEUE lock_free_queue
	#endif
	#ifndef ASCS_OUTPUT_CONTAINER
	#define ASCS_OUTPUT_CONTAINER concurrent_queue
	#endif
#else
	#ifndef ASCS_INPUT_QUEUE
	#define ASCS_INPUT_QUEUE lock_queue
	#endif
	#ifndef ASCS_INPUT_CONTAINER
	#define ASCS_INPUT_CONTAINER list
	#endif
	#ifndef ASCS_OUTPUT_QUEUE
	#define ASCS_OUTPUT_QUEUE lock_queue
	#endif
	#ifndef ASCS_OUTPUT_CONTAINER
	#define ASCS_OUTPUT_CONTAINER list
	#endif
#endif
//we also can control the queues (and their containers) via template parameters on calss 'connector_base'
//'server_socket_base', 'ssl::connector_base' and 'ssl::server_socket_base'.
//we even can let a socket to use different queue (and / or different container) for input and output via template parameters.

//buffer type used when receiving messages (unpacker's prepare_next_recv() need to return this type)
#ifndef ASCS_RECV_BUFFER_TYPE
#define ASCS_RECV_BUFFER_TYPE asio::mutable_buffers_1
#endif

#ifndef ASCS_HEARTBEAT_INTERVAL
#define ASCS_HEARTBEAT_INTERVAL	0 //second(s), disable heartbeat by default, just for compatibility
#endif
//at every ASCS_HEARTBEAT_INTERVAL second(s):
// 1. tcp::socket_base will send an heartbeat if no messages been sent within this interval,
// 2. tcp::socket_base will check the link's connectedness, see ASCS_HEARTBEAT_MAX_ABSENCE macro for more details.
//less than or equal to zero means disable heartbeat, then you can send and check heartbeat with you own logic by calling
//tcp::socket_base::check_heartbeat (do above steps one time) or tcp::socket_base::start_heartbeat (do above steps regularly).

#ifndef ASCS_HEARTBEAT_MAX_ABSENCE
#define ASCS_HEARTBEAT_MAX_ABSENCE	3 //times of ASCS_HEARTBEAT_INTERVAL
#endif
static_assert(ASCS_HEARTBEAT_MAX_ABSENCE > 0, "heartbeat absence must be bigger than zero.");
//if no any messages (include heartbeat) been received within ASCS_HEARTBEAT_INTERVAL * ASCS_HEARTBEAT_MAX_ABSENCE second(s), shut down the link.

//#define ASCS_REUSE_SSL_STREAM
//if you need ssl::connector_base to be able to reconnect the server, or to open object pool in ssl::object_pool, you must define this macro.
//i tried many ways, onle one way can make asio::ssl::stream reusable, which is:
// don't call any shutdown functions of asio::ssl::stream, just call asio::ip::tcp::socket's shutdown function,
// this seems not a normal procedure, but it works, i believe that asio's defect caused this problem.
//configurations

#endif /* _ASCS_CONFIG_H_ */
