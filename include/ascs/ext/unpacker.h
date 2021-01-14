/*
 * unpacker.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * unpackers
 */

#ifndef _ASCS_EXT_UNPACKER_H_
#define _ASCS_EXT_UNPACKER_H_

#include <array>

#include "ext.h"

namespace ascs { namespace ext {

class unpacker_helper
{
public:
	static void dump_left_data(const char* data, size_t cur_msg_len, size_t remain_len)
	{
		if (nullptr != data && remain_len > 0)
		{
			std::stringstream os;
			for (size_t i = 0; i < remain_len; ++i, ++data)
			{
				if (i > 0)
				{
					if (0 == (i % 8))
						os << std::endl;
					else if (0 == (i % 4))
						os << "  ";
					else
						os << ' ';
				}
				os << std::setfill('0') << std::setw(2) << std::hex << (int) (unsigned char) *data;
			}

			if ((size_t) -1 == cur_msg_len)
				cur_msg_len = 0; //shorten the logs
			unified_out::error_out("unparsed data (current msg length is: " ASCS_SF ") are:\n%s", cur_msg_len, os.str().data());
		}
	}
};

//protocol: length + body
//T can be std::string or basic_buffer
template<typename T = std::string>
class unpacker : public i_unpacker<T>
{
private:
	typedef i_unpacker<T> super;

public:
	unpacker() {reset();}
	size_t current_msg_length() const {return cur_msg_len;} //current msg's total length, -1 means not available

	bool parse_msg(std::list<std::pair<const char*, size_t>>& msg_can)
	{
		auto pnext = &*std::begin(raw_buff);
		auto unpack_ok = true;
		while (unpack_ok) //considering sticky package problem, we need a loop
			if ((size_t) -1 != cur_msg_len)
			{
				if (cur_msg_len > ASCS_MSG_BUFFER_SIZE || cur_msg_len < ASCS_HEAD_LEN)
					unpack_ok = false;
				else if (remain_len >= cur_msg_len) //one msg received
				{
					msg_can.emplace_back(pnext, cur_msg_len);
					remain_len -= cur_msg_len;
					std::advance(pnext, cur_msg_len);
					cur_msg_len = -1;
				}
				else
					break;
			}
			else if (remain_len >= ASCS_HEAD_LEN) //the msg's head been received, sticky package found
			{
				ASCS_HEAD_TYPE head;
				memcpy(&head, pnext, ASCS_HEAD_LEN);
				cur_msg_len = ASCS_HEAD_N2H(head);
#ifdef ASCS_HUGE_MSG
				if ((size_t) -1 == cur_msg_len) //avoid dead loop on 32bit system with macro ASCS_HUGE_MSG
					unpack_ok = false;
#endif
			}
			else
				break;

		if (pnext == &*std::begin(raw_buff)) //we should have at least got one msg.
			unpack_ok = false;

		return unpack_ok;
	}

public:
	virtual void reset() {cur_msg_len = -1; remain_len = 0;}
	virtual void dump_left_data() const {unpacker_helper::dump_left_data(raw_buff.data(), cur_msg_len, remain_len);}
	virtual bool parse_msg(size_t bytes_transferred, typename super::container_type& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= ASCS_MSG_BUFFER_SIZE);

		std::list<std::pair<const char*, size_t>> msg_pos_can;
		auto unpack_ok = parse_msg(msg_pos_can);
		do_something_to_all(msg_pos_can, [this, &msg_can](decltype(msg_pos_can.front()) item) {
			if (item.second > ASCS_HEAD_LEN) //ignore heartbeat
			{
				if (this->stripped())
					msg_can.emplace_back(std::next(item.first, ASCS_HEAD_LEN), item.second - ASCS_HEAD_LEN);
				else
					msg_can.emplace_back(item.first, item.second);
			}
		});

		if (remain_len > 0 && !msg_pos_can.empty())
		{
			auto pnext = std::next(msg_pos_can.back().first, msg_pos_can.back().second);
			memmove(&*std::begin(raw_buff), pnext, remain_len); //left behind unparsed data
		}

