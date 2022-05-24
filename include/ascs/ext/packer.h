/*
 * packer.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * packers
 */

#ifndef _ASCS_EXT_PACKER_H_
#define _ASCS_EXT_PACKER_H_

#include "ext.h"

namespace ascs { namespace ext {

class packer_helper
{
public:
	//return (size_t) -1 means length exceeded the ASCS_MSG_BUFFER_SIZE
	static size_t msg_size_check(size_t pre_len, const char* const pstr[], const size_t len[], size_t num)
	{
		if (nullptr == pstr || nullptr == len)
			return -1;

		auto total_len = pre_len;
		auto last_total_len = total_len;
		for (size_t i = 0; i < num; ++i)
			if (nullptr != pstr[i])
			{
				total_len += len[i];
				if (last_total_len > total_len || total_len > ASCS_MSG_BUFFER_SIZE) //overflow
				{
					unified_out::error_out("pack msg error: length exceeded the ASCS_MSG_BUFFER_SIZE!");
					return -1;
				}
				last_total_len = total_len;
			}

		return total_len;
	}

	static ASCS_HEAD_TYPE pack_header(size_t len)
	{
		assert(len < ASCS_MSG_BUFFER_SIZE);
		auto total_len = ASCS_HEAD_LEN + len;
		assert(total_len <= ASCS_MSG_BUFFER_SIZE);
		auto head_len = (ASCS_HEAD_TYPE) total_len;
		assert(head_len == total_len);

		return ASCS_HEAD_H2N(head_len);
	}
};

//protocol: length + body
//T can be std::string or basic_buffer
template<typename T = std::string>
class packer : public i_packer<T>
{
private:
	typedef i_packer<T> super;

public:
	static size_t get_max_msg_size() {return ASCS_MSG_BUFFER_SIZE - ASCS_HEAD_LEN;}

	packer() {auto head_len = packer_helper::pack_header(0); heartbeat.assign((const char*) &head_len, ASCS_HEAD_LEN);}

	using i_packer<typename super::msg_type>::pack_msg;
	virtual typename super::msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		typename super::msg_type msg;
		auto pre_len = native ? 0 : ASCS_HEAD_LEN;
		auto total_len = packer_helper::msg_size_check(pre_len, pstr, len, num);
		if ((size_t) -1 != total_len && total_len > pre_len)
		{
			if (!native)
			{
				auto head_len = (ASCS_HEAD_TYPE) total_len;
				if (total_len != head_len)
				{
					unified_out::error_out("pack msg error: length exceeded the header's range!");
					return msg;
				}

				head_len = ASCS_HEAD_H2N(head_len);
				msg.reserve(total_len);
				msg.append((const char*) &head_len, ASCS_HEAD_LEN);
			}
			else
				msg.reserve(total_len);

			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
					msg.append(pstr[i], len[i]);
		} //if (total_len > pre_len)

		return msg;
	}
	virtual bool pack_msg(typename super::msg_type&& msg, typename super::container_type& msg_can)
	{
		auto len = msg.size();
		if (len > get_max_msg_size())
			return false;

		auto head_len = packer_helper::pack_header(len);
		msg_can.emplace_back((const char*) &head_len, ASCS_HEAD_LEN);
		msg_can.emplace_back(std::move(msg));

		return true;
	}
	virtual bool pack_msg(typename super::msg_type&& msg1, typename super::msg_type&& msg2, typename super::container_type& msg_can)
	{
		auto len = msg1.size() + msg2.size();
		if (len > get_max_msg_size()) //not considered overflow
			return false;

		auto head_len = packer_helper::pack_header(len);
		msg_can.emplace_back((const char*) &head_len, ASCS_HEAD_LEN);
		msg_can.emplace_back(std::move(msg1));
		msg_can.emplace_back(std::move(msg2));

		return true;
	}
	virtual bool pack_msg(typename super::container_type& in, typename super::container_type& out)
	{
		auto len = ascs::get_size_in_byte(in);
		if (len > get_max_msg_size()) //not considered overflow
			return false;

		auto head_len = packer_helper::pack_header(len);
		out.emplace_back((const char*) &head_len, ASCS_HEAD_LEN);
		out.splice(std::end(out), in);

		return true;
	}
	virtual typename super::msg_type pack_heartbeat() {return typename super::msg_type(heartbeat);}

