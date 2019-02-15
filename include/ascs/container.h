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

//Container must at least has the following functions (like std::list):
// Container() and Container(size_t) constructor
// size
// empty
// clear
// swap
// emplace_back(typename Container::const_reference item)
// emplace_back(typename Container::value_type&& item)
// splice(iter, Container&)
// splice(iter, Container&, iter, iter)
// front
// pop_front
// back
// begin
// end
template<typename Container, typename Lockable> //thread safety depends on Container or Lockable
class queue : private Container, public Lockable
{
public:
	using typename Container::value_type;
	using typename Container::size_type;
	using typename Container::reference;
	using typename Container::const_reference;

	queue() : buff_size(0) {}
	queue(size_t capacity) : Container(capacity), buff_size(0) {}

	//thread safe
	bool is_thread_safe() const {return Lockable::is_lockable();}
	size_t size() const {return buff_size;} //must be thread safe, but doesn't have to be consistent
	bool empty() const {return 0 == size();} //must be thread safe, but doesn't have to be consistent
	void clear() {typename Lockable::lock_guard lock(*this); Container::clear(); buff_size = 0;}
	void swap(Container& can)
	{
		auto size_in_byte = ascs::get_size_in_byte(can);

		typename Lockable::lock_guard lock(*this);
		Container::swap(can);
		buff_size = size_in_byte;
	}

	size_t get_size_in_byte() {typename Lockable::lock_guard lock(*this); return get_size_in_byte_();}

	bool enqueue(const_reference item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(value_type&& item) {typename Lockable::lock_guard lock(*this); return enqueue_(std::move(item));}
	void move_items_in(Container& src, size_t size_in_byte = 0) {typename Lockable::lock_guard lock(*this); move_items_in_(src, size_in_byte);}
	bool try_dequeue(reference item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}
	void move_items_out(Container& dest, size_t max_item_num = -1) {typename Lockable::lock_guard lock(*this); move_items_out_(dest, max_item_num);}
	void move_items_out(size_t max_size_in_byte, Container& dest) {typename Lockable::lock_guard lock(*this); move_items_out_(max_size_in_byte, dest);}
	template<typename _Predicate> void do_something_to_all(const _Predicate& __pred) {typename Lockable::lock_guard lock(*this); do_something_to_all_(__pred);}
	template<typename _Predicate> void do_something_to_one(const _Predicate& __pred) {typename Lockable::lock_guard lock(*this); do_something_to_one_(__pred);}
	//thread safe

	//not thread safe
	bool enqueue_(const_reference item)
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

	bool enqueue_(value_type&& item)
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
			size_in_byte = ascs::get_size_in_byte(src);

		this->splice(this->end(), src);
		buff_size += size_in_byte;
	}

	bool try_dequeue_(reference item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); buff_size -= item.size(); return true;}

	void move_items_out_(Container& dest, size_t max_item_num = -1)
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
			do_something_to_one_([&](const_reference item) {if (++index > max_item_num) return true; s += item.size(); ++end_iter; return false;});

			if (end_iter == this->end())
				dest.splice(std::end(dest), *this);
			else
				dest.splice(std::end(dest), *this, this->begin(), end_iter);
			buff_size -= s;
		}
	}

	void move_items_out_(size_t max_size_in_byte, Container& dest)
	{
		if ((size_t) -1 == max_size_in_byte)
		{
			dest.splice(std::end(dest), *this);
			buff_size = 0;
		}
		else
		{
			size_t s = 0;
			auto end_iter = this->begin();
			do_something_to_one_([&](const_reference item) {s += item.size(); ++end_iter; if (s >= max_size_in_byte) return true; return false;});

			if (end_iter == this->end())
				dest.splice(std::end(dest), *this);
			else
				dest.splice(std::end(dest), *this, this->begin(), end_iter);
			buff_size -= s;
		}
	}

	template<typename _Predicate>
	void do_something_to_all_(const _Predicate& __pred) {for (auto& item : *this) __pred(item);}
	template<typename _Predicate>
	void do_something_to_all_(const _Predicate& __pred) const {for (auto& item : *this) __pred(item);}

	template<typename _Predicate>
	void do_something_to_one_(const _Predicate& __pred) {for (auto iter = this->begin(); iter != this->end(); ++iter) if (__pred(*iter)) break;}
	template<typename _Predicate>
	void do_something_to_one_(const _Predicate& __pred) const {for (auto iter = this->begin(); iter != this->end(); ++iter) if (__pred(*iter)) break;}

	size_t get_size_in_byte_() const
	{
		size_t size_in_byte = 0;
		do_something_to_all_([&size_in_byte](const_reference item) {size_in_byte += item.size();});
		return size_in_byte;
	}
	//not thread safe

private:
	size_t buff_size; //in use
};

//ascs requires that queue must take one and only one template argument
template<typename Container> using non_lock_queue = queue<Container, dummy_lockable>; //thread safety depends on Container
template<typename Container> using lock_queue = queue<Container, lockable>;

} //namespace

#endif /* _ASCS_CONTAINER_H_ */