		//if unpacking failed, successfully parsed msgs will still returned via msg_can(sticky package), please note.
		return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---asio::async_read
	//read as many as possible to reduce asynchronous call-back, and don't forget to handle sticky package carefully in parse_msg function.
	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = remain_len + bytes_transferred;
		assert(data_len <= ASCS_MSG_BUFFER_SIZE);

		if ((size_t) -1 == cur_msg_len && data_len >= ASCS_HEAD_LEN) //the msg's head been received
		{
			ASCS_HEAD_TYPE head;
			memcpy(&head, &*std::begin(raw_buff), ASCS_HEAD_LEN);
			cur_msg_len = ASCS_HEAD_N2H(head);
			if (cur_msg_len > ASCS_MSG_BUFFER_SIZE || cur_msg_len < ASCS_HEAD_LEN) //invalid msg, stop reading
				return 0;
		}

		return data_len >= cur_msg_len ? 0 : ASCS_MSG_BUFFER_SIZE;
		//read as many as possible except that we have already got an entire msg
	}

#ifdef ASCS_SCATTERED_RECV_BUFFER
	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
	virtual typename super::buffer_type prepare_next_recv() {assert(remain_len < ASCS_MSG_BUFFER_SIZE); return typename super::buffer_type(1, asio::buffer(raw_buff) + remain_len);}
#elif ASIO_VERSION < 101100
	virtual typename super::buffer_type prepare_next_recv() {assert(remain_len < ASCS_MSG_BUFFER_SIZE); return asio::buffer(asio::buffer(raw_buff) + remain_len);}
#else
	virtual typename super::buffer_type prepare_next_recv() {assert(remain_len < ASCS_MSG_BUFFER_SIZE); return asio::buffer(raw_buff) + remain_len;}
#endif

	//msg must has been unpacked by this unpacker
	virtual char* raw_data(typename super::msg_type& msg) const {return const_cast<char*>(this->stripped() ? msg.data() : std::next(msg.data(), ASCS_HEAD_LEN));}
	virtual const char* raw_data(typename super::msg_ctype& msg) const {return this->stripped() ? msg.data() : std::next(msg.data(), ASCS_HEAD_LEN);}
	virtual size_t raw_data_len(typename super::msg_ctype& msg) const {return this->stripped() ? msg.size() : msg.size() - ASCS_HEAD_LEN;}

protected:
	std::array<char, ASCS_MSG_BUFFER_SIZE> raw_buff;
	size_t cur_msg_len; //-1 means head not received, so msg length is not available.
	size_t remain_len; //half-baked msg
};

//protocol: length + body
//this unpacker has a fixed buffer (4000 bytes), if messages can be held in it, then this unpacker works just as the default unpacker,
// otherwise, a dynamic std::string will be created to hold big messages, then this unpacker works just as the non_copy_unpacker.
//T can be std::string or basic_buffer, the latter will not fill its buffer in resize invocation, so is more efficient.
template<typename T = std::string>
class flexible_unpacker : public i_unpacker<T>
{
private:
	typedef i_unpacker<T> super;

public:
	flexible_unpacker() {reset();}
	size_t current_msg_length() const {return cur_msg_len;} //current msg's total length, -1 means not available

	bool parse_msg(std::list<std::pair<const char*, size_t>>& msg_can)
	{
		auto pnext = &*std::begin(raw_buff);
		auto unpack_ok = true;
		while (unpack_ok) //considering sticky package problem, we need a loop
			if ((size_t) -1 != cur_msg_len)
			{
				if (cur_msg_len > ASCS_MSG_BUFFER_SIZE || cur_msg_len < ASCS_HEAD_LEN)
					unpack_ok = false;
				else if (remain_len >= cur_msg_len) //one msg received
				{
					msg_can.emplace_back(pnext, cur_msg_len);
					remain_len -= cur_msg_len;
					std::advance(pnext, cur_msg_len);
					cur_msg_len = -1;
				}
				else
					break;
			}
			else if (remain_len >= ASCS_HEAD_LEN) //the msg's head been received, sticky package found
			{
				ASCS_HEAD_TYPE head;
				memcpy(&head, pnext, ASCS_HEAD_LEN);
				cur_msg_len = ASCS_HEAD_N2H(head);
#ifdef ASCS_HUGE_MSG
				if ((size_t) -1 == cur_msg_len) //avoid dead loop on 32bit system with macro ASCS_HUGE_MSG
					unpack_ok = false;
#endif
			}
			else
				break;

		if (pnext == &*std::begin(raw_buff)) //we should have at least got one msg.
			unpack_ok = false;

		return unpack_ok;
	}

public:
	virtual void reset() {big_msg.clear(); cur_msg_len = -1; remain_len = 0;}
	virtual void dump_left_data() const {unpacker_helper::dump_left_data(big_msg.empty() ? raw_buff.data() : big_msg.data(), cur_msg_len, remain_len);}
	virtual bool parse_msg(size_t bytes_transferred, typename super::container_type& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= std::max(raw_buff.size(), big_msg.size()));

		if (!big_msg.empty())
		{
			if (remain_len != big_msg.size())
				return false;

			msg_can.emplace_back(std::move(big_msg));
			reset();
			return true;
		}

		if ((size_t) -1 == cur_msg_len && remain_len >= ASCS_HEAD_LEN) //the msg's head been received
		{
			ASCS_HEAD_TYPE head;
			memcpy(&head, &*std::begin(raw_buff), ASCS_HEAD_LEN);
			cur_msg_len = ASCS_HEAD_N2H(head);
		}

		if (cur_msg_len <= ASCS_MSG_BUFFER_SIZE && cur_msg_len > raw_buff.size()) //big message
		{
			extend_buffer();
			return true;
		}

		std::list<std::pair<const char*, size_t>> msg_pos_can;
		auto unpack_ok = parse_msg(msg_pos_can);
		do_something_to_all(msg_pos_can, [this, &msg_can](decltype(msg_pos_can.front()) item) {
			if (item.second > ASCS_HEAD_LEN) //ignore heartbeat
			{
				if (this->stripped())
					msg_can.emplace_back(std::next(item.first, ASCS_HEAD_LEN), item.second - ASCS_HEAD_LEN);
				else
					msg_can.emplace_back(item.first, item.second);
			}
		});

		if (remain_len > 0 && !msg_pos_can.empty())
		{
			auto pnext = std::next(msg_pos_can.back().first, msg_pos_can.back().second);
			memmove(&*std::begin(raw_buff), pnext, remain_len); //left behind unparsed data
		}

		if (unpack_ok && (size_t) -1 != cur_msg_len && cur_msg_len > raw_buff.size()) //big message
			extend_buffer();

		//if unpacking failed, successfully parsed msgs will still returned via msg_can(sticky package), please note.
		return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---asio::async_read
	//read as many as possible to reduce asynchronous call-back, and don't forget to handle sticky package carefully in parse_msg function.
	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = remain_len + bytes_transferred;
		assert(data_len <= cur_msg_len);

		if ((size_t) -1 == cur_msg_len && data_len >= ASCS_HEAD_LEN) //the msg's head been received
		{
			ASCS_HEAD_TYPE head;
			memcpy(&head, &*std::begin(raw_buff), ASCS_HEAD_LEN);
			cur_msg_len = ASCS_HEAD_N2H(head);
			if (cur_msg_len > ASCS_MSG_BUFFER_SIZE || cur_msg_len < ASCS_HEAD_LEN || //invalid msg, stop reading
				cur_msg_len > raw_buff.size()) //big message
				return 0;
		}

		return data_len >= cur_msg_len ? 0 : (big_msg.empty() ? raw_buff.size() : big_msg.size());
		//read as many as possible except that we have already got an entire msg
	}

#ifdef ASCS_SCATTERED_RECV_BUFFER
	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
	virtual typename super::buffer_type prepare_next_recv()
	{
		assert(remain_len < cur_msg_len);
		if (big_msg.empty())
			return typename super::buffer_type(1, asio::buffer(raw_buff) + remain_len);

		return typename super::buffer_type(1, asio::buffer(const_cast<char*>(big_msg.data()), big_msg.size()) + remain_len);
	}
#elif ASIO_VERSION < 101100
	virtual typename super::buffer_type prepare_next_recv()
	{
		assert(remain_len < cur_msg_len);
		if (big_msg.empty())
			return asio::buffer(asio::buffer(raw_buff) + remain_len);

		return asio::buffer(asio::buffer(const_cast<char*>(big_msg.data()), big_msg.size()) + remain_len);
	}
#else
	virtual typename super::buffer_type prepare_next_recv()
	{
		assert(remain_len < cur_msg_len);
		if (big_msg.empty())
			return asio::buffer(raw_buff) + remain_len;

		return asio::buffer(const_cast<char*>(big_msg.data()), big_msg.size()) + remain_len;
	}
#endif

	//msg must has been unpacked by this unpacker
	virtual char* raw_data(typename super::msg_type& msg) const {return const_cast<char*>(this->stripped() ? msg.data() : std::next(msg.data(), ASCS_HEAD_LEN));}
	virtual const char* raw_data(typename super::msg_ctype& msg) const {return this->stripped() ? msg.data() : std::next(msg.data(), ASCS_HEAD_LEN);}
	virtual size_t raw_data_len(typename super::msg_ctype& msg) const {return this->stripped() ? msg.size() : msg.size() - ASCS_HEAD_LEN;}

private:
	void extend_buffer()
	{
		auto step = 0;
		if (this->stripped())
		{
			cur_msg_len -= ASCS_HEAD_LEN;
			remain_len -= ASCS_HEAD_LEN;
			step = ASCS_HEAD_LEN;
		}

		big_msg.resize(cur_msg_len); //std::string will fill big_msg, which is totally not necessary for this scenario, but basic_buffer won't.
		memcpy(const_cast<char*>(big_msg.data()), std::next(raw_buff.data(), step), remain_len);
	}

protected:
	std::array<char, 4000> raw_buff;
	typename super::msg_type big_msg;
	size_t cur_msg_len; //-1 means head not received, so msg length is not available.
	size_t remain_len; //half-baked msg
};

//protocol: UDP has message boundary, so we don't need a specific protocol to unpack it.
//this unpacker doesn't support heartbeat, please note.
class udp_unpacker : public i_unpacker<std::string>
{
public:
	virtual void reset() {}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
		{assert(bytes_transferred <= ASCS_MSG_BUFFER_SIZE); msg_can.emplace_back(raw_buff.data(), bytes_transferred); return true;}

#ifdef ASCS_SCATTERED_RECV_BUFFER
	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
	virtual buffer_type prepare_next_recv() {return buffer_type(1, asio::buffer(raw_buff));}
#else
	virtual buffer_type prepare_next_recv() {return asio::buffer(raw_buff);}
#endif

protected:
	std::array<char, ASCS_MSG_BUFFER_SIZE> raw_buff;
};

//protocol: length + body
//Buffer can be unique_buffer or shared_buffer, the latter makes output messages seemingly copyable.
//T can be std::string or basic_buffer, and the output message type will be Buffer<T>.
//Unpacker can be the default unpacker or flexible_unpacker, which means unpacker2 is just a wrapper.
template<template<typename> class Buffer = shared_buffer, typename T = std::string, typename Unpacker = unpacker<>>
class unpacker2 : public i_unpacker<Buffer<T>>
{
private:
	typedef i_unpacker<Buffer<T>> super;

public:
	virtual void stripped(bool stripped_) {super::stripped(stripped_); unpacker_.stripped(stripped_);}

	virtual void reset() {unpacker_.reset();}
	virtual void dump_left_data() const {unpacker_.dump_left_data();}
	virtual bool parse_msg(size_t bytes_transferred, typename super::container_type& msg_can)
	{
		typename Unpacker::container_type tmp_can;
		auto unpack_ok = unpacker_.parse_msg(bytes_transferred, tmp_can);
		do_something_to_all(tmp_can, [&msg_can](typename Unpacker::msg_type& item) {msg_can.emplace_back(new T(std::move(item)));});

		//if unpacking failed, successfully parsed msgs will still returned via msg_can(sticky package), please note.
		return unpack_ok;
	}

	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) {return unpacker_.completion_condition(ec, bytes_transferred);}
	virtual typename super::buffer_type prepare_next_recv() {return unpacker_.prepare_next_recv();}

	//msg must has been unpacked by this unpacker
	virtual char* raw_data(typename super::msg_type& msg) const {return unpacker_.raw_data(*msg.raw_buffer());}
	virtual const char* raw_data(typename super::msg_ctype& msg) const {return unpacker_.raw_data(*msg.raw_buffer());}
	virtual size_t raw_data_len(typename super::msg_ctype& msg) const {return unpacker_.raw_data_len(*msg.raw_buffer());}

protected:
	Unpacker unpacker_;
};

//protocol: UDP has message boundary, so we don't need a specific protocol to unpack it.
//T can be unique_buffer<std::string> or shared_buffer<std::string>, the latter makes output messages seemingly copyable.
template<typename T = shared_buffer<std::string>>
class udp_unpacker2 : public i_unpacker<T>
{
private:
	typedef i_unpacker<T> super;

public:
	virtual void reset() {}
	virtual bool parse_msg(size_t bytes_transferred, typename super::container_type& msg_can)
	{
		assert(bytes_transferred <= ASCS_MSG_BUFFER_SIZE);

		msg_can.emplace_back(new std::string(raw_buff.data(), bytes_transferred));
		return true;
	}

#ifdef ASCS_SCATTERED_RECV_BUFFER
	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
	virtual typename super::buffer_type prepare_next_recv() {return typename super::buffer_type(1, asio::buffer(raw_buff));}
#else
	virtual typename super::buffer_type prepare_next_recv() {return asio::buffer(raw_buff);}
#endif

protected:
	std::array<char, ASCS_MSG_BUFFER_SIZE> raw_buff;
};

//protocol: length + body
//let asio write msg directly (no temporary memory needed), not support unstripped messages, please note (you can fix this defect if you like).
//actually, this unpacker has the worst performance, because it needs 2 read for one message, other unpackers are able to get many messages from just one read.
//so this unpacker just demonstrates a way to avoid memory replications and temporary memory utilization, it can provide better performance for huge messages.
class non_copy_unpacker : public i_unpacker<basic_buffer>
{
private:
	typedef i_unpacker<basic_buffer> super;

public:
	non_copy_unpacker() {reset();}
	size_t current_msg_length() const {return raw_buff.size();} //current msg's total length(not include the head), 0 means not available

public:
	virtual void stripped(bool stripped_)
		{if (!stripped_) unified_out::error_out("non_copy_unpacker doesn't support unstripped messages"); else super::stripped(stripped_);}

	virtual void reset() {raw_buff.clear(); step = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		if (0 == step) //the head been received
		{
			assert(raw_buff.empty());
			if (ASCS_HEAD_LEN != bytes_transferred)
			{
				unpacker_helper::dump_left_data((const char*) &head, ASCS_HEAD_LEN, bytes_transferred);
				return false;
			}

			auto cur_msg_len = ASCS_HEAD_N2H(head) - ASCS_HEAD_LEN;
			if (cur_msg_len > ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN) //invalid size
			{
				unpacker_helper::dump_left_data((const char*) &head, cur_msg_len, bytes_transferred);
				return false;
			}
			else if (cur_msg_len > 0) //exclude heartbeat
			{
				raw_buff.assign(cur_msg_len); assert(!raw_buff.empty());
				step = 1;
			}
		}
		else if (1 == step) //the body been received
		{
			assert(!raw_buff.empty());
			if (bytes_transferred != raw_buff.size())
			{
				unpacker_helper::dump_left_data(raw_buff.data(), raw_buff.size(), bytes_transferred);
				return false;
			}

			msg_can.emplace_back(std::move(raw_buff));
			step = 0;
		}

		return true;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---asio::async_read
	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		if (0 == step) //want the head
		{
			assert(raw_buff.empty());
			return ASCS_HEAD_LEN;
		}
		else if (1 == step) //want the body
		{
			assert(!raw_buff.empty());
			return raw_buff.size();
		}
		else
			assert(false);

		return 0;
	}

	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
#ifdef ASCS_SCATTERED_RECV_BUFFER
	virtual buffer_type prepare_next_recv() {return buffer_type(1, raw_buff.empty() ? asio::buffer((char*) &head, ASCS_HEAD_LEN) : asio::buffer(raw_buff.data(), raw_buff.size()));}
#else
	virtual buffer_type prepare_next_recv() {return raw_buff.empty() ? asio::buffer((char*) &head, ASCS_HEAD_LEN) : asio::buffer(raw_buff.data(), raw_buff.size());}
#endif

private:
	ASCS_HEAD_TYPE head;
	//please note that we don't have a fixed size array with maximum size any more(like the default unpacker).
	//this is very useful if you have a few type of msgs which are very large, fox example: you have a type of very large msg(1M size),
	//but all others are very small, if you use the default unpacker, all unpackers must have a fixed buffer with at least 1M size, each socket has a unpacker,
	//this will cause your application to occupy very large memory but with very low utilization ratio.
	//this non_copy_unpacker will resolve above problem, and with another benefit: no memory replication needed any more.
	msg_type raw_buff;
	int step; //-1-error format, 0-want the head, 1-want the body
};

//protocol: fixed length
//non-copy, let asio write msg directly (no temporary memory needed), actually, this unpacker has poor performance, because it needs one read for one message,
// other unpackers are able to get many messages from just one read, so this unpacker just demonstrates a way to avoid memory replications and temporary memory
// utilization, it can provide better performance for huge messages.
//this unpacker doesn't support heartbeat, please note.
class fixed_length_unpacker : public i_unpacker<basic_buffer>
{
public:
	fixed_length_unpacker() : _fixed_length(1024) {}

	void fixed_length(size_t fixed_length) {assert(0 < fixed_length && fixed_length <= ASCS_MSG_BUFFER_SIZE); _fixed_length = fixed_length;}
	size_t fixed_length() const {return _fixed_length;}

public:
	virtual void reset() {}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		if (bytes_transferred != raw_buff.size())
		{
			unpacker_helper::dump_left_data(raw_buff.data(), raw_buff.size(), bytes_transferred);
			return false;
		}

		msg_can.emplace_back(std::move(raw_buff));
		return true;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---asio::async_read
	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) {return ec || bytes_transferred == raw_buff.size() ? 0 : _fixed_length;}

	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
#ifdef ASCS_SCATTERED_RECV_BUFFER
	virtual buffer_type prepare_next_recv() {raw_buff.assign(_fixed_length); return buffer_type(1, asio::buffer(raw_buff.data(), raw_buff.size()));}
#else
	virtual buffer_type prepare_next_recv() {raw_buff.assign(_fixed_length); return asio::buffer(raw_buff.data(), raw_buff.size());}
#endif

private:
	msg_type raw_buff;
	size_t _fixed_length;
};

//protocol: [prefix] + body + suffix
class prefix_suffix_unpacker : public i_unpacker<std::string>
{
public:
	prefix_suffix_unpacker() {reset();}

	void prefix_suffix(const std::string& prefix, const std::string& suffix) {assert(!suffix.empty() && prefix.size() + suffix.size() < ASCS_MSG_BUFFER_SIZE); _prefix = prefix; _suffix = suffix;}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

	size_t peek_msg(size_t data_len, const char* buff)
	{
		assert(nullptr != buff);

		if ((size_t) -1 == cur_msg_len)
		{
			if (data_len >= _prefix.size())
			{
				if (0 != memcmp(_prefix.data(), buff, _prefix.size()))
					return 0; //invalid msg, stop reading
				else
					cur_msg_len = 0; //prefix been checked.
			}
		}
		else if (0 != cur_msg_len)
			return 0;

		auto min_len = _prefix.size() + _suffix.size();
		if (data_len > min_len)
		{
			auto end = (const char*) memmem(std::next(buff, _prefix.size()), data_len - _prefix.size(), _suffix.data(), _suffix.size());
			if (nullptr != end)
			{
				cur_msg_len = std::distance(buff, end) + _suffix.size(); //got a msg
				return 0;
			}
			else if (data_len >= ASCS_MSG_BUFFER_SIZE)
				return 0; //invalid msg, stop reading
		}

		return ASCS_MSG_BUFFER_SIZE; //read as many as possible
	}

	//like strstr, except support \0 in the middle of mem and sub_mem
	static const void* memmem(const void* mem, size_t len, const void* sub_mem, size_t sub_len)
	{
		if (nullptr != mem && nullptr != sub_mem && sub_len <= len)
		{
			auto valid_len = len - sub_len;
			for (size_t i = 0; i <= valid_len; ++i, mem = (const char*) mem + 1)
				if (0 == memcmp(mem, sub_mem, sub_len))
					return mem;
		}

		return nullptr;
	}

public:
	virtual void reset() {cur_msg_len = -1; remain_len = 0;}
	virtual void dump_left_data() const {unpacker_helper::dump_left_data(raw_buff.data(), cur_msg_len, remain_len);}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= ASCS_MSG_BUFFER_SIZE);

		auto pnext = &*std::begin(raw_buff);
		auto min_len = _prefix.size() + _suffix.size();
		while (0 == peek_msg(remain_len, pnext) && (size_t) -1 != cur_msg_len && 0 != cur_msg_len)
		{
			assert(cur_msg_len >= min_len);
			if (cur_msg_len > min_len) //exclude heartbeat
			{
				if (stripped())
					msg_can.emplace_back(std::next(pnext, _prefix.size()), cur_msg_len - min_len);
				else
					msg_can.emplace_back(pnext, cur_msg_len);
			}
			remain_len -= cur_msg_len;
			std::advance(pnext, cur_msg_len);
			cur_msg_len = -1;
		}

		if (pnext == &*std::begin(raw_buff)) //we should have at least got one msg.
			return false;
		else if (remain_len > 0)
			memmove(&*std::begin(raw_buff), pnext, remain_len); //left behind unparsed msg

		return true;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---asio::async_read
	//read as many as possible to reduce asynchronous call-back, and don't forget to handle sticky package carefully in parse_msg function.
	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = remain_len + bytes_transferred;
		assert(data_len <= ASCS_MSG_BUFFER_SIZE);

		return peek_msg(data_len, &*std::begin(raw_buff));
	}

	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
#ifdef ASCS_SCATTERED_RECV_BUFFER
	virtual buffer_type prepare_next_recv() {assert(remain_len < ASCS_MSG_BUFFER_SIZE); return buffer_type(1, asio::buffer(raw_buff) + remain_len);}
#elif ASIO_VERSION < 101100
	virtual buffer_type prepare_next_recv() {assert(remain_len < ASCS_MSG_BUFFER_SIZE); return asio::buffer(asio::buffer(raw_buff) + remain_len);}
#else
	virtual buffer_type prepare_next_recv() {assert(remain_len < ASCS_MSG_BUFFER_SIZE); return asio::buffer(raw_buff) + remain_len;}
#endif

	//msg must has been unpacked by this unpacker
	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(stripped() ? msg.data() : std::next(msg.data(), _prefix.size()));}
	virtual const char* raw_data(msg_ctype& msg) const {return stripped() ? msg.data() : std::next(msg.data(), _prefix.size());}
	virtual size_t raw_data_len(msg_ctype& msg) const {return stripped() ? msg.size() : msg.size() - _prefix.size() - _suffix.size();}

private:
	std::array<char, ASCS_MSG_BUFFER_SIZE> raw_buff;
	std::string _prefix, _suffix;
	size_t cur_msg_len; //-1 means prefix not received, 0 means prefix received but suffix not received, otherwise message length (include prefix and suffix)
	size_t remain_len; //half-baked msg
};

//protocol: stream (non-protocol)
//this unpacker doesn't support heartbeat, please note.
class stream_unpacker : public i_unpacker<std::string>
{
public:
	virtual void reset() {}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		if (0 == bytes_transferred)
			return false;

		assert(bytes_transferred <= ASCS_MSG_BUFFER_SIZE);

		msg_can.emplace_back(raw_buff.data(), bytes_transferred);
		return true;
	}

	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) {return ec || bytes_transferred > 0 ? 0 : ASCS_MSG_BUFFER_SIZE;}

	//this is just to satisfy the compiler, it's not a real scatter-gather buffer,
	//if you introduce a ring buffer, then you will have the chance to provide a real scatter-gather buffer.
#ifdef ASCS_SCATTERED_RECV_BUFFER
	virtual buffer_type prepare_next_recv() {return buffer_type(1, asio::buffer(raw_buff));}
#else
	virtual buffer_type prepare_next_recv() {return asio::buffer(raw_buff);}
#endif

protected:
	std::array<char, ASCS_MSG_BUFFER_SIZE> raw_buff;
};

}} //namespace

#endif /* _ASCS_EXT_UNPACKER_H_ */
