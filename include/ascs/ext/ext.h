/*
 * ext.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * extensional common.
 */

#ifndef _ASCS_EXT_H_
#define _ASCS_EXT_H_

#include <initializer_list>
#include "../base.h"

//the size of the buffer used when receiving msg, must equal to or larger than the biggest msg size,
//the bigger this buffer is, the more msgs can be received in one time if there are enough msgs buffered in the SOCKET.
//for unpackers who use fixed buffer, every unpacker has a fixed buffer with this size, every tcp::socket_base has an unpacker,
//so this size is not the bigger the better, bigger buffers may waste more memory.
//if you customized the packer and unpacker, the above principle maybe not right anymore, it should depends on your implementations.
#ifndef ASCS_MSG_BUFFER_SIZE
#define ASCS_MSG_BUFFER_SIZE	4000
#endif

//#define ASCS_SCATTERED_RECV_BUFFER
//define this macro will introduce scatter-gather buffers when doing async read, it's very useful under certain situations (for example, ring buffer).
//this macro is used by unpackers only, it doesn't belong to ascs.

#ifdef ASCS_HUGE_MSG
#define ASCS_HEAD_TYPE	uint32_t
#define ASCS_HEAD_H2N	htonl
#define ASCS_HEAD_N2H	ntohl
#else
#define ASCS_HEAD_TYPE	uint16_t
#define ASCS_HEAD_H2N	htons
#define ASCS_HEAD_N2H	ntohs
#endif

#define ASCS_HEAD_LEN	(sizeof(ASCS_HEAD_TYPE))
static_assert(100 * 1024 * 1024 >= ASCS_MSG_BUFFER_SIZE && ASCS_MSG_BUFFER_SIZE >= ASCS_HEAD_LEN, "invalid message buffer size.");

namespace ascs { namespace ext {

//implement i_buffer interface, then protocol can be changed at runtime, see demo file_server for more details.
class string_buffer : public std::string, public i_buffer
{
public:
	virtual bool empty() const {return std::string::empty();}
	virtual size_t size() const {return std::string::size();}
	virtual const char* data() const {return std::string::data();}
};

//a substitute of std::string, because std::string has a small defect which is terrible for unpacking scenario (and protobuf encoding),
// it cannot change its size without fill the extended buffers.
//please note that basic_buffer won't append '\0' to the end of the string (std::string will do), you must appen '\0' characters
// by your own if you want to print it via "%s" formatter, e.g. basic_buffer("123", 4), bb.append("abc", 4).
class basic_buffer
{
public:
	basic_buffer() {do_detach();}
	virtual ~basic_buffer() {clean();}
	basic_buffer(size_t len) {do_detach(); resize(len);}
	basic_buffer(int len) {do_detach(); if (len > 0) resize((size_t) len);}
	basic_buffer(size_t count, char c) {do_detach(); append(count, c);}
	basic_buffer(char* buff) {do_detach(); operator+=(buff);}
	basic_buffer(const char* buff) {do_detach(); operator+=(buff);}
	basic_buffer(const char* buff, size_t len) {do_detach(); append(buff, len);}
	template<size_t size> basic_buffer(char(&buff)[size], size_t len) {do_detach(); append(buff, len);}
	template<size_t size> basic_buffer(const char(&buff)[size], size_t len) {do_detach(); append(buff, len);}
	basic_buffer(std::initializer_list<char> ilist) {do_detach(); operator+=(ilist);}
	basic_buffer(const basic_buffer& other) {do_detach(); append(other);}
	template<typename Buff> basic_buffer(const Buff& other) {do_detach(); append(other);}
	template<typename Buff> basic_buffer(const Buff& other, size_t pos) {do_detach(); if (pos < other.size()) append(std::next(other.data(), pos), other.size() - pos);}
	template<typename Buff> basic_buffer(const Buff& other, size_t pos, size_t len)
	{
		do_detach();
		if (pos < other.size())
			append(std::next(other.data(), pos), pos + len > other.size() ? other.size() - pos : len);
	}
	basic_buffer(basic_buffer&& other) {do_attach(other.buff, other.len, other.cap); other.do_detach();}

