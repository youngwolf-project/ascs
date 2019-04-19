/*
 * base.h
 *
 *  Created on: 2019-4-18
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * forward list, but has size, push_back and emplace_back functions.
 */

#ifndef _ASCS_LIST_H_
#define _ASCS_LIST_H_

#include <forward_list>

namespace ascs
{

//ascs requires that container must take one and only one template argument
template<typename T>
class list : protected std::forward_list<T>
{
private:
	typedef std::forward_list<T> super;

public:
	using typename super::value_type;
	using typename super::size_type;
	using typename super::reference;
	using typename super::const_reference;
	using typename super::iterator;
	using typename super::const_iterator;

	using super::empty;
	using super::max_size;
	using super::begin;
	using super::cbegin;
	using super::front;
	using super::end;
	using super::cend;

public:
	list() {refresh_status();}
	list(size_type count) : super(count) {refresh_status();}

	list(super&& other) {swap(other);}
	list(const super& other) {*this = other;}
	list(list&& other) {refresh_status(); swap(other);}
	list(const list& other) {*this = other;}

	list& operator=(super&& other) {super::clear(); swap(other); return *this;}
	list& operator=(const super& other) {super::operator=(other); refresh_status(); return *this;}
	list& operator=(list&& other) {clear(); swap(other); return *this;}
	list& operator=(const list& other) {super::operator=(other); refresh_status(); return *this;}

	void swap(super& other) {super::swap(other); refresh_status();}
	void swap(list& other) {super::swap(other); std::swap(s, other.s); std::swap(back_iter, other.back_iter);}

	void refresh_status()
	{
		s = 0;
		back_iter = this->before_begin();
		for (auto iter = std::begin(*this); iter != std::end(*this); ++iter, ++back_iter)
			++s;
	}

	void clear() {super::clear(); refresh_status();}
	size_type size() const {return s;}

	void push_front(T&& _Val) {super::push_front(std::forward<T>(_Val)); if (1 == ++s) back_iter = std::begin(*this);}
	template<class... _Valty> void emplace_front(_Valty&&... _Val) {super::emplace_front(std::forward<_Valty>(_Val)...); if (1 == ++s) back_iter = std::begin(*this);}
	void pop_front() {super::pop_front(); if (0 == --s) back_iter = std::end(*this);}

	void push_back(T&& _Val) {back_iter = this->insert_after(1 == ++s ? this->cbefore_begin() : back_iter, std::forward<T>(_Val));}
	template<class... _Valty> void emplace_back(_Valty&& ... _Val) {back_iter = this->emplace_after(1 == ++s ? this->cbefore_begin() : back_iter, std::forward<_Valty>(_Val)...);}
	reference back() {return *back_iter;}
	const_reference back() const {return *back_iter;}

	void splice_after(super& other)
	{
		if (!other.empty())
		{
			super::splice_after(empty() ? this->cbefore_begin() : back_iter, other);
			refresh_status();
		}
	}
#ifdef _MSC_VER
	void splice_after(list& other) {splice_after((super&) other); other.refresh_status();}
#else
	void splice_after(list& other)
	{
		if (!other.empty())
		{
			s += other.s;
			super::splice_after(empty() ? this->cbefore_begin() : back_iter, other);
			back_iter = other.back_iter;

			other.refresh_status();
		}
	}
#endif

	void splice_after_until(super& other, const_iterator last) {super::splice_after(empty() ? this->cbefore_begin() : back_iter, other, other.cbefore_begin(), last); refresh_status();}
	void splice_after_until(list& other, const_iterator last) {splice_after_until((super&) other, last); other.refresh_status();}

private:
	size_type s;
	iterator back_iter; //for empty list, this item can be either before_begin() or end(), please note.
};

} //namespace

#endif /* _ASCS_LIST_H_ */