	//msg must has been packed by this packer with native == false
	virtual char* raw_data(typename super::msg_type& msg) const {return const_cast<char*>(std::next(msg.data(), ASCS_HEAD_LEN));}
	virtual const char* raw_data(typename super::msg_ctype& msg) const {return std::next(msg.data(), ASCS_HEAD_LEN);}
	virtual size_t raw_data_len(typename super::msg_ctype& msg) const {return msg.size() - ASCS_HEAD_LEN;}

private:
	typename super::msg_type heartbeat;
};

//protocol: length + body
//Buffer can be unique_buffer<XXXX> or shared_buffer<XXXX>, the latter makes output messages seemingly copyable.
//T is XXXX or a class that inherit from XXXX (because XXXX can be a virtual interface).
//Packer is the real packer who packs messages, which means packer2 is just a wrapper.
template<typename Buffer = unique_buffer<i_buffer>, typename T = string_buffer, typename Packer = packer<>>
class packer2 : public i_packer<Buffer>
{
private:
	typedef i_packer<Buffer> super;

public:
	static size_t get_max_msg_size() {return Packer::get_max_msg_size();}

	using super::pack_msg;
	virtual typename super::msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		auto raw_msg = new T();
		auto str = Packer().pack_msg(pstr, len, num, native);
		raw_msg->swap(str);
		return typename super::msg_type(raw_msg);
	}
	virtual bool pack_msg(typename super::msg_type&& msg, typename super::container_type& msg_can)
	{
		auto len = msg.size();
		if (len > get_max_msg_size())
			return false;

		auto head_len = packer_helper::pack_header(len);
		auto raw_msg = new T();
		raw_msg->assign((const char*) &head_len, ASCS_HEAD_LEN);
		msg_can.emplace_back(raw_msg);
		msg_can.emplace_back(std::move(msg));

		return true;
	}
	virtual bool pack_msg(typename super::msg_type&& msg1, typename super::msg_type&& msg2, typename super::container_type& msg_can)
	{
		auto len = msg1.size() + msg2.size();
		if (len > get_max_msg_size()) //not considered overflow
			return false;

		auto head_len = packer_helper::pack_header(len);
		auto raw_msg = new T();
		raw_msg->assign((const char*) &head_len, ASCS_HEAD_LEN);
		msg_can.emplace_back(raw_msg);
		msg_can.emplace_back(std::move(msg1));
		msg_can.emplace_back(std::move(msg2));

		return true;
	}
	virtual bool pack_msg(typename super::container_type& in, typename super::container_type& out)
	{
		auto len = ascs::get_size_in_byte(in);
		if (len > get_max_msg_size()) //not considered overflow
			return false;

		auto head_len = packer_helper::pack_header(len);
		auto raw_msg = new T();
		raw_msg->assign((const char*) &head_len, ASCS_HEAD_LEN);
		out.emplace_back(raw_msg);
		out.splice(std::end(out), in);

		return true;
	}
	virtual typename super::msg_type pack_heartbeat()
	{
		auto raw_msg = new T();
		auto str = Packer().pack_heartbeat();
		raw_msg->swap(str);
		return typename super::msg_type(raw_msg);
	}

	//msg must has been packed by this packer with native == false
	virtual char* raw_data(typename super::msg_type& msg) const {return const_cast<char*>(std::next(msg.data(), ASCS_HEAD_LEN));}
	virtual const char* raw_data(typename super::msg_ctype& msg) const {return std::next(msg.data(), ASCS_HEAD_LEN);}
	virtual size_t raw_data_len(typename super::msg_ctype& msg) const {return msg.size() - ASCS_HEAD_LEN;}
};

