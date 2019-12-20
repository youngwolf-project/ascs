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
#include <mutex>
#include <vector>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#ifdef ASCS_SYNC_SEND
#include <future>
#elif defined(ASCS_SYNC_RECV)
#include <condition_variable>
#endif

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

class tracked_executor;
class service_pump;
class i_matrix
{
public:
	virtual bool started() const = 0;
	virtual service_pump& get_service_pump() = 0;
	virtual const service_pump& get_service_pump() const = 0;

	virtual bool socket_exist(uint_fast64_t id) = 0;
	virtual std::shared_ptr<tracked_executor> find_socket(uint_fast64_t id) = 0;
};

namespace tcp
{
	class i_server : public i_matrix
	{
	public:
		virtual bool del_socket(const std::shared_ptr<tracked_executor>& socket_ptr) = 0;
		virtual bool restore_socket(const std::shared_ptr<tracked_executor>& socket_ptr, uint_fast64_t id) = 0;
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

//convert '->' operation to '.' operation
//user need to allocate object, and shared_buffer will free it
//not like auto_buffer, shared_buffer is copyable (seemingly), but auto_buffer is a bit more efficient.
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

//ascs requires that container must take one and only one template argument
template<typename T> using list = std::list<T>;

//packer concept
template<typename MsgType>
class i_packer
{
public:
	typedef MsgType msg_type;
	typedef const msg_type msg_ctype;
	typedef list<msg_type> container_type;

protected:
	virtual ~i_packer() {}

public:
	virtual void reset() {}
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) = 0;
	//no native parameter anymore, which means it's always false, if true, you should call direct_(sync_)send_msg instead
	virtual bool pack_msg(msg_type&& msg, container_type& msg_can) {return false;}
	virtual bool pack_msg(msg_type&& msg1, msg_type&& msg2, container_type& msg_can) {return false;}
	virtual bool pack_msg(container_type&& in, container_type& out) {return false;}
	virtual msg_type pack_heartbeat() {return msg_type();}

	//this default implementation is meaningless, just satisfy compilers
	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(msg.data());}
	virtual const char* raw_data(msg_ctype& msg) const {return msg.data();}
	virtual size_t raw_data_len(msg_ctype& msg) const {return msg.size();}

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
template<typename MsgType>
class i_unpacker
{
public:
	typedef MsgType msg_type;
	typedef const msg_type msg_ctype;
	typedef list<msg_type> container_type;
	typedef ASCS_RECV_BUFFER_TYPE buffer_type;

	bool stripped() const {return _stripped;}
	void stripped(bool stripped_) {_stripped = stripped_;}

protected:
	i_unpacker() : _stripped(true) {}
	virtual ~i_unpacker() {}

public:
	virtual void reset() = 0;
	//heartbeat must not be included in msg_can, otherwise you must handle heartbeat at where you handle normal messages.
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can) = 0;
	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) {return 0;}
	virtual buffer_type prepare_next_recv() = 0;

	//this default implementation is meaningless, just satisfy compilers
	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(msg.data());}
	virtual const char* raw_data(msg_ctype& msg) const {return msg.data();}
	virtual size_t raw_data_len(msg_ctype& msg) const {return msg.size();}

private:
	bool _stripped;
};

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
} //namespace
//unpacker concept

struct statistic
{
#ifdef ASCS_FULL_STATISTIC
	struct duration : public std::chrono::system_clock::duration
	{
		typedef std::chrono::system_clock::duration super;

		duration() {reset();}
		super get_avg() const {return 0 == num ? super(*this) : *this / num;}
		void reset() {super::operator=(min_duration = max_duration = super(0)); num = 0;}

		duration& operator+=(const super& other)
		{
			super::operator+=(other);
			++num;

			if (max_duration < other)
				max_duration = other;
			if (0 == min_duration.count() || min_duration > other)
				min_duration = other;

			return *this;
		}
		duration& operator+=(const duration& other)
		{
			super::operator+=(other);
			num += other.num;

			if (max_duration < other.max_duration)
				max_duration = other.max_duration;
			if (0 == min_duration.count() || min_duration > other.min_duration)
				min_duration = other.min_duration;

			return *this;
		}
		duration& operator-=(const duration& other) {super::operator-=(other); num -= other.num; return *this;}

