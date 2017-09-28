/*
 * base.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * interfaces, free functions, convenience and logs etc.
 */

#ifndef _ASCS_BASE_H_
#define _ASCS_BASE_H_

#include <stdio.h>
#include <stdarg.h>

#include <list>
#include <vector>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>

#include <asio.hpp>

#include "config.h"

namespace ascs
{

#if defined(_MSC_VER) && _MSC_VER <= 1800 //terrible VC++
inline bool operator==(asio::error::basic_errors _Left, const asio::error_code& _Right) {return _Left == _Right.value();}
inline bool operator!=(asio::error::basic_errors _Left, const asio::error_code& _Right) {return !(_Left == _Right);}

inline bool operator==(asio::error::misc_errors _Left, const asio::error_code& _Right) {return _Left == _Right.value();}
inline bool operator!=(asio::error::misc_errors _Left, const asio::error_code& _Right) {return !(_Left == _Right);}
#endif

class scope_atomic_lock : public asio::noncopyable
{
public:
	scope_atomic_lock(std::atomic_flag& atomic_) : _locked(false), atomic(atomic_) {lock();} //atomic_ must has been initialized with false
	~scope_atomic_lock() {unlock();}

	void lock() {if (!_locked) _locked = !atomic.test_and_set(std::memory_order_acq_rel);}
	void unlock() {if (_locked) atomic.clear(std::memory_order_release); _locked = false;}
	bool locked() const {return _locked;}

private:
	bool _locked;
	std::atomic_flag& atomic;
};

class service_pump;
class object;
namespace tcp
{
	class i_server
	{
	public:
		virtual service_pump& get_service_pump() = 0;
		virtual const service_pump& get_service_pump() const = 0;
		virtual bool del_socket(const std::shared_ptr<object>& socket_ptr) = 0;
		virtual bool restore_socket(const std::shared_ptr<object>& socket_ptr, uint_fast64_t id) = 0;
	};
} //namespace

class i_buffer
{
public:
	virtual ~i_buffer() {}

	virtual bool empty() const = 0;
	virtual size_t size() const = 0;
	virtual const char* data() const = 0;
};

//convert '->' operation to '.' operation
//user need to allocate object, and auto_buffer will free it
template<typename T> class auto_buffer
#if defined(_MSC_VER) && _MSC_VER <= 1800
	: public asio::noncopyable
#endif
{
public:
	typedef T* buffer_type;
	typedef const buffer_type buffer_ctype;

	auto_buffer() : buffer(nullptr) {}
	auto_buffer(buffer_type _buffer) : buffer(_buffer) {}
	auto_buffer(auto_buffer&& other) : buffer(other.buffer) {other.buffer = nullptr;}
	~auto_buffer() {clear();}

	auto_buffer& operator=(auto_buffer&& other) {clear(); swap(other); return *this;}

	buffer_type raw_buffer() const {return buffer;}
	void raw_buffer(buffer_type _buffer) {buffer = _buffer;}

	//the following five functions are needed by ascs
	bool empty() const {return nullptr == buffer || buffer->empty();}
	size_t size() const {return nullptr == buffer ? 0 : buffer->size();}
	const char* data() const {return nullptr == buffer ? nullptr : buffer->data();}
	void swap(auto_buffer& other) {std::swap(buffer, other.buffer);}
	void clear() {delete buffer; buffer = nullptr;}

protected:
	buffer_type buffer;
};
typedef auto_buffer<i_buffer> replaceable_buffer;

//convert '->' operation to '.' operation
//user need to allocate object, and shared_buffer will free it
template<typename T> class shared_buffer
{
public:
	typedef std::shared_ptr<T> buffer_type;
	typedef const buffer_type buffer_ctype;

	shared_buffer() {}
	shared_buffer(T* _buffer) {buffer.reset(_buffer);}
	shared_buffer(buffer_type _buffer) : buffer(_buffer) {}

#if defined(_MSC_VER) && _MSC_VER <= 1800
	shared_buffer(shared_buffer&& other) : buffer(std::move(other.buffer)) {}
	shared_buffer& operator=(shared_buffer&& other) {clear(); swap(other); return *this;}
#endif

	buffer_type raw_buffer() const {return buffer;}
	void raw_buffer(T* _buffer) {buffer.reset(_buffer);}
	void raw_buffer(buffer_ctype _buffer) {buffer = _buffer;}

	//the following five functions are needed by ascs
	bool empty() const {return !buffer || buffer->empty();}
	size_t size() const {return !buffer ? 0 : buffer->size();}
	const char* data() const {return !buffer ? nullptr : buffer->data();}
	void swap(shared_buffer& other) {buffer.swap(other.buffer);}
	void clear() {buffer.reset();}

protected:
	buffer_type buffer;
};
//not like auto_buffer, shared_buffer is copyable, but auto_buffer is a bit more efficient.
//packer or/and unpacker who used auto_buffer or shared_buffer as its msg type will be replaceable.

//packer concept
template<typename MsgType>
class i_packer
{
public:
	typedef MsgType msg_type;
	typedef const msg_type msg_ctype;

protected:
	virtual ~i_packer() {}

public:
	virtual void reset() {}
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) = 0;
	virtual msg_type pack_heartbeat() {return msg_type();}
	virtual char* raw_data(msg_type& msg) const {return nullptr;}
	virtual const char* raw_data(msg_ctype& msg) const {return nullptr;}
	virtual size_t raw_data_len(msg_ctype& msg) const {return 0;}