	inline basic_buffer& operator=(char c) {resize(0); return operator+=(c);}
	inline basic_buffer& operator=(char* buff) {resize(0); return operator+=(buff);}
	inline basic_buffer& operator=(const char* buff) {resize(0); return operator+=(buff);}
	inline basic_buffer& operator=(std::initializer_list<char> ilist) {resize(0); return operator+=(ilist);}
	inline basic_buffer& operator=(const basic_buffer& other) {resize(0); return operator+=(other);}
	template<typename Buff> inline basic_buffer& operator=(const Buff& other) {resize(0); return operator+=(other);}
	inline basic_buffer& operator=(basic_buffer&& other) {clean(); swap(other); return *this;}

	inline basic_buffer& operator+=(char c) {return append(c);}
	inline basic_buffer& operator+=(char* buff) {return append(buff);}
	inline basic_buffer& operator+=(const char* buff) {return append(buff);}
	inline basic_buffer& operator+=(std::initializer_list<char> ilist) {return append(ilist);}
	template<typename Buff> inline basic_buffer& operator+=(const Buff& other) {return append(other);}

	basic_buffer& append(char c) {return append(1, c);}
	basic_buffer& append(size_t count, char c)
	{
		if (count > 0)
		{
			check_length(count);

			auto capacity = len + count;
			if (capacity > cap)
				reserve(capacity);

			memset(std::next(buff, len), c, count);
			len += (unsigned) count;
		}

		return *this;
	}

	basic_buffer& append(std::initializer_list<char> ilist) {return append(std::begin(ilist), ilist.size());}
	template<typename Buff> basic_buffer& append(const Buff& other) {return append(other.data(), other.size());}
	basic_buffer& append(char* buff) {return append((const char*) buff);}
	basic_buffer& append(const char* buff) {return append(buff, strlen(buff));}
	basic_buffer& append(const char* _buff, size_t _len)
	{
		if (nullptr != _buff && _len > 0)
		{
			check_length(_len);

			auto capacity = len + _len;
			if (capacity > cap)
				reserve(capacity);

			memcpy(std::next(buff, len), _buff, _len);
			len += (unsigned) _len;
		}

		return *this;
	}
	template<size_t size> basic_buffer& append(char(&buff)[size], size_t len) {return append((const char*) buff, std::min(size, len));}
	template<size_t size> basic_buffer& append(const char(&buff)[size], size_t len) {return append((const char*) buff, std::min(size, len));}

	//nonstandard function append2 -- delete the last character if it's '\0' before appending another string.
	//this feature makes basic_buffer to be able to works as std::string, which will append '\0' automatically.
	//please note that the first and the third verion of append2 still need you to provide '\0' at the end of the fed string.
	//usage:
	/*
		basic_buffer bb("1"); //not include the '\0' character
		printf("%s\n", bb.data()); //not ok, random characters can be outputted after "1", use basic_buffer bb("1", 2) instead.
		bb.append2("23"); //include the '\0' character, but just append2, please note.
		printf("%s\n", bb.data()); //ok, additional '\0' has been added automatically so no random characters can be outputted after "123".
		bb.append2("456", 4); //include the '\0' character
		printf("%s\n", bb.data()); //ok, additional '\0' has been added manually so no random characters can be outputted after "123456".
		bb.append2("789", 3); //not include the '\0' character
		printf("%s\n", bb.data()); //not ok, random characters can be outputted after "123456789"
		bb.append2({'0', '0', '0', '\0'}); //the last character '9' will not be deleted because it's not '\0'
		printf("%s\n", bb.data()); //ok, additional '\0' has been added manually so no random characters can be outputted after "123456789000".
		printf("%zu\n", bb.size()); //the final length of bb is 13, just include the last '\0' character, all middle '\0' characters have been deleted.
	*/
	//by the way, deleting the last character which is not '\0' before appending another string is very efficient, please use it freely.
	basic_buffer& append2(std::initializer_list<char> ilist) {return append2(std::begin(ilist), ilist.size());}
	basic_buffer& append2(const char* buff) {return append(buff, strlen(buff) + 1);} //include the '\0' character
	basic_buffer& append2(const char* _buff, size_t _len)
	{
		if (len > 0 && '\0' == *std::next(buff, len - 1))
			resize(len - 1); //delete the last character if it's '\0'

		return append(_buff, _len);
	}