		float to_float() const {return to_float(*this);}
		static float to_float(const super& dur) {return std::chrono::duration_cast<std::chrono::duration<float>>(dur).count();}

		template<typename stream_type> friend stream_type& operator<<(stream_type& stream, const duration& dur)
		{
			return stream << dur.to_float() << "[" << duration::to_float(dur.min_duration) << ", "
				<< duration::to_float(dur.get_avg()) << " * " << dur.num << ", " << duration::to_float(dur.max_duration) << "]";
		}

		super min_duration, max_duration;
		int_fast64_t num;
	};

	typedef std::chrono::system_clock::time_point stat_time;
	static stat_time now() {return std::chrono::system_clock::now();}
	typedef duration stat_duration;
#else
	struct dummy_duration //not a real duration
	{
		dummy_duration& operator+=(const dummy_duration& other) {return *this;} //just satisfy compiler(d1 += d2)
		dummy_duration& operator-=(const dummy_duration& other) {return *this;} //just satisfy compiler(d1 -= d2)
	};
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
		send_delay_sum.reset();
		send_time_sum.reset();
		pack_time_sum.reset();

		dispatch_delay_sum.reset();
		recv_idle_sum.reset();
		handle_time_sum.reset();
		unpack_time_sum.reset();
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
		dispatch_delay_sum += other.dispatch_delay_sum;
		recv_idle_sum += other.recv_idle_sum;
		handle_time_sum += other.handle_time_sum;
		unpack_time_sum += other.unpack_time_sum;

		return *this;
	}

	statistic operator+(const struct statistic& other) const
	{
		auto re = *this;
		re += other;

		return re;
	}

	statistic& operator-=(const struct statistic& other)
	{
		send_msg_sum -= other.send_msg_sum;
		send_byte_sum -= other.send_byte_sum;
		send_delay_sum -= other.send_delay_sum;
		send_time_sum -= other.send_time_sum;
		pack_time_sum -= other.pack_time_sum;

		recv_msg_sum -= other.recv_msg_sum;
		recv_byte_sum -= other.recv_byte_sum;
		dispatch_delay_sum -= other.dispatch_delay_sum;
		recv_idle_sum -= other.recv_idle_sum;
		handle_time_sum -= other.handle_time_sum;
		unpack_time_sum -= other.unpack_time_sum;

		return *this;
	}

	statistic operator-(const struct statistic& other) const
	{
		auto re = *this;
		re -= other;

		return re;
	}

	std::string to_string() const
	{
		std::ostringstream s;
		s << "send corresponding statistic:\nmessage sum: " << send_msg_sum << std::endl << "size in bytes: " << send_byte_sum << std::endl
#ifdef ASCS_FULL_STATISTIC
			<< "send delay: " << send_delay_sum << std::endl << "send duration: " << send_time_sum << std::endl << "pack duration: " << pack_time_sum << std::endl
#endif
			<< "\nrecv corresponding statistic:\nmessage sum: " << recv_msg_sum << std::endl << "size in bytes: " << recv_byte_sum
#ifdef ASCS_FULL_STATISTIC
			<< "\ndispatch delay: " << dispatch_delay_sum << std::endl << "recv idle duration: " << recv_idle_sum << std::endl
			<< "msg handling duration: " << handle_time_sum << std::endl << "unpack duration: " << unpack_time_sum
#endif
		;return s.str();
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
	stat_duration dispatch_delay_sum; //from parse_msg(exclude msg unpacking) to on_msg_handle
	stat_duration recv_idle_sum; //during this duration, socket suspended msg reception (receiving buffer overflow)
	stat_duration handle_time_sum; //on_msg_handle (and on_msg) consumed time, this indicate the efficiency of msg handling
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

enum sync_call_result {SUCCESS, NOT_APPLICABLE, DUPLICATE, TIMEOUT};

template<typename T> struct obj_with_begin_time : public T
{
	obj_with_begin_time() {}
	obj_with_begin_time(const T& obj) : T(obj) {restart();}
	obj_with_begin_time(T&& obj) : T(std::move(obj)) {restart();}
	obj_with_begin_time& operator=(const T& obj) {T::operator=(obj); restart(); return *this;}
	obj_with_begin_time& operator=(T&& obj) {T::operator=(std::move(obj)); restart(); return *this;}
	obj_with_begin_time(const obj_with_begin_time& other) : T(other), begin_time(other.begin_time) {}
	obj_with_begin_time(obj_with_begin_time&& other) : T(std::move(other)), begin_time(std::move(other.begin_time)) {}
	obj_with_begin_time& operator=(const obj_with_begin_time& other) {T::operator=(other); begin_time = other.begin_time; return *this;}
	obj_with_begin_time& operator=(obj_with_begin_time&& other) {T::operator=(std::move(other)); begin_time = std::move(other.begin_time); return *this;}

	void restart() {restart(statistic::now());}
	void restart(const typename statistic::stat_time& begin_time_) {begin_time = begin_time_;}
	void swap(T& obj) {T::swap(obj); restart();}
	void swap(obj_with_begin_time& other) {T::swap(other); std::swap(begin_time, other.begin_time);}

	void clear() {T::clear(); begin_time = typename statistic::stat_time();}

	typename statistic::stat_time begin_time;
};

#ifdef ASCS_SYNC_SEND
template<typename T> struct obj_with_begin_time_promise : public obj_with_begin_time<T>
{
	typedef obj_with_begin_time<T> super;

	obj_with_begin_time_promise(bool need_promise = false) {check_and_create_promise(need_promise);}
	obj_with_begin_time_promise(const T& obj, bool need_promise = false) : super(obj) {check_and_create_promise(need_promise);}
	obj_with_begin_time_promise(T&& obj, bool need_promise = false) : super(std::move(obj)) {check_and_create_promise(need_promise);}
	obj_with_begin_time_promise(const obj_with_begin_time_promise& other) : super(other), p(other.p) {}
	obj_with_begin_time_promise(obj_with_begin_time_promise&& other) : super(std::move(other)), p(std::move(other.p)) {}
	obj_with_begin_time_promise& operator=(const obj_with_begin_time_promise& other) {super::operator=(other); p = other.p; return *this;}
	obj_with_begin_time_promise& operator=(obj_with_begin_time_promise&& other) {super::operator=(std::move(other)); p = std::move(other.p); return *this;}

	void swap(T& obj, bool need_promise = false) {super::swap(obj); check_and_create_promise(need_promise);}
	void swap(obj_with_begin_time_promise& other) {super::swap(other); p.swap(other.p);}

	void clear() {super::clear(); p.reset();}
	void check_and_create_promise(bool need_promise) {if (!need_promise) p.reset(); else if (!p) p = std::make_shared<std::promise<sync_call_result>>();}

	std::shared_ptr<std::promise<sync_call_result>> p;
};
#endif

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
size_t get_size_in_byte(const _Can& __can)
{
	size_t size_in_byte = 0;
	do_something_to_all(__can, [&size_in_byte](typename _Can::const_reference item) {size_in_byte += item.size();});
	return size_in_byte;
}

template<typename _Can>
size_t get_size_in_byte(const _Can& __can, size_t& size)
{
	auto size_in_byte = size = 0;
	do_something_to_all(__can, [&size_in_byte, &size](typename _Can::const_reference item) {size_in_byte += item.size(); ++size;});
	return size_in_byte;
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
#define SAFE_SEND_MSG_CHECK(F_VALUE) \
{ \
	if (!is_ready()) return F_VALUE; \
	std::this_thread::sleep_for(std::chrono::milliseconds(50)); \
}

#define GET_PENDING_MSG_SIZE(FUNNAME, CAN) size_t FUNNAME() const {return CAN.size_in_byte();}
#define POP_FIRST_PENDING_MSG(FUNNAME, CAN, MSGTYPE) void FUNNAME(MSGTYPE& msg) {msg.clear(); CAN.try_dequeue(msg);}
#define POP_FIRST_PENDING_MSG_NOTIFY(FUNNAME, CAN, MSGTYPE) void FUNNAME(MSGTYPE& msg) \
	{msg.clear(); if (CAN.try_dequeue(msg) && msg.p) msg.p->set_value(sync_call_result::NOT_APPLICABLE);}
#define POP_ALL_PENDING_MSG(FUNNAME, CAN, CANTYPE) void FUNNAME(CANTYPE& can) {can.clear(); CAN.swap(can);}
#define POP_ALL_PENDING_MSG_NOTIFY(FUNNAME, CAN, CANTYPE) void FUNNAME(CANTYPE& can) \
	{can.clear(); CAN.swap(can); ascs::do_something_to_all(can, [](typename CANTYPE::reference msg) {if (msg.p) msg.p->set_value(sync_call_result::NOT_APPLICABLE);});}

///////////////////////////////////////////////////
//TCP msg sending interface
#define TCP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false, bool prior = false) {return FUNNAME(&pstr, &len, 1, can_overflow, prior);} \
TYPE FUNNAME(char* pstr, size_t len, bool can_overflow = false, bool prior = false) {return FUNNAME(&pstr, &len, 1, can_overflow, prior);} \
template<typename Buffer> \
TYPE FUNNAME(const Buffer& buffer, bool can_overflow = false, bool prior = false) {return FUNNAME(buffer.data(), buffer.size(), can_overflow, prior);}

#define TCP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(in_msg_type&& msg, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return false; \
	else if (NATIVE) \
		return do_direct_send_msg(std::move(msg), prior); \
	typename Packer::container_type msg_can; \
	auto_duration dur(stat.pack_time_sum); \
	auto re = packer_->pack_msg(std::move(msg), msg_can); \
	dur.end(); \
	return re && do_direct_send_msg(msg_can, prior); \
} \
bool FUNNAME(in_msg_type&& msg1, in_msg_type&& msg2, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return false; \
	typename Packer::container_type msg_can; \
	if (NATIVE) \
	{ \
		msg_can.emplace_back(std::move(msg1)); msg_can.emplace_back(std::move(msg2)); \
		return do_direct_send_msg(msg_can, prior); \
	} \
	auto_duration dur(stat.pack_time_sum); \
	auto re = packer_->pack_msg(std::move(msg1), std::move(msg2), msg_can); \
	dur.end(); \
	return re && do_direct_send_msg(msg_can, prior); \
} \
bool FUNNAME(typename Packer::container_type& msg_can, bool can_overflow = false, bool prior = false)  \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return false; \
	else if (NATIVE) \
		return do_direct_send_msg(msg_can, prior); \
	typename Packer::container_type out; \
	auto_duration dur(stat.pack_time_sum); \
	auto re = packer_->pack_msg(msg_can, out); \
	dur.end(); \
	return re && do_direct_send_msg(out, prior); \
} \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return false; \
	auto_duration dur(stat.pack_time_sum); \
	auto msg = packer_->pack_msg(pstr, len, num, NATIVE); \
	dur.end(); \
	return do_direct_send_msg(std::move(msg), prior); \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into tcp::socket_base's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(in_msg_type&& msg, bool can_overflow = false, bool prior = false) \
	{while (!SEND_FUNNAME(std::move(msg), can_overflow, prior)) SAFE_SEND_MSG_CHECK(false) return true;} \
bool FUNNAME(in_msg_type&& msg1, in_msg_type&& msg2, bool can_overflow = false, bool prior = false) \
	{while (!SEND_FUNNAME(std::move(msg1), std::move(msg2), can_overflow, prior)) SAFE_SEND_MSG_CHECK(false) return true;} \
bool FUNNAME(typename Packer::container_type& msg_can, bool can_overflow = false, bool prior = false) \
	{while (!SEND_FUNNAME(msg_can, can_overflow, prior)) SAFE_SEND_MSG_CHECK(false) return true;} \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
	{while (!SEND_FUNNAME(pstr, len, num, can_overflow, prior)) SAFE_SEND_MSG_CHECK(false) return true;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
	{this->do_something_to_all([=](typename Pool::object_ctype& item) {item->SEND_FUNNAME(pstr, len, num, can_overflow, prior);});} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//TCP msg sending interface
///////////////////////////////////////////////////

#ifdef ASCS_SYNC_SEND
///////////////////////////////////////////////////
//TCP sync msg sending interface
#define TCP_SYNC_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(&pstr, &len, 1, duration, can_overflow, prior);} \
TYPE FUNNAME(char* pstr, size_t len, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(&pstr, &len, 1, duration, can_overflow, prior);} \
template<typename Buffer> TYPE FUNNAME(const Buffer& buffer, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(buffer.data(), buffer.size(), duration, can_overflow, prior);}

#define TCP_SYNC_SEND_MSG(FUNNAME, NATIVE) \
sync_call_result FUNNAME(in_msg_type&& msg, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return sync_call_result::NOT_APPLICABLE; \
	else if (NATIVE) \
		return do_direct_sync_send_msg(std::move(msg), duration, prior); \
	typename Packer::container_type msg_can; \
	auto_duration dur(stat.pack_time_sum); \
	auto re = packer_->pack_msg(std::move(msg), msg_can); \
	dur.end(); \
	return re ? do_direct_sync_send_msg(msg_can, duration, prior) : sync_call_result::NOT_APPLICABLE; \
} \
sync_call_result FUNNAME(in_msg_type&& msg1, in_msg_type&& msg2, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return sync_call_result::NOT_APPLICABLE; \
	typename Packer::container_type msg_can; \
	if (NATIVE) \
	{ \
		msg_can.emplace_back(std::move(msg1)); msg_can.emplace_back(std::move(msg2)); \
		return do_direct_sync_send_msg(msg_can, duration, prior); \
	} \
	auto_duration dur(stat.pack_time_sum); \
	auto re = packer_->pack_msg(std::move(msg1), std::move(msg2), msg_can); \
	dur.end(); \
	return re ? do_direct_sync_send_msg(msg_can, duration, prior) : sync_call_result::NOT_APPLICABLE; \
} \
sync_call_result FUNNAME(typename Packer::container_type& msg_can, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return sync_call_result::NOT_APPLICABLE; \
	else if (NATIVE) \
		return do_direct_sync_send_msg(msg_can, duration, prior); \
	typename Packer::container_type out; \
	auto_duration dur(stat.pack_time_sum); \
	auto re = packer_->pack_msg(msg_can, out); \
	dur.end(); \
	return re ? do_direct_sync_send_msg(out, duration, prior) : sync_call_result::NOT_APPLICABLE; \
} \
sync_call_result FUNNAME(const char* const pstr[], const size_t len[], size_t num, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return sync_call_result::NOT_APPLICABLE; \
	auto_duration dur(stat.pack_time_sum); \
	auto msg = packer_->pack_msg(pstr, len, num, NATIVE); \
	dur.end(); \
	return do_direct_sync_send_msg(std::move(msg), duration, prior); \
} \
TCP_SYNC_SEND_MSG_CALL_SWITCH(FUNNAME, sync_call_result)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into tcp::socket_base's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define TCP_SYNC_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
sync_call_result FUNNAME(in_msg_type&& msg, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{while (sync_call_result::SUCCESS != SEND_FUNNAME(std::move(msg), duration, can_overflow, prior)) \
		SAFE_SEND_MSG_CHECK(sync_call_result::NOT_APPLICABLE) return sync_call_result::SUCCESS;} \
sync_call_result FUNNAME(in_msg_type&& msg1, in_msg_type&& msg2, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{while (sync_call_result::SUCCESS != SEND_FUNNAME(std::move(msg1), std::move(msg2), duration, can_overflow, prior)) \
		SAFE_SEND_MSG_CHECK(sync_call_result::NOT_APPLICABLE) return sync_call_result::SUCCESS;} \
sync_call_result FUNNAME(typename Packer::container_type& msg_can, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{while (sync_call_result::SUCCESS != SEND_FUNNAME(msg_can, duration, can_overflow, prior)) \
		SAFE_SEND_MSG_CHECK(sync_call_result::NOT_APPLICABLE) return sync_call_result::SUCCESS;} \
sync_call_result FUNNAME(const char* const pstr[], const size_t len[], size_t num, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{while (sync_call_result::SUCCESS != SEND_FUNNAME(pstr, len, num, duration, can_overflow, prior)) \
		SAFE_SEND_MSG_CHECK(sync_call_result::NOT_APPLICABLE) return sync_call_result::SUCCESS;} \
TCP_SYNC_SEND_MSG_CALL_SWITCH(FUNNAME, sync_call_result)
//TCP sync msg sending interface
///////////////////////////////////////////////////
#endif

///////////////////////////////////////////////////
//UDP msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false, bool prior = false) {return FUNNAME(peer_addr, pstr, len, can_overflow, prior);} \
TYPE FUNNAME(char* pstr, size_t len, bool can_overflow = false, bool prior = false) {return FUNNAME(peer_addr, pstr, len, can_overflow, prior);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false, bool prior = false) \
    {return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow, prior);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, char* pstr, size_t len, bool can_overflow = false, bool prior = false) \
    {return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow, prior);} \
template<typename Buffer> TYPE FUNNAME(const Buffer& buffer, bool can_overflow = false, bool prior = false) {return FUNNAME(peer_addr, buffer, can_overflow, prior);} \
template<typename Buffer> TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const Buffer& buffer, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, buffer.data(), buffer.size(), can_overflow, prior);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, pstr, len, num, can_overflow, prior);} \
bool FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return false; \
	in_msg_type msg(peer_addr, packer_->pack_msg(pstr, len, num, NATIVE)); \
	return do_direct_send_msg(std::move(msg), prior); \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into udp::socket_base's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, pstr, len, num, can_overflow, prior);} \
bool FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
	{while (!SEND_FUNNAME(peer_addr, pstr, len, num, can_overflow, prior)) SAFE_SEND_MSG_CHECK(false) return true;} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//UDP msg sending interface
