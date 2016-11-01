/*
 * container.h
 *
 *  Created on: 2016-10-10
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * containers.
 */

#ifndef _ASCS_CONTAINER_H_
#define _ASCS_CONTAINER_H_

#include <list>
#include <shared_mutex>

#include "config.h"

namespace ascs
{

//ascs requires that container must take one and only one template argument.
#if defined(_MSC_VER) || defined(__clang__) || __GNUC__ >= 5
template<typename T> using list = std::list<T>;
#else
//a substitute of std::list (before gcc 5), it's size() function has O(1) complexity
//BTW, the naming rule is not mine, I copied them from std::list in Visual C++ 14.0
template<typename _Ty>
class list
{
public:
	typedef list<_Ty> _Myt;
	typedef std::list<_Ty> _Mybase;

	typedef typename _Mybase::size_type size_type;

	typedef typename _Mybase::reference reference;
	typedef typename _Mybase::const_reference const_reference;

	typedef typename _Mybase::iterator iterator;
	typedef typename _Mybase::const_iterator const_iterator;
	typedef typename _Mybase::reverse_iterator reverse_iterator;
	typedef typename _Mybase::const_reverse_iterator const_reverse_iterator;

	list() : s(0) {}
	list(list&& other) : s(0) {swap(other);}
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
#endif

class dummy_lockable
{
public:
	typedef std::lock_guard<dummy_lockable> lock_guard;

	//lockable, dummy
	void lock() const {}
	void unlock() const {}
	bool idle() const {return true;} //locked or not
};

class lockable
{
public:
	typedef std::lock_guard<lockable> lock_guard;

	//lockable
	void lock() {mutex.lock();}
	void unlock() {mutex.unlock();}
	bool idle() {std::unique_lock<std::shared_mutex> lock(mutex, std::try_to_lock); return lock.owns_lock();} //locked or not

private:
	std::shared_mutex mutex;
};

//Container must at least has the following functions:
// Container() and Container(size_t) constructor
// move constructor
// swap
// size_approx
// enqueue(const T& item)
// enqueue(T&& item)
// try_dequeue(T& item)
template<typename T, typename Container>
class lock_free_queue : public Container, public dummy_lockable
{
public:
	typedef T data_type;
	typedef Container super;
	typedef lock_free_queue<T, Container> me;

	lock_free_queue() {}
	lock_free_queue(size_t size) : super(size) {}

	size_t size() const {return this->size_approx();}
	bool empty() const {return 0 == size();}

	//not thread-safe
	void clear() {super(std::move(*this));}

	bool enqueue_(const T& item) {return this->enqueue(item);}
	bool enqueue_(T&& item) {return this->enqueue(std::move(item));}
	bool try_dequeue_(T& item) {return this->try_dequeue(item);}
};

//Container must at least has the following functions:
// Container() and Container(size_t) constructor
// size
// empty
// clear
// swap
// push_back(const T& item)
// push_back(T&& item)
// front
// pop_front
template<typename T, typename Container, typename Lockable>
class queue : public Container, public Lockable
{
public:
	typedef T data_type;
	typedef Container super;
	typedef queue<T, Container, Lockable> me;

	queue() {}
	queue(size_t size) : super(size) {}

	bool enqueue(const T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(T&& item) {typename Lockable::lock_guard lock(*this); return enqueue_(std::move(item));}
	bool try_dequeue(T& item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}

	bool enqueue_(const T& item) {this->push_back(item); return true;}
	bool enqueue_(T&& item) {this->push_back(std::move(item)); return true;}
	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); return true;}
};

template<typename T, typename Container> using non_lock_queue = queue<T, Container, dummy_lockable>; //totally not thread safe
template<typename T, typename Container> using lock_queue = queue<T, Container, lockable>;

//it's not thread safe for 'other', please note. for 'dest', depends on 'Q'
template<typename Q>
size_t move_items_in(Q& dest, Q& other, size_t max_size = ASCS_MAX_MSG_NUM)
{
	if (other.empty())
		return 0;

	auto cur_size = dest.size();
	if (cur_size >= max_size)
		return 0;

	size_t num = 0;
	typename Q::data_type item;

	typename Q::lock_guard lock(dest);
	while (cur_size < max_size && other.try_dequeue_(item)) //size not controlled accurately
	{
		dest.enqueue_(std::move(item));
		++cur_size;
		++num;
	}

	return num;
}

//it's not thread safe for 'other', please note. for 'dest', depends on 'Q'
template<typename Q, typename Q2>
size_t move_items_in(Q& dest, Q2& other, size_t max_size = ASCS_MAX_MSG_NUM)
{
	if (other.empty())
		return 0;

	auto cur_size = dest.size();
	if (cur_size >= max_size)
		return 0;

	size_t num = 0;

	typename Q::lock_guard lock(dest);
	while (cur_size < max_size && !other.empty()) //size not controlled accurately
	{
		dest.enqueue_(std::move(other.front()));
		other.pop_front();
		++cur_size;
		++num;
	}

	return num;
}

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

} //namespace

#endif /* _ASCS_CONTAINER_H_ */