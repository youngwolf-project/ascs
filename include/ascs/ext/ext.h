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

#include <chrono>

#include "../base.h"

//the size of the buffer used when receiving msg, must equal to or larger than the biggest msg size,
//the bigger this buffer is, the more msgs can be received in one time if there are enough msgs buffered in the SOCKET.
//for unpackers who use fixed buffer, every unpacker has a fixed buffer with this size, every tcp::socket_base has an unpacker,
//so this size is not the bigger the better, bigger buffers may waste more memory.
//if you customized the packer and unpacker, the above principle maybe not right anymore, it should depends on your implementations.
#ifndef ASCS_MSG_BUFFER_SIZE
#define ASCS_MSG_BUFFER_SIZE	4000
#endif
static_assert(ASCS_MSG_BUFFER_SIZE > 0, "message buffer size must be bigger than zero.");

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

namespace ascs { namespace ext {

//implement i_buffer interface, then string_buffer can be wrapped by replaceable_buffer
class string_buffer : public std::string, public i_buffer
{
public:
	virtual bool empty() const {return std::string::empty();}
	virtual size_t size() const {return std::string::size();}
	virtual const char* data() const {return std::string::data();}
};

class basic_buffer
#if defined(_MSC_VER) && _MSC_VER <= 1800
	: public asio::noncopyable
#endif
{
public:
	basic_buffer() {do_detach();}
	basic_buffer(size_t len) {do_detach(); assign(len);}
	basic_buffer(basic_buffer&& other) {do_attach(other.buff, other.len, other.buff_len); other.do_detach();}
	~basic_buffer() {clear();}

	basic_buffer& operator=(basic_buffer&& other) {clear(); swap(other); return *this;}
	void assign(size_t len) {clear(); do_attach(new char[len], len, len);}

	//the following five functions are needed by ascs
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return nullptr == buff ? 0 : len;}
	const char* data() const {return buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(buff_len, other.buff_len);}
	void clear() {delete[] buff; do_detach();}

	//functions needed by packer and unpacker
	char* data() {return buff;}
	bool size(size_t _len) {assert(_len <= buff_len); return (_len <= buff_len) ? (len = _len, true) : false;}
	size_t buffer_size() const {return nullptr == buff ? 0 : buff_len;}

protected:
	void do_attach(char* _buff, size_t _len, size_t _buff_len) {buff = _buff; len = _len; buff_len = _buff_len;}
	void do_detach() {buff = nullptr; len = buff_len = 0;}

protected:
	char* buff;
	size_t len, buff_len;
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