	msg_type pack_msg(const char* pstr, size_t len, bool native = false) {return pack_msg(&pstr, &len, 1, native);}
	msg_type pack_msg(const std::string& str, bool native = false) {return pack_msg(str.data(), str.size(), native);}
};
//packer concept

//just provide msg_type definition, you should not call any functions of it, but send msgs directly
template<typename MsgType>
class dummy_packer : public i_packer<MsgType>
{
public:
	using typename i_packer<MsgType>::msg_type;
	using typename i_packer<MsgType>::msg_ctype;

	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) {assert(false); return msg_type();}
};

//unpacker concept
namespace tcp
{
	template<typename MsgType>
	class i_unpacker
	{
	public:
		typedef MsgType msg_type;
		typedef const msg_type msg_ctype;
		typedef std::list<msg_type> container_type;
		typedef ASCS_RECV_BUFFER_TYPE buffer_type;

		bool stripped() const {return _stripped;}
		void stripped(bool stripped_) {_stripped = stripped_;}

	protected:
		i_unpacker() : _stripped(true) {}
		virtual ~i_unpacker() {}

	public:
		virtual void reset() = 0;
		//heartbeat must not be included in msg_can, otherwise you must handle heartbeat at where you handle normal messges.
		virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can) = 0;
		virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) = 0;
		virtual buffer_type prepare_next_recv() = 0;

	private:
		bool _stripped;
	};
} //namespace

namespace udp
{
	template<typename MsgType>
	class udp_msg : public MsgType
	{
	public:
		asio::ip::udp::endpoint peer_addr;

		udp_msg() {}
		udp_msg(const asio::ip::udp::endpoint& _peer_addr) : peer_addr(_peer_addr) {}
		udp_msg(const asio::ip::udp::endpoint& _peer_addr, const MsgType& msg) : MsgType(msg), peer_addr(_peer_addr) {}
		udp_msg(const asio::ip::udp::endpoint& _peer_addr, MsgType&& msg) : MsgType(std::move(msg)), peer_addr(_peer_addr) {}

		using MsgType::operator=;
		using MsgType::swap;
		void swap(udp_msg& other) {MsgType::swap(other); std::swap(peer_addr, other.peer_addr);}

#if defined(_MSC_VER) && _MSC_VER <= 1800
		udp_msg(udp_msg&& other) : MsgType(std::move(other)), peer_addr(std::move(other.peer_addr)) {}
		udp_msg& operator=(udp_msg&& other) {MsgType::clear(); swap(other); return *this;}
#endif
	};

	template<typename MsgType>
	class i_unpacker
	{
	public:
		typedef MsgType msg_type;
		typedef const msg_type msg_ctype;
		typedef std::list<udp_msg<msg_type>> container_type;
		typedef ASCS_RECV_BUFFER_TYPE buffer_type;

	protected:
		virtual ~i_unpacker() {}

	public:
		virtual void reset() {}
		//heartbeat must not be returned (use empty message instead), otherwise you must handle heartbeat at where you handle normal messges.
		virtual msg_type parse_msg(size_t bytes_transferred) = 0;
		virtual buffer_type prepare_next_recv() = 0;
	};
} //namespace
//unpacker concept

struct statistic
{
#ifdef ASCS_FULL_STATISTIC
	typedef std::chrono::system_clock::time_point stat_time;
	static stat_time now() {return std::chrono::system_clock::now();}
	typedef std::chrono::system_clock::duration stat_duration;
#else
	struct dummy_duration {dummy_duration& operator+=(const dummy_duration& other) {return *this;}}; //not a real duration, just satisfy compiler(d1 += d2)
	struct dummy_time {dummy_duration operator-(const dummy_time& other) {return dummy_duration();}}; //not a real time, just satisfy compiler(t1 - t2)

	typedef dummy_time stat_time;
	static stat_time now() {return stat_time();}
	typedef dummy_duration stat_duration;
#endif
	statistic() {reset();}

	void reset_number()
	{
		send_msg_sum = 0;
		send_byte_sum = 0;

		recv_msg_sum = 0;
		recv_byte_sum = 0;

		last_send_time = 0;
		last_recv_time = 0;

		establish_time = 0;
		break_time = 0;
	}

#ifdef ASCS_FULL_STATISTIC
	void reset() {reset_number(); reset_duration();}
	void reset_duration()
	{
		send_delay_sum = send_time_sum = pack_time_sum = stat_duration(0);

		dispatch_dealy_sum = recv_idle_sum = stat_duration(0);
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
		handle_time_1_sum = stat_duration(0);
#endif
		handle_time_2_sum = stat_duration(0);
		unpack_time_sum = stat_duration(0);
	}
#else
	void reset() {reset_number();}
#endif

	statistic& operator+=(const struct statistic& other)
	{
		send_msg_sum += other.send_msg_sum;
		send_byte_sum += other.send_byte_sum;
		send_delay_sum += other.send_delay_sum;
		send_time_sum += other.send_time_sum;
		pack_time_sum += other.pack_time_sum;

		recv_msg_sum += other.recv_msg_sum;
		recv_byte_sum += other.recv_byte_sum;
		dispatch_dealy_sum += other.dispatch_dealy_sum;
		recv_idle_sum += other.recv_idle_sum;
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
		handle_time_1_sum += other.handle_time_1_sum;
#endif
		handle_time_2_sum += other.handle_time_2_sum;
		unpack_time_sum += other.unpack_time_sum;

		return *this;
	}

	std::string to_string() const
	{
		std::ostringstream s;
#ifdef ASCS_FULL_STATISTIC
		s << "send corresponding statistic:\n"
			<< "message sum: " << send_msg_sum << std::endl
			<< "size in bytes: " << send_byte_sum << std::endl
			<< "send delay: " << std::chrono::duration_cast<std::chrono::duration<float>>(send_delay_sum).count() << std::endl
			<< "send duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(send_time_sum).count() << std::endl
			<< "pack duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(pack_time_sum).count() << std::endl
			<< "\nrecv corresponding statistic:\n"
			<< "message sum: " << recv_msg_sum << std::endl
			<< "size in bytes: " << recv_byte_sum << std::endl
			<< "dispatch delay: " << std::chrono::duration_cast<std::chrono::duration<float>>(dispatch_dealy_sum).count() << std::endl
			<< "recv idle duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(recv_idle_sum).count() << std::endl
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
			<< "on_msg duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(handle_time_1_sum).count() << std::endl
#endif
			<< "on_msg_handle duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(handle_time_2_sum).count() << std::endl
			<< "unpack duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(unpack_time_sum).count();
#else
		s << std::setfill('0') << "send corresponding statistic:\n"
			<< "message sum: " << send_msg_sum << std::endl
			<< "size in bytes: " << send_byte_sum << std::endl
			<< "\nrecv corresponding statistic:\n"
			<< "message sum: " << recv_msg_sum << std::endl
			<< "size in bytes: " << recv_byte_sum;
#endif
		return s.str();
	}