//protocol: fixed length
class fixed_length_packer : public packer<>
{
public:
	using packer::pack_msg;
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = true) {return packer::pack_msg(pstr, len, num, true);}
	virtual bool pack_msg(msg_type&& msg, container_type& msg_can) {msg_can.emplace_back(std::move(msg)); return true;}
	virtual bool pack_msg(msg_type&& msg1, msg_type&& msg2, container_type& msg_can) {msg_can.emplace_back(std::move(msg1)); msg_can.emplace_back(std::move(msg2)); return true;}
	virtual bool pack_msg(container_type& in, container_type& out) {in.swap(out); return true;}
	//not support heartbeat because fixed_length_unpacker cannot recognize heartbeat message
};

//protocol: [prefix] + body + suffix
class prefix_suffix_packer : public i_packer<std::string>
{
public:
	void prefix_suffix(const std::string& prefix, const std::string& suffix)
		{assert(!suffix.empty() && prefix.size() + suffix.size() < ASCS_MSG_BUFFER_SIZE); _prefix = prefix; _suffix = suffix; heartbeat = prefix + suffix;}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

public:
	using i_packer<msg_type>::pack_msg;
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		msg_type msg;
		auto pre_len = native ? 0 : _prefix.size() + _suffix.size();
		auto total_len = packer_helper::msg_size_check(pre_len, pstr, len, num);
		if ((size_t) -1 != total_len && total_len > pre_len)
		{
			msg.reserve(total_len);
			if (!native)
				msg.append(_prefix);
			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
					msg.append(pstr[i], len[i]);
			if (!native)
				msg.append(_suffix);
		} //if (total_len > pre_len)

		return msg;
	}
	virtual bool pack_msg(msg_type&& msg, container_type& msg_can)
	{
		auto len = _prefix.size() + msg.size() + _suffix.size();
		if (len > ASCS_MSG_BUFFER_SIZE) //not considered overflow
			return false;

		if (!_prefix.empty())
			msg_can.emplace_back(_prefix);
		msg_can.emplace_back(std::move(msg));
		if (!_suffix.empty())
			msg_can.emplace_back(_suffix);

		return true;
	}
	virtual bool pack_msg(msg_type&& msg1, msg_type&& msg2, container_type& msg_can)
	{
		auto len = _prefix.size() + msg1.size() + msg2.size() + _suffix.size();
		if (len > ASCS_MSG_BUFFER_SIZE) //not considered overflow
			return false;

		if (!_prefix.empty())
			msg_can.emplace_back(_prefix);
		msg_can.emplace_back(std::move(msg1));
		msg_can.emplace_back(std::move(msg2));
		if (!_suffix.empty())
			msg_can.emplace_back(_suffix);

		return true;
	}
	virtual bool pack_msg(container_type& in, container_type& out)
	{
		auto len = _prefix.size() + _suffix.size() + ascs::get_size_in_byte(in);
		if (len > ASCS_MSG_BUFFER_SIZE) //not considered overflow
			return false;

		if (!_prefix.empty())
			out.emplace_back(_prefix);
		out.splice(std::end(out), in);
		if (!_suffix.empty())
			out.emplace_back(_suffix);

		return true;
	}
	virtual msg_type pack_heartbeat() {return msg_type(heartbeat);}

	//msg must has been packed by this packer with native == false
	virtual char* raw_data(msg_type& msg) const {return const_cast<char*>(std::next(msg.data(), _prefix.size()));}
	virtual const char* raw_data(msg_ctype& msg) const {return std::next(msg.data(), _prefix.size());}
	virtual size_t raw_data_len(msg_ctype& msg) const {return msg.size() - _prefix.size() - _suffix.size();}

private:
	std::string _prefix, _suffix;
	msg_type heartbeat;
};

}} //namespace

#endif /* _ASCS_EXT_PACKER_H_ */
