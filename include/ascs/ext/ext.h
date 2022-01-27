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
	explicit basic_buffer(size_t len) {do_detach(); resize(len);}
	basic_buffer(size_t count, char c) {do_detach(); append(count, c);}
	basic_buffer(const char* buff) {do_detach(); operator+=(buff);}
	basic_buffer(const char* buff, size_t len) {do_detach(); append(buff, len);}
	basic_buffer(std::initializer_list<char> ilist) {do_detach(); operator+=(ilist);}
	basic_buffer(const basic_buffer& other) {do_detach(); append(other.buff, other.len);}
	basic_buffer(basic_buffer&& other) {do_attach(other.buff, other.len, other.cap); other.do_detach();}
	virtual ~basic_buffer() {clean();}

	inline basic_buffer& operator=(basic_buffer&& other) {clean(); swap(other); return *this;}
	inline basic_buffer& operator=(const basic_buffer& other) {resize(0); return operator+=(other);}
	inline basic_buffer& operator=(char c) {resize(0); return operator+=(c);}
	inline basic_buffer& operator=(const char* buff) {resize(0); return operator+=(buff);}
	inline basic_buffer& operator=(std::initializer_list<char> ilist) {resize(0); return operator+=(ilist);}

	inline basic_buffer& operator+=(const basic_buffer& other) {return append(other);}
	inline basic_buffer& operator+=(char c) {return append(c);}
	inline basic_buffer& operator+=(const char* buff) {return append(buff);}
	inline basic_buffer& operator+=(std::initializer_list<char> ilist) {return append(ilist);}

	basic_buffer& append(char c) {return append(1, c);}
	basic_buffer& append(size_t count, char c)
	{
		if (count > 0)
		{
			reserve(len + count);
			memset(std::next(buff, len), c, count);
			len += (unsigned) count;
		}

		return *this;
	}

	basic_buffer& append(std::initializer_list<char> ilist) {return append(std::begin(ilist), ilist.size());}
	basic_buffer& append(const basic_buffer& other) {return append(other.data(), other.size());}
	basic_buffer& append(const char* buff) {return append(buff, strlen(buff));}
	basic_buffer& append(const char* _buff, size_t _len)
	{
		if (nullptr != _buff && _len > 0)
		{
			reserve(len + _len);
			memcpy(std::next(buff, len), _buff, _len);
			len += (unsigned) _len;
		}

		return *this;
	}

	void resize(size_t _len) //won't fill the extended buffers
	{
		reserve(_len);
		len = (unsigned) _len;
	}

	void assign(size_t len) {resize(len);}
	void assign(const char* buff, size_t len) {resize(0); append(buff, len);}

	void reserve(size_t capacity)
	{
		if (capacity > cap)
		{
			cap = (unsigned) capacity & 0xFFFFFFF0;
			if ((unsigned) capacity > cap)
				cap <<= 1;
			//memory expansion strategy -- double memory reservation each time as std::string does.
			//not considered integer overflow, please note.

			if (cap < 16)
				cap = 16;
			buff = (char*) realloc(buff, cap);
		}
	}

	size_t max_size() const {return (unsigned) -1;}
	size_t capacity() const {return cap;}

	//the following five functions are needed by ascs
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return nullptr == buff ? 0 : len;}
	const char* data() const {return buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(cap, other.cap);}
	void clear() {resize(0);}
	void clean() {free(buff); do_detach();}

	//functions needed by packer and unpacker
	char* data() {return buff;}

protected:
	void do_attach(char* _buff, size_t _len, size_t capacity) {buff = _buff; len = (unsigned) _len; cap = (unsigned) capacity;}
	void do_detach() {buff = nullptr; len = cap = 0;}

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
