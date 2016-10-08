/*
 * base.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this is a global head file
 */

#ifndef _ASCS_BASE_H_
#define _ASCS_BASE_H_

#include <stdio.h>
#include <stdarg.h>

#include <list>
#include <memory>
#include <string>
#include <thread>
#include <sstream>
#include <shared_mutex>

#include <asio.hpp>
#include <asio/detail/noncopyable.hpp>

#include "config.h"

namespace ascs
{

class service_pump;
class timer;
class i_server
{
public:
	virtual service_pump& get_service_pump() = 0;
	virtual const service_pump& get_service_pump() const = 0;
	virtual bool del_client(const std::shared_ptr<timer>& client_ptr) = 0;
};

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
template<typename T>
class auto_buffer : public asio::detail::noncopyable
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
//replaceable_packer and replaceable_unpacker used replaceable_buffer as their msg type, so they're replaceable,
//shared_buffer<i_buffer> is available too.

//convert '->' operation to '.' operation
//user need to allocate object, and shared_buffer will free it
template<typename T>
class shared_buffer
{
public:
	typedef std::shared_ptr<T> buffer_type;
	typedef const buffer_type buffer_ctype;

	shared_buffer() {}
	shared_buffer(buffer_type _buffer) : buffer(_buffer) {}
	shared_buffer(const shared_buffer& other) : buffer(other.buffer) {}
	shared_buffer(shared_buffer&& other) : buffer(std::move(other.buffer)) {}
	const shared_buffer& operator=(const shared_buffer& other) {buffer = other.buffer; return *this;}
	~shared_buffer() {clear();}

	shared_buffer& operator=(shared_buffer&& other) {clear(); swap(other); return *this;}

	buffer_type raw_buffer() const {return buffer;}
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
	virtual void reset_state() {}
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) = 0;
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

#if !defined(__clang__) && defined(__GNUC__) &&  __GNUC__ < 5
//a substitute of std::list (gcc before 5.1), it's size() function has O(1) complexity
//BTW, the naming rule is not mine, I copied them from std::list in Visual C++ 14.0
template<typename _Ty, typename _Alloc = std::allocator<_Ty>>
class list
{
public:
	typedef list<_Ty, _Alloc> _Myt;
	typedef std::list<_Ty, _Alloc> _Mybase;

	typedef typename _Mybase::size_type size_type;

	typedef typename _Mybase::reference reference;
	typedef typename _Mybase::const_reference const_reference;

	typedef typename _Mybase::iterator iterator;
	typedef typename _Mybase::const_iterator const_iterator;
	typedef typename _Mybase::reverse_iterator reverse_iterator;
	typedef typename _Mybase::const_reverse_iterator const_reverse_iterator;

	list() : s(0) {}
	void swap(list& other) {impl.swap(other.impl); std::swap(s, other.s);}

	bool empty() const {return 0 == s;}
	size_type size() const {return s;}
	void resize(size_type _Newsize)
	{
		while (s < _Newsize)
		{
			++s;
			impl.emplace_back();
		}

		if (s > _Newsize)
		{
			auto end_iter = std::end(impl);
			auto begin_iter = _Newsize <= s / 2 ? std::next(std::begin(impl), _Newsize) : std::prev(end_iter, s - _Newsize); //minimize iterator movement

			s = _Newsize;
			impl.erase(begin_iter, end_iter);
		}
	}
	void clear() {s = 0; impl.clear();}
	iterator erase(const_iterator _Where) {--s; return impl.erase(_Where);}

	void push_front(const _Ty& _Val) {++s; impl.push_front(_Val);}
	void push_front(_Ty&& _Val) {++s; impl.push_front(std::move(_Val));}
	void pop_front() {--s; impl.pop_front();}
	reference front() {return impl.front();}
	iterator begin() {return impl.begin();}
	reverse_iterator rbegin() {return impl.rbegin();}
	const_reference front() const {return impl.front();}
	const_iterator begin() const {return impl.begin();}
	const_reverse_iterator rbegin() const {return impl.rbegin();}

	void push_back(const _Ty& _Val) {++s; impl.push_back(_Val);}
	void push_back(_Ty&& _Val) {++s; impl.push_back(std::move(_Val));}
	void pop_back() {--s; impl.pop_back();}
	reference back() {return impl.back();}
	iterator end() {return impl.end();}
	reverse_iterator rend() {return impl.rend();}
	const_reference back() const {return impl.back();}
	const_iterator end() const {return impl.end();}
	const_reverse_iterator rend() const {return impl.rend();}

