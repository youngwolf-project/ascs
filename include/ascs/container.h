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
#if defined(_MSC_VER) || defined(__clang__) || (!defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__) && __GNUC__ >= 5)
template<typename T> using list = std::list<T>;
//on cygwin and mingw, even gcc 7 and 8 still have not made list::size() to be O(1) complexity, which also means function size() and empty() are
// not thread safe, but ascs::queue needs them to be thread safe no matter itself is lockable or dummy lockable (see ascs::queue for more details),
//so, we must use ascs::list instead of std::list for cygwin and mingw (terrible cygwin and mingw).
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

#if	__GNUC__ > 4 || __GNUC_MINOR__ > 8
	typedef const_iterator Iter;
#else
	typedef iterator Iter; //just satisfy old gcc compilers (before gcc 4.9)
#endif

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
			impl.emplace_back();
			++s;
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
	iterator erase(Iter _Where) {--s; return impl.erase(_Where);}

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
	void emplace_back(_Valty&&... _Val) {impl.emplace_back(std::forward<_Valty>(_Val)...); ++s;}
	void pop_back() {--s; impl.pop_back();}
	reference back() {return impl.back();}
	iterator end() {return impl.end();}
	reverse_iterator rend() {return impl.rend();}
	const_reference back() const {return impl.back();}
	const_iterator end() const {return impl.end();}
	const_reverse_iterator rend() const {return impl.rend();}

	void splice(Iter _Where, _Mybase& _Right) {s += _Right.size(); impl.splice(_Where, _Right);}
	void splice(Iter _Where, _Mybase& _Right, Iter _First) {++s; impl.splice(_Where, _Right, _First);}
	void splice(Iter _Where, _Mybase& _Right, Iter _First, Iter _Last)
	{
		auto size = std::distance(_First, _Last);
		//this std::distance invocation is the penalty for making complexity of size() constant.
		s += size;

		impl.splice(_Where, _Right, _First, _Last);
	}

	void splice(Iter _Where, _Myt& _Right) {s += _Right.size(); _Right.s = 0; impl.splice(_Where, _Right.impl);}
	void splice(Iter _Where, _Myt& _Right, Iter _First) {++s; --_Right.s; impl.splice(_Where, _Right.impl, _First);}
	void splice(Iter _Where, _Myt& _Right, Iter _First, Iter _Last)
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

//Container must at least has the following functions (like list):
// Container() and Container(size_t) constructor
// size (must be thread safe, but doesn't have to be consistent, std::list before gcc 5 doesn't meet this requirement, ascs::list does)
// empty (must be thread safe, but doesn't have to be consistent)
// clear
// swap
// emplace_back(const T& item)
// emplace_back(T&& item)
// splice(decltype(Container::end()), std::list<T>&), after this, std::list<T> must be empty
// front
// pop_front
// end
template<typename T, typename Container, typename Lockable> //thread safety depends on Container or Lockable
#ifdef ASCS_DISPATCH_BATCH_MSG
class queue : public Container, public Lockable
#else
class queue : protected Container, public Lockable
#endif
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
	bool enqueue_(const T& item)
		{try {this->emplace_back(item);} catch (const std::exception& e) {unified_out::error_out("cannot hold more objects (%s)", e.what()); return false;} return true;}
	bool enqueue_(T&& item)
		{try {this->emplace_back(std::move(item));} catch (const std::exception& e) {unified_out::error_out("cannot hold more objects (%s)", e.what()); return false;} return true;}
	void move_items_in_(std::list<T>& can) {this->splice(this->end(), can);}
	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); return true;}
};

template<typename T, typename Container> using non_lock_queue = queue<T, Container, dummy_lockable>; //thread safety depends on Container
template<typename T, typename Container> using lock_queue = queue<T, Container, lockable>;

} //namespace

#endif /* _ASCS_CONTAINER_H_ */