	//send corresponding statistic
	uint_fast64_t send_msg_sum; //not counted msgs in sending buffer
	uint_fast64_t send_byte_sum; //include data added by packer, not counted msgs in sending buffer
	stat_duration send_delay_sum; //from send_(native_)msg (exclude msg packing) to asio::async_write
	stat_duration send_time_sum; //from asio::async_write to send_handler
	//above two items indicate your network's speed or load
	stat_duration pack_time_sum; //udp::socket_base will not gather this item

	//recv corresponding statistic
	uint_fast64_t recv_msg_sum; //msgs returned by i_unpacker::parse_msg
	uint_fast64_t recv_byte_sum; //msgs (in bytes) returned by i_unpacker::parse_msg
	stat_duration dispatch_dealy_sum; //from parse_msg(exclude msg unpacking) to on_msg_handle
	stat_duration recv_idle_sum; //during this duration, socket suspended msg reception (receiving buffer overflow or doing congestion control)
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	stat_duration handle_time_1_sum; //on_msg consumed time, this indicate the efficiency of msg handling
#endif
	stat_duration handle_time_2_sum; //on_msg_handle consumed time, this indicate the efficiency of msg handling
	stat_duration unpack_time_sum; //udp::socket_base will not gather this item

	time_t last_send_time; //include heartbeat
	time_t last_recv_time; //include heartbeat

	time_t establish_time; //time of link establishment
	time_t break_time; //time of link broken
};

class auto_duration
{
public:
	auto_duration(statistic::stat_duration& duration_) : started(true), begin_time(statistic::now()), duration(duration_) {}
	~auto_duration() {end();}

	void end() {if (started) duration += statistic::now() - begin_time; started = false;}

private:
	bool started;
	statistic::stat_time begin_time;
	statistic::stat_duration& duration;
};

template<typename T>
struct obj_with_begin_time : public T
{
	obj_with_begin_time() {}
	obj_with_begin_time(T&& obj) : T(std::move(obj)) {restart();}
	obj_with_begin_time& operator=(T&& obj) {T::operator=(std::move(obj)); restart(); return *this;}
	//following two functions are used by concurrent queue only, ascs just use swap
	obj_with_begin_time(obj_with_begin_time&& other) : T(std::move(other)), begin_time(std::move(other.begin_time)) {}
	obj_with_begin_time& operator=(obj_with_begin_time&& other) {T::operator=(std::move(other)); begin_time = std::move(other.begin_time); return *this;}

	void restart() {restart(statistic::now());}
	void restart(const typename statistic::stat_time& begin_time_) {begin_time = begin_time_;}
	void swap(T& obj) {T::swap(obj); restart();}
	void swap(obj_with_begin_time& other) {T::swap(other); std::swap(begin_time, other.begin_time);}

	typename statistic::stat_time begin_time;
};

//free functions, used to do something to any container(except map and multimap) optionally with any mutex
template<typename _Can, typename _Mutex, typename _Predicate>
void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred) {std::lock_guard<std::mutex> lock(__mutex); for (auto& item : __can) __pred(item);}

template<typename _Can, typename _Predicate>
void do_something_to_all(_Can& __can, const _Predicate& __pred) {for (auto& item : __can) __pred(item);}

