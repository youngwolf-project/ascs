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

#include "base.h"

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

	typedef typename _Mybase::value_type value_type;
	typedef typename _Mybase::size_type size_type;

	typedef typename _Mybase::reference reference;
	typedef typename _Mybase::const_reference const_reference;

	typedef typename _Mybase::iterator iterator;
	typedef typename _Mybase::const_iterator const_iterator;
	typedef typename _Mybase::reverse_iterator reverse_iterator;
	typedef typename _Mybase::const_reverse_iterator const_reverse_iterator;

	list() : s(0) {}
	list(list&& other) : s(0) {swap(other);}

	list& operator=(list&& other) {clear(); swap(other); return *this;}
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
	iterator erase(iterator _Where) {--s; return impl.erase(_Where);} //just satisfy old compilers (for example gcc 4.7)
	iterator erase(const_iterator _Where) {--s; return impl.erase(_Where);}

	void push_front(const _Ty& _Val) {++s; impl.push_front(_Val);}
	void push_front(_Ty&& _Val) {++s; impl.push_front(std::move(_Val));}
	template<class... _Valty>
	void emplace_front(_Valty&&... _Val) {++s; impl.emplace_front(std::forward<_Valty>(_Val)...);}
	void pop_front() {--s; impl.pop_front();}
	reference front() {return impl.front();}
	iterator begin() {return impl.begin();}
	reverse_iterator rbegin() {return impl.rbegin();}
	const_reference front() const {return impl.front();}
	const_iterator begin() const {return impl.begin();}
	const_reverse_iterator rbegin() const {return impl.rbegin();}

	void push_back(const _Ty& _Val) {++s; impl.push_back(_Val);}
	void push_back(_Ty&& _Val) {++s; impl.push_back(std::move(_Val));}
	template<class... _Valty>
	void emplace_back(_Valty&&... _Val) {++s; impl.emplace_back(std::forward<_Valty>(_Val)...);}
	void pop_back() {--s; impl.pop_back();}
	reference back() {return impl.back();}
	iterator end() {return impl.end();}
	reverse_iterator rend() {return impl.rend();}
	const_reference back() const {return impl.back();}
	const_iterator end() const {return impl.end();}
	const_reverse_iterator rend() const {return impl.rend();}

	void splice(iterator _Where, _Mybase& _Right) {s += _Right.size(); impl.splice(_Where, _Right);} //just satisfy old compilers (for example gcc 4.7)
	void splice(const_iterator _Where, _Mybase& _Right) {s += _Right.size(); impl.splice(_Where, _Right);}
	//please add corresponding overloads which take non-const iterators for following 5 functions,
	//i didn't provide them because ascs doesn't use them and it violated the standard, just some old compilers need them.
	void splice(const_iterator _Where, _Mybase& _Right, const_iterator _First) {++s; impl.splice(_Where, _Right, _First);}
	void splice(const_iterator _Where, _Mybase& _Right, const_iterator _First, const_iterator _Last)
	{
		auto size = std::distance(_First, _Last);
		//this std::distance invocation is the penalty for making complexity of size() constant.
		s += size;

		impl.splice(_Where, _Right, _First, _Last);
	}

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
	volatile size_type s;
	_Mybase impl;
};
#endif

class dummy_lockable
{
public:
	typedef std::lock_guard<dummy_lockable> lock_guard;

	//lockable, dummy
	bool is_lockable() const {return false;}
	void lock() const {}
	void unlock() const {}
};

class lockable
{
public:
	typedef std::lock_guard<lockable> lock_guard;

	//lockable
	bool is_lockable() const {return true;}
	void lock() {mutex.lock();}
	void unlock() {mutex.unlock();}

private:
	std::mutex mutex; //std::mutex is more efficient than std::shared_(timed_)mutex
};

//Container must at least has the following functions (like concurrent_queue):
// Container() and Container(size_t) constructor
// move constructor
// size_approx (must be thread safe, but doesn't have to be coherent)
// swap
// enqueue(const T& item)
// enqueue(T&& item)
// try_dequeue(T& item)
template<typename T, typename Container>
class lock_free_queue : public Container, public dummy_lockable //thread safety depends on Container
{
public:
	typedef T data_type;

	lock_free_queue() {}
	lock_free_queue(size_t capacity) : Container(capacity) {}

	bool is_thread_safe() const {return true;}
	size_t size() const {return this->size_approx();}
	bool empty() const {return 0 == size();}
	void clear() {Container(std::move(*this));}
	using Container::swap;

	void move_items_in(std::list<T>& can) {move_items_in_(can);}

	bool enqueue_(const T& item) {return this->enqueue(item);}
	bool enqueue_(T&& item) {return this->enqueue(std::move(item));}
	void move_items_in_(std::list<T>& can) {do_something_to_all(can, [this](T& item) {this->enqueue(std::move(item));}); can.clear();}
	bool try_dequeue_(T& item) {return this->try_dequeue(item);}
};

//Container must at least has the following functions (like list):
// Container() and Container(size_t) constructor
// size (must be thread safe, but doesn't have to be coherent, std::list before gcc 5 doesn't meet this requirement, ascs::list does)
// empty (must be thread safe, but doesn't have to be coherent)
// clear
// swap
// emplace_back(const T& item)
// emplace_back(T&& item)
// splice(Container::const_iterator, std::list<T>&), after this, std::list<T> must be empty
// front
// pop_front
template<typename T, typename Container, typename Lockable> //thread safety depends on Container or Lockable
class queue : public Container, public Lockable
{
public:
	typedef T data_type;

	queue() {}
	queue(size_t capacity) : Container(capacity) {}

	bool is_thread_safe() const {return Lockable::is_lockable();}
	using Container::size;
	using Container::empty;
	void clear() {typename Lockable::lock_guard lock(*this); Container::clear();}
	void swap(Container& other) {typename Lockable::lock_guard lock(*this); Container::swap(other);}

	//thread safe
	bool enqueue(const T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(T&& item) {typename Lockable::lock_guard lock(*this); return enqueue_(std::move(item));}
	void move_items_in(std::list<T>& can) {typename Lockable::lock_guard lock(*this); move_items_in_(can);}
	bool try_dequeue(T& item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}

	//not thread safe
	bool enqueue_(const T& item) {this->emplace_back(item); return true;}
	bool enqueue_(T&& item) {this->emplace_back(std::move(item)); return true;}
	void move_items_in_(std::list<T>& can) {this->splice(std::end(*this), can);}
	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); return true;}
};

template<typename T, typename Container> using non_lock_queue = queue<T, Container, dummy_lockable>; //thread safety depends on Container
template<typename T, typename Container> using lock_queue = queue<T, Container, lockable>;

} //namespace

#endif /* _ASCS_CONTAINER_H_ */