	void splice(const_iterator _Where, _Myt& _Right) {s += _Right.size(); _Right.s = 0; impl.splice(_Where, _Right.impl);}
	void splice(const_iterator _Where, _Myt& _Right, const_iterator _First) {++s; --_Right.s; impl.splice(_Where, _Right.impl, _First);}
	void splice(const_iterator _Where, _Myt& _Right, const_iterator _First, const_iterator _Last)
	{
		auto size = std::distance(_First, _Last);
		//this std::distance invocation is the penalty for making complexity of size() constant.
		s += size;
		_Right.s -= size;

		impl.splice(_Where, _Right.impl, _First, _Last);
	}

private:
	size_type s;
	_Mybase impl;
};
#else
template<typename T, typename _Alloc = std::allocator<T>> using list = std::list<T, _Alloc>;
#endif

#ifdef ASCS_USE_CUSTOM_QUEUE
#elif defined(ASCS_USE_CONCURRENT_QUEUE)
template<typename T>
class message_queue_ : public moodycamel::ConcurrentQueue<T>
{
public:
	typedef moodycamel::ConcurrentQueue<T> super;
	typedef message_queue_<T> me;
	typedef std::lock_guard<me> lock_guard;

	message_queue_() {}
	message_queue_(size_t size) : super(size) {}

	size_t size() const {return super::size_approx();}
	bool empty() const {return 0 == size();}

	//not thread-safe
	void clear() {super(std::move(*this));}
	void swap(me& other) {super::swap(other);}

	//lockable, dummy
	void lock() const {}
	void unlock() const {}

	bool idle() const {return true;}

	bool enqueue_(const T& item) {return this->enqueue(item);}
	bool enqueue_(T&& item) {return this->enqueue(std::move(item));}
	bool try_enqueue_(const T& item) {return this->try_enqueue(item);}
	bool try_enqueue_(T&& item) {return this->try_enqueue(std::move(item));}
	bool try_dequeue_(T& item) {return this->try_dequeue(item);}
};
#else
template<typename T>
class message_queue_ : public list<T>
{
public:
	typedef list<T> super;
	typedef message_queue_<T> me;
	typedef std::lock_guard<me> lock_guard;

	message_queue_() {}
	message_queue_(size_t) {}

	//not thread-safe
	void clear() {super::clear();}
	void swap(me& other) {super::swap(other);}

	//lockable
	void lock() {mutex.lock();}
	void unlock() {mutex.unlock();}

	bool idle() {std::unique_lock<std::shared_mutex> lock(mutex, std::try_to_lock); return lock.owns_lock();}

	bool enqueue(const T& item) {lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(T&& item) {lock_guard lock(*this); return enqueue_(std::move(item));}
	bool try_enqueue(const T& item) {lock_guard lock(*this); return try_enqueue_(item);}
	bool try_enqueue(T&& item) {lock_guard lock(*this); return try_enqueue_(std::move(item));}
	bool try_dequeue(T& item) {lock_guard lock(*this); return try_dequeue_(item);}

	bool enqueue_(const T& item) {this->push_back(item); return true;}
	bool enqueue_(T&& item) {this->push_back(std::move(item)); return true;}
	bool try_enqueue_(const T& item) {return enqueue_(item);}
	bool try_enqueue_(T&& item) {return enqueue_(std::move(item));}
	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); return true;}

private:
	std::shared_mutex mutex;
};
#endif

#ifndef ASCS_USE_CUSTOM_QUEUE
template<typename T>
class message_queue : public message_queue_<T>
{
public:
	typedef message_queue_<T> super;

	message_queue() {}
	message_queue(size_t size) : super(size) {}

	//it's not thread safe for 'other', please note.
	size_t move_items_in(typename super::me& other, size_t max_size = ASCS_MAX_MSG_NUM)
	{
		typename super::lock_guard lock(*this);
		auto cur_size = this->size();
		if (cur_size >= max_size)
			return 0;

		size_t num = 0;
		while (cur_size < max_size)
		{
			T item;
			if (!other.try_dequeue_(item)) //not thread safe for 'other', because we called 'try_dequeue_'
				break;

			this->enqueue_(std::move(item));
			++cur_size;
			++num;
		}

		return num;
	}

	//it's no thread safe for 'other', please note.
	size_t move_items_in(list<T>& other, size_t max_size = ASCS_MAX_MSG_NUM)
	{
		typename super::lock_guard lock(*this);
		auto cur_size = this->size();
		if (cur_size >= max_size)
			return 0;

		size_t num = 0;
		while (cur_size < max_size && !other.empty())
		{
			this->enqueue_(std::move(other.front()));
			other.pop_front();
			++cur_size;
			++num;
		}

		return num;
	}
};
#endif

//unpacker concept
namespace tcp
{
	template<typename MsgType>
	class i_unpacker
	{
	public:
		typedef MsgType msg_type;
		typedef const msg_type msg_ctype;
		typedef list<msg_type> container_type;

	protected:
		virtual ~i_unpacker() {}

	public:
		virtual void reset_state() = 0;
		virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can) = 0;
		virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) = 0;
		virtual asio::mutable_buffers_1 prepare_next_recv() = 0;
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
		udp_msg(const asio::ip::udp::endpoint& _peer_addr, MsgType&& msg) : MsgType(std::move(msg)), peer_addr(_peer_addr) {}

		void swap(udp_msg& other) {std::swap(peer_addr, other.peer_addr); MsgType::swap(other);}
		void swap(asio::ip::udp::endpoint& addr, MsgType&& tmp_msg) {std::swap(peer_addr, addr); MsgType::swap(tmp_msg);}
	};

	template<typename MsgType>
	class i_unpacker
	{
	public:
		typedef MsgType msg_type;
		typedef const msg_type msg_ctype;
		typedef list<udp_msg<msg_type>> container_type;

	protected:
		virtual ~i_unpacker() {}

	public:
		virtual void reset_state() {}
		virtual msg_type parse_msg(size_t bytes_transferred) = 0;
		virtual asio::mutable_buffers_1 prepare_next_recv() = 0;
	};
} //namespace
//unpacker concept

//free functions, used to do something to any container(except map and multimap) optionally with any mutex
template<typename _Can, typename _Mutex, typename _Predicate>
void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred) {std::shared_lock<std::shared_mutex> lock(__mutex); for (auto& item : __can) __pred(item);}

template<typename _Can, typename _Predicate>
void do_something_to_all(_Can& __can, const _Predicate& __pred) {for (auto& item : __can) __pred(item);}

template<typename _Can, typename _Mutex, typename _Predicate>
void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
{
	std::shared_lock<std::shared_mutex> lock(__mutex);
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
	if (size >= max_size) //dest_can can hold more items.
		return false;

	size = max_size - size; //maximum items this time can handle
	if (src_can.size() > size) //some items left behind
	{
		auto begin_iter = std::begin(src_can);
		auto left_size = src_can.size() - size;
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
template<typename _Predicate> void NAME(const _Predicate& __pred) {std::shared_lock<std::shared_mutex> lock(MUTEX); for (auto& item : CAN) __pred(item);}

#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto& item : CAN) __pred(item);} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto& item : CAN) __pred(item);}

#define DO_SOMETHING_TO_ONE_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ONE_MUTEX_NAME(do_something_to_one, CAN, MUTEX)
#define DO_SOMETHING_TO_ONE(CAN) DO_SOMETHING_TO_ONE_NAME(do_something_to_one, CAN)

#define DO_SOMETHING_TO_ONE_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{std::shared_lock<std::shared_mutex> lock(MUTEX); for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

//used by both TCP and UDP
#define SAFE_SEND_MSG_CHECK \
{ \
	if (!this->is_send_allowed() || this->stopped()) return false; \
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
	{return can_overflow || this->is_send_buffer_available() ? this->do_direct_send_msg(this->packer_->pack_msg(pstr, len, num, NATIVE)) : false;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into tcp::socket's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {while (!SEND_FUNNAME(pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{this->do_something_to_all([=](const auto& item) {item->SEND_FUNNAME(pstr, len, num, can_overflow);});} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//TCP msg sending interface
///////////////////////////////////////////////////

///////////////////////////////////////////////////
//UDP msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const asio::ip::udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false) {return FUNNAME(peer_addr, str.data(), str.size(), can_overflow);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (can_overflow || this->is_send_buffer_available()) \
	{ \
		in_msg_type msg(peer_addr, this->packer_->pack_msg(pstr, len, num, NATIVE)); \
		return this->do_direct_send_msg(std::move(msg)); \
	} \
	return false; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into udp::socket's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
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