///////////////////////////////////////////////////

#ifdef ASCS_SYNC_SEND
///////////////////////////////////////////////////
//UDP sync msg sending interface
#define UDP_SYNC_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, pstr, len, duration, can_overflow, prior);} \
TYPE FUNNAME(char* pstr, size_t len, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, pstr, len, duration, can_overflow, prior);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, &pstr, &len, 1, duration, can_overflow, prior);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, char* pstr, size_t len, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, &pstr, &len, 1, duration, can_overflow, prior);} \
template<typename Buffer> TYPE FUNNAME(const Buffer& buffer, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, buffer, duration, can_overflow, prior);} \
template<typename Buffer> \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const Buffer& buffer, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, buffer.data(), buffer.size(), duration, can_overflow, prior);}

#define UDP_SYNC_SEND_MSG(FUNNAME, NATIVE) \
sync_call_result FUNNAME(const char* const pstr[], const size_t len[], size_t num, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, pstr, len, num, duration, can_overflow, prior);} \
sync_call_result FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, \
	unsigned duration = 0, bool can_overflow = false, bool prior = false) \
{ \
	if (!can_overflow && !this->shrink_send_buffer()) \
		return sync_call_result::NOT_APPLICABLE; \
	in_msg_type msg(peer_addr, packer_->pack_msg(pstr, len, num, NATIVE)); \
	return do_direct_sync_send_msg(std::move(msg), duration, prior); \
} \
UDP_SYNC_SEND_MSG_CALL_SWITCH(FUNNAME, sync_call_result)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into udp::socket_base's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define UDP_SYNC_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
sync_call_result FUNNAME(const char* const pstr[], const size_t len[], size_t num, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{return FUNNAME(peer_addr, pstr, len, num, duration, can_overflow, prior);} \
sync_call_result FUNNAME(const asio::ip::udp::endpoint& peer_addr, \
	const char* const pstr[], const size_t len[], size_t num, unsigned duration = 0, bool can_overflow = false, bool prior = false) \
	{while (sync_call_result::SUCCESS != SEND_FUNNAME(peer_addr, pstr, len, num, duration, can_overflow, prior)) \
		SAFE_SEND_MSG_CHECK(sync_call_result::NOT_APPLICABLE) return sync_call_result::SUCCESS;} \
UDP_SYNC_SEND_MSG_CALL_SWITCH(FUNNAME, sync_call_result)
//UDP sync msg sending interface
///////////////////////////////////////////////////
#endif

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

		os << '[' << std::this_thread::get_id() << "] ";

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
#ifdef _MSC_VER
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