	basic_buffer& erase(size_t pos = 0, size_t len = UINT_MAX)
	{
		if (pos > size())
			throw std::out_of_range("invalid position");

		if (len >= size())
			resize(pos);
		else
		{
			auto start = size() - len;
			if (pos >= start)
				resize(pos);
			else
			{
				memmove(std::next(buff, pos), std::next(buff, pos + len), size() - pos - len);
				resize(size() - len);
			}
		}

		return *this;
	}

	void resize(size_t _len) //won't fill the extended buffers
	{
		if (_len > cap)
			reserve(_len);

		len = (unsigned) _len;
	}

	void assign(size_t len) {resize(len);}
	void assign(const char* buff, size_t len) {resize(0); append(buff, len);}

	void shrink_to_fit() {reserve(0);}
	void reserve(size_t capacity)
	{
		if (capacity > max_size())
			throw std::length_error("too big memory request");
		else if (capacity < len)
			capacity = len;
		else if (0 == capacity)
			capacity = 16;

		if (capacity != cap)
		{
#ifdef _MSC_VER
			if (cap < capacity && capacity < cap + cap / 2) //memory expansion strategy -- quoted from std::string.
			{
				capacity = cap + cap / 2;
#else
			if (cap < capacity && capacity < 2 * cap) //memory expansion strategy -- quoted from std::string.
			{
				capacity = 2 * cap;
#endif
				if (capacity > max_size())
					capacity = max_size();
			}

			auto new_cap = capacity & (unsigned) (max_size() << 4);
			if (capacity > new_cap)
			{
				new_cap += 0x10;
				if (capacity > new_cap || new_cap > max_size()) //overflow
					new_cap = max_size();
			}

			if (new_cap != cap)
			{
				auto new_buff = (char*) realloc(buff, new_cap);
				if (nullptr == new_buff)
					throw std::bad_alloc();

				cap = (unsigned) new_cap;
				buff = new_buff;
			}
		}
	}

	size_t max_size() const {return UINT_MAX;}
	size_t capacity() const {return cap;}

	//the following five functions are needed by ascs
	void clear() {resize(0);}
	void clean() {free(buff); do_detach();}
	const char* data() const {return buff;}
	size_t size() const {return nullptr == buff ? 0 : len;}
	bool empty() const {return 0 == len || nullptr == buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(cap, other.cap);}

	//functions needed by packer and unpacker
	char* data() {return buff;}

protected:
	void do_attach(char* _buff, size_t _len, size_t capacity) {buff = _buff; len = (unsigned) _len; cap = (unsigned) capacity;}
	void do_detach() {buff = nullptr; len = cap = 0;}

	void check_length(size_t add_len) {if (add_len > max_size() || max_size() - add_len < len) throw std::length_error("too big memory request");}

protected:
	char* buff;
	unsigned len, cap;
};

class cpu_timer //a substitute of boost::timer::cpu_timer
{
public:
	cpu_timer() {restart();}

	void restart() {started = false; elapsed_seconds = .0f; start();}
	void start() {if (started) return; started = true; start_time = std::chrono::system_clock::now();}
	void resume() {start();}
	void stop() {if (!started) return; started = false; elapsed_seconds += std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - start_time).count();}

	bool stopped() const {return !started;}
	float elapsed() const {if (!started) return elapsed_seconds; return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - start_time).count();}

protected:
	bool started;
	float elapsed_seconds;
	std::chrono::system_clock::time_point start_time;
};

inline std::list<std::string> split_string(const std::string& str) //delimiters can only be ' ' or '\t'
{
	std::list<std::string> re;

	auto start_pos = std::string::npos;
	for (std::string::size_type pos = 0; pos < str.size(); ++pos)
	{
		if (' ' == str[pos] || '\t' == str[pos])
		{
			if (std::string::npos != start_pos)
			{
				re.emplace_back(std::next(str.data(), start_pos), pos - start_pos);
				start_pos = std::string::npos;
			}
		}
		else if (std::string::npos == start_pos)
			start_pos = pos;
	}

	if (std::string::npos != start_pos)
		re.emplace_back(std::next(str.data(), start_pos), str.size() - start_pos);

	return re;
}

}} //namespace

#endif /* _ASCS_EXT_H_ */