template<typename _Can, typename _Mutex, typename _Predicate>
void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
{
	std::lock_guard<std::mutex> lock(__mutex);
	for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;
}

template<typename _Can, typename _Predicate>
void do_something_to_one(_Can& __can, const _Predicate& __pred) {for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;}

template<typename _Can>
bool splice_helper(_Can& dest_can, _Can& src_can, size_t max_size = ASCS_MAX_MSG_NUM)
{
	if (src_can.empty())
		return false;

	auto size = dest_can.size();
	if (size >= max_size) //dest_can cannot hold more items.
		return false;

	size = max_size - size; //maximum items can be handled this time
	auto left_size = src_can.size();
	if (left_size > size) //some items left behind
	{
		left_size -= size;
		auto begin_iter = std::begin(src_can);
		auto end_iter = left_size > size ? std::next(begin_iter, size) : std::prev(std::end(src_can), left_size); //minimize iterator movement
		dest_can.splice(std::end(dest_can), src_can, begin_iter, end_iter);
	}
	else
		dest_can.splice(std::end(dest_can), src_can);

	return true;
}

//member functions, used to do something to any member container(except map and multimap) optionally with any member mutex
#define DO_SOMETHING_TO_ALL_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ALL_MUTEX_NAME(do_something_to_all, CAN, MUTEX)
#define DO_SOMETHING_TO_ALL(CAN) DO_SOMETHING_TO_ALL_NAME(do_something_to_all, CAN)

#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {std::lock_guard<std::mutex> lock(MUTEX); for (auto& item : CAN) __pred(item);}

#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto& item : CAN) __pred(item);} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto& item : CAN) __pred(item);}

#define DO_SOMETHING_TO_ONE_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ONE_MUTEX_NAME(do_something_to_one, CAN, MUTEX)
#define DO_SOMETHING_TO_ONE(CAN) DO_SOMETHING_TO_ONE_NAME(do_something_to_one, CAN)

#define DO_SOMETHING_TO_ONE_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{std::lock_guard<std::mutex> lock(MUTEX); for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

//used by both TCP and UDP
#define SAFE_SEND_MSG_CHECK \
{ \
	if (!this->is_ready()) return false; \
	std::this_thread::sleep_for(std::chrono::milliseconds(50)); \
}

#define GET_PENDING_MSG_NUM(FUNNAME, CAN) size_t FUNNAME() const {return CAN.size();}
#define POP_FIRST_PENDING_MSG(FUNNAME, CAN, MSGTYPE) void FUNNAME(MSGTYPE& msg) {msg.clear(); CAN.try_dequeue(msg);}
#define POP_ALL_PENDING_MSG(FUNNAME, CAN, CANTYPE) void FUNNAME(CANTYPE& msg_queue) {msg_queue.clear(); CAN.swap(msg_queue);}

///////////////////////////////////////////////////
//TCP msg sending interface
#define TCP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(&pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(str.data(), str.size(), can_overflow);}

#define TCP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (!can_overflow && !this->is_send_buffer_available()) \
		return false; \
	auto_duration dur(this->stat.pack_time_sum); \
	auto msg = this->packer_->pack_msg(pstr, len, num, NATIVE); \
	dur.end(); \
	return this->do_direct_send_msg(std::move(msg)); \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_SYNC_SEND_MSG(FUNNAME, NATIVE) \
size_t FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (this->lock_sending_flag()) \
	{ \
		auto_duration dur(this->stat.pack_time_sum); \
		auto msg = this->packer_->pack_msg(pstr, len, num, NATIVE); \
		dur.end(); \
		if (msg.empty()) \
			unified_out::error_out("found an empty message, please check your packer."); \
		return do_sync_send_msg(msg); /*always send message even it's empty, this is very important*/ \
	} \
	return 0; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, size_t)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into tcp::socket_base's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {while (!SEND_FUNNAME(pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{this->do_something_to_all([=](typename Pool::object_ctype& item) {item->SEND_FUNNAME(pstr, len, num, can_overflow);});} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//TCP msg sending interface
