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

template<typename T> using list = std::list<T>;

//Container must at least has the following functions (like list):
// Container() and Container(size_t) constructor
// clear
// swap
// emplace_back(const T& item)
// emplace_back(T&& item)
// splice(iter, Container&), after this, the source container must become empty
// splice(iter, Container&, iter, iter), if macro ASCS_DISPATCH_BATCH_MSG been defined
// front
// pop_front
// back
// begin
// end
template<typename T, typename Container, typename Lockable> //thread safety depends on Container or Lockable
class queue : protected Container, public Lockable
{
public:
	typedef T data_type;

	queue() : buff_size(0) {}
	queue(size_t capacity) : Container(capacity), buff_size(0) {}

	bool is_thread_safe() const {return Lockable::is_lockable();}
	size_t size() const {return buff_size;} //must be thread safe, but doesn't have to be consistent
	bool empty() const {return 0 == size();} //must be thread safe, but doesn't have to be consistent
	void clear() {typename Lockable::lock_guard lock(*this); Container::clear(); buff_size = 0;}
	void swap(Container& other)
	{
		size_t s = 0;
		do_something_to_all(other, [&s](const T& item) {s += item.size();});

		typename Lockable::lock_guard lock(*this);
		Container::swap(other);
		buff_size = s;
	}

	//thread safe
	bool enqueue(const T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(T&& item) {typename Lockable::lock_guard lock(*this); return enqueue_(std::move(item));}
	void move_items_in(Container& src, size_t size_in_byte = 0) {typename Lockable::lock_guard lock(*this); move_items_in_(src, size_in_byte);}
	bool try_dequeue(T& item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}
	//thread safe

	//not thread safe
	bool enqueue_(const T& item)
	{
		try
		{
			this->emplace_back(item);
			buff_size += item.size();
		}
		catch (const std::exception& e)
		{
			unified_out::error_out("cannot hold more objects (%s)", e.what());
			return false;
		}

		return true;
	}

	bool enqueue_(T&& item)
	{
		try
		{
			auto s = item.size();
			this->emplace_back(std::move(item));
			buff_size += s;
		}
		catch (const std::exception& e)
		{
			unified_out::error_out("cannot hold more objects (%s)", e.what());
			return false;
		}

		return true;
	}

	void move_items_in_(Container& src, size_t size_in_byte = 0)
	{
		if (0 == size_in_byte)
			do_something_to_all(src, [&size_in_byte](const T& item) {size_in_byte += item.size();});

		this->splice(this->end(), src);
		buff_size += size_in_byte;
	}

	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); buff_size -= item.size(); return true;}
	//not thread safe

#ifdef ASCS_DISPATCH_BATCH_MSG
	void move_items_out(Container& dest, size_t max_item_num = -1) {typename Lockable::lock_guard lock(*this); move_items_out_(dest, max_item_num);} //thread safe
	void move_items_out_(Container& dest, size_t max_item_num = -1) //not thread safe
	{
		if ((size_t) -1 == max_item_num)
		{
			dest.splice(std::end(dest), *this);
			buff_size = 0;
		}
		else if (max_item_num > 0)
		{
			size_t s = 0, index = 0;
			auto end_iter = this->begin();
			do_something_to_one(*this, [&](const T& item) {if (++index > max_item_num) return true; s += item.size(); ++end_iter; return false;});

			dest.splice(std::end(dest), *this, this->begin(), end_iter);
			buff_size -= s;
		}
	}

	using Container::begin;
	using Container::end;
#endif

private:
	size_t buff_size; //in use
};

template<typename T, typename Container> using non_lock_queue = queue<T, Container, dummy_lockable>; //thread safety depends on Container
template<typename T, typename Container> using lock_queue = queue<T, Container, lockable>;

} //namespace

#endif /* _ASCS_CONTAINER_H_ */