///////////////////////////////////////////////////

///////////////////////////////////////////////////
//UDP msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(peer_addr, pstr, len, can_overflow);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(peer_addr, str, can_overflow);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false) {return FUNNAME(peer_addr, str.data(), str.size(), can_overflow);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {return FUNNAME(peer_addr, pstr, len, num, can_overflow);} \
bool FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (!can_overflow && !this->is_send_buffer_available()) \
		return false; \
	in_msg_type msg(peer_addr, this->packer_->pack_msg(pstr, len, num, NATIVE)); \
	return this->do_direct_send_msg(std::move(msg)); \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define UDP_SYNC_SEND_MSG(FUNNAME, NATIVE) \
size_t FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {return FUNNAME(peer_addr, pstr, len, num, can_overflow);} \
size_t FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (this->lock_sending_flag()) \
	{ \
		auto msg = this->packer_->pack_msg(pstr, len, num, NATIVE); \
		if (msg.empty()) \
			unified_out::error_out("found an empty message, please check your packer."); \
		return do_sync_send_msg(peer_addr, msg); /*always send message even it's empty, this is very important*/ \
	} \
	return 0; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, size_t)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into udp::socket_base's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false)  {return FUNNAME(peer_addr, pstr, len, num, can_overflow);} \
bool FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{while (!SEND_FUNNAME(peer_addr, pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//UDP msg sending interface
///////////////////////////////////////////////////

class log_formater
{
public:
	static void all_out(const char* head, char* buff, size_t buff_len, const char* fmt, va_list& ap)
	{
		assert(nullptr != buff && buff_len > 0);

		std::stringstream os;
		os.rdbuf()->pubsetbuf(buff, buff_len);

		if (nullptr != head)
			os << '[' << head << "] ";

		char time_buff[64];
		auto now = time(nullptr);
#ifdef _MSC_VER
		ctime_s(time_buff, sizeof(time_buff), &now);
#else
		ctime_r(&now, time_buff);
#endif
		auto len = strlen(time_buff);
		assert(len > 0);
		if ('\n' == *std::next(time_buff, --len))
			*std::next(time_buff, len) = '\0';

		os << time_buff << " -> ";

#if defined _MSC_VER || (defined __unix__ && !defined __linux__)
		os.rdbuf()->sgetn(buff, buff_len);
#endif
		len = (size_t) os.tellp();
		if (len >= buff_len)
			*std::next(buff, buff_len - 1) = '\0';
		else
#if defined(_MSC_VER) && _MSC_VER >= 1400
			vsnprintf_s(std::next(buff, len), buff_len - len, _TRUNCATE, fmt, ap);
#else
			vsnprintf(std::next(buff, len), buff_len - len, fmt, ap);
#endif
	}
};

#define all_out_helper(head, buff, buff_len) va_list ap; va_start(ap, fmt); log_formater::all_out(head, buff, buff_len, fmt, ap); va_end(ap)
#define all_out_helper2(head) char output_buff[ASCS_UNIFIED_OUT_BUF_NUM]; all_out_helper(head, output_buff, sizeof(output_buff)); puts(output_buff)

#ifndef ASCS_CUSTOM_LOG
class unified_out
{
public:
#ifdef ASCS_NO_UNIFIED_OUT
	static void fatal_out(const char* fmt, ...) {}
	static void error_out(const char* fmt, ...) {}
	static void warning_out(const char* fmt, ...) {}
	static void info_out(const char* fmt, ...) {}
	static void debug_out(const char* fmt, ...) {}
#else
	static void fatal_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void error_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void warning_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void info_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void debug_out(const char* fmt, ...) {all_out_helper2(nullptr);}
#endif
};
#endif

} //namespace

#endif /* _ASCS_BASE_H_ */
