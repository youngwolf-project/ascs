/*
 * reliable_socket.h
 *
 *  Created on: 2021-4-6
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * UDP reliable socket
 */

#ifndef _ASCS_UDP_RELIABLE_SOCKET_H_
#define _ASCS_UDP_RELIABLE_SOCKET_H_

#include <unordered_map>

#include "../socket.h"

namespace ascs { namespace udp {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class generic_reliable_socket : public socket4<asio::ip::udp::socket, asio::ip::udp, Packer, Unpacker, udp_msg, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef udp_msg<typename Packer::msg_type, asio::ip::udp> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type, asio::ip::udp> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

	enum DataType {RESET, SYNC, NORMAL};

private:
	typedef socket4<asio::ip::udp::socket, asio::ip::udp, Packer, Unpacker, udp_msg, InQueue, InContainer, OutQueue, OutContainer> super;

	class sync_send_buffers
	{
	private:
		class sync_send_buffer
		{
		private:
			struct sent_msg
			{
				in_msg_type msg;
				std::chrono::system_clock::time_point send_time;
			};

		public:
			sync_send_buffer() : cur_index(0) {}

			size_t reset() {cur_index = 0; auto size = can.size(); can.clear(); return size;}
			unsigned short gen_index() {return cur_index++;}

			bool emplace_1(in_msg_ctype& msg)
			{
				assert(msg.size() >= 1 + sizeof(unsigned short));
				if (msg.size() < 1 + sizeof(unsigned short))
					return false;

				unsigned short index;
				memcpy(&index, std::next(msg.data(), 1), sizeof(unsigned short));
				can[ntohs(index)].send_time = std::chrono::system_clock::now();

				return true;
			}

			void emplace_2(in_msg_type& msg)
			{
				assert(msg.size() >= 1 + sizeof(unsigned short));

				unsigned short index;
				memcpy(&index, std::next(msg.data(), 1), sizeof(unsigned short));
				auto iter = can.find(ntohs(index));
				if (iter != std::end(can))
					iter->msg.swap(msg);
			}

			size_t sync_msg(out_msg_ctype& msg)
			{
				size_t n = 0;
				if (!msg.empty() && DataType::SYNC == *msg.data())
					for (size_t i = 1; i + 1 < msg.size(); i += 2)
					{
						unsigned short index;
						memcpy(&index, std::next(msg.data(), i), sizeof(unsigned short));
						n += can.erase(ntohs(index));
					}

				return n;
			}

			size_t pick_msg(std::list<in_msg_type>& _can, unsigned interval) //unit is milliseconds
			{
				size_t n = 0;
				auto now = std::chrono::system_clock::now();

				for (auto iter = std::begin(can); iter != std::end(can);)
				{
					auto milli_escaped = std::chrono::duration_cast<std::chrono::milliseconds>(now - iter->second.send_time).count();
					if (milli_escaped >= (int_fast64_t) interval)
					{
						if (!iter->second.msg.empty())
							_can.emplace_back(std::move(iter->second.msg));

						iter = can.erase(iter);
						++n;
					}
					else
						++iter;
				}

				return n;
			}

		private:
			unsigned short cur_index;
			std::unordered_map<unsigned short, sent_msg> can;
		};

	public:
		sync_send_buffers() : cap(ASCS_UDP_MAX_SYNC_BUF), size_(0) {}

		void capacity(size_t _cap) {cap = _cap;}
		size_t capacity() const {return cap;}
		size_t size() const {return size_;}
		bool available() const {return size_ < cap;}

		void clear() {std::lock_guard<std::mutex> lock(mutex); size_ = 0; can.clear();}
		void reset(const std::string& ip_port) {std::lock_guard<std::mutex> lock(mutex); size_ -= can[ip_port].reset();}
		void gen_index(const std::string& ip_port) {std::lock_guard<std::mutex> lock(mutex); can[ip_port].gen_index();}
		void emplace_1(const std::string& ip_port, in_msg_ctype& msg) {std::lock_guard<std::mutex> lock(mutex); if (can[ip_port].emplace_1(msg)) ++size_;}
		void emplace_2(const std::string& ip_port, in_msg_type& msg) {std::lock_guard<std::mutex> lock(mutex); can[ip_port].emplace_2(msg);}
		void sync_msg(const std::string& ip_port, out_msg_ctype& msg) {std::lock_guard<std::mutex> lock(mutex); size_ -= can[ip_port].sync_msg(msg);}
		void pick_msg(std::list<in_msg_type>& _can, unsigned interval) //unit is milliseconds
			{ascs::do_something_to_all(can, mutex, [&](sync_send_buffer& item) {this->size_ -= item.pick_msg(_can, interval);});}

	private:
		size_t cap, size_;
		std::unordered_map<std::string, sync_send_buffer> can; //ip::port -> sync_send_buffer
		std::mutex mutex;
	};

	class sync_recv_buffers
	{
	private:
		class sync_recv_buffer
		{
		public:
			sync_recv_buffer() : next_index(0) {}

			size_t reset() {next_index = 0; auto size = can.size(); can.clear(); return size;}
			bool valid(out_msg_ctype& msg, size_t cap) const
			{
				assert(msg.size() >= 1 + sizeof(unsigned short));
				if (msg.size() < 1 + sizeof(unsigned short))
					return false;

				unsigned short index;
				memcpy(&index, std::next(msg.data(), 1), sizeof(unsigned short));
				index = ntohs(index);

				auto gap = 1;
				if (index >= next_index)
					gap += index - next_index;
				else
				{
					gap += 65535 - next_index;
					gap += index + 1;
				}

				return (unsigned) gap * 2 <= cap;
			}

			size_t emplace(out_msg_type& msg, std::list<out_msg_type>& _can)
			{
				unsigned short index;
				memcpy(&index, std::next(msg.data(), 1), sizeof(unsigned short));
				index = ntohs(index);

				size_t n = 0;
				if (index != next_index)
					can[index].swap(msg);
				else
				{
					_can.emplace_back(std::move(msg));
					while ((++n, true))
					{
						auto iter = can.find(++next_index);
						if (iter == std::end(can))
							break;

						_can.emplace_back(std::move(iter->second));
						can.erase(iter);
					}
				}

				return n;
			}

		private:
			unsigned short next_index;
			std::unordered_map<unsigned short, out_msg_type> can;
		};

	public:
		sync_recv_buffers() : cap(ASCS_UDP_MAX_SYNC_BUF), size_(0) {}

		void capacity(size_t _cap) {cap = _cap;}
		size_t capacity() const {return cap;}
		size_t size() const {return size_;}
		bool available() const {return size_ < cap;}

		void clear() {size_ = 0; can.clear();}
		void reset(const std::string& ip_port) {size_ -= can[ip_port].reset();}
		bool emplace(const std::string& ip_port, out_msg_type& msg, std::list<out_msg_type>& _can)
		{
			auto& item = can[ip_port];
			if (!item.valid(msg, cap))
				return false;

			++size_;
			size_ -= item.emplace(msg, _can);

			return true;
		}

	private:
		size_t cap, size_;
		std::unordered_map<std::string, sync_recv_buffer> can; //ip::port -> sync_recv_buffer
	};

public:
	static bool set_addr(asio::ip::udp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = asio::ip::udp::endpoint(ASCS_UDP_DEFAULT_IP_VERSION, port);
		else
		{
			asio::error_code ec;
#if ASIO_VERSION >= 101100
			auto addr = asio::ip::make_address(ip, ec); assert(!ec);
#else
			auto addr = asio::ip::address::from_string(ip, ec); assert(!ec);
#endif
			if (ec)
			{
				unified_out::error_out("invalid IP address %s.", ip.data());
				return false;
			}

			endpoint = asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}

protected:
	generic_reliable_socket(asio::io_context& io_context_) : super(io_context_), is_bound(false), is_connected(false), connect_mode(ASCS_UDP_CONNECT_MODE), matrix(nullptr) {}
	generic_reliable_socket(Matrix& matrix_) : super(matrix_.get_service_pump()), is_bound(false), is_connected(false), connect_mode(ASCS_UDP_CONNECT_MODE), matrix(&matrix_) {}

public:
	virtual bool is_ready() {return is_bound;}
	virtual void send_heartbeat()
	{
		in_msg_type msg(peer_addr, this->packer()->pack_heartbeat());
		do_direct_send_msg(std::move(msg));
	}
	virtual const char* type_name() const {return "UDP";}
	virtual int type_id() const {return 0;}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function, so if you want to do some additional initialization
	// for this socket, do it at here and in the constructor.
	//for udp::single_service_base, this virtual function will never be called, please note.
	virtual void reset()
	{
		is_connected = is_bound = false;

		sending_msg.clear();
		super::reset();
	}

	bool connected() const {return is_connected;}
	void set_connect_mode() {connect_mode = true;}
	bool get_connect_mode() const {return connect_mode;}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(local_addr, port, ip);}
	bool set_local_addr(const std::string& file_name) {local_addr = asio::ip::udp::endpoint(file_name); return true;}
	const asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}

	bool set_peer_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(peer_addr, port, ip);}
	bool set_peer_addr(const std::string& file_name) {peer_addr = asio::ip::udp::endpoint(file_name); return true;}
	const asio::ip::udp::endpoint& get_peer_addr() const {return peer_addr;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {show_info("link:", "been shutting down."); this->dispatch_strand(rw_strand, [this]() {this->close(true);});}
	void graceful_shutdown() {force_shutdown();}

	std::string endpoint_to_string(const asio::ip::udp::endpoint& ep) const {return ep.address().to_string() + ':' + std::to_string(ep.port());}
#ifdef ASIO_HAS_LOCAL_SOCKETS
	std::string endpoint_to_string(const asio::local::datagram_protocol::endpoint& ep) const {return ep.path();}
#endif

	void show_info(const char* head = nullptr, const char* tail = nullptr) const
	{
		unified_out::info_out(ASCS_LLF " %s %s %s",
			this->id(), nullptr == head ? "" : head, endpoint_to_string(local_addr).data(), nullptr == tail ? "" : tail);
	}

	void show_status() const
	{
		std::stringstream s;

		if (stat.last_send_time > 0)
		{
			s << "\n\tlast send time: ";
			log_formater::to_time_str(stat.last_send_time, s);
		}

		if (stat.last_recv_time > 0)
		{
			s << "\n\tlast recv time: ";
			log_formater::to_time_str(stat.last_recv_time, s);
		}

		unified_out::info_out(
			"\n\tid: " ASCS_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ASCS_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\trecv suspended: %d"
			"\n\tsend buffer usage: %.2f%%"
			"\n\trecv buffer usage: %.2f%%"
			"%s",
			this->id(), this->started(), this->is_sending(),
#ifdef ASCS_PASSIVE_RECV
			this->is_reading(),
#endif
			this->is_dispatching(), this->is_recv_idle(),
			this->send_buf_usage() * 100.f, this->recv_buf_usage() * 100.f, s.str().data());
	}

	///////////////////////////////////////////////////
	typedef asio::ip::udp Family;
	//msg sending interface
	//if the message already packed, do call direct_send_msg or direct_sync_send_msg to reduce unnecessary memory replication, if you will not
	// use it any more, use std::move to wrap it when calling direct_send_msg or direct_sync_send_msg.
	UDP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::generic_reliable_socket's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ASCS_SYNC_SEND
	UDP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::generic_reliable_socket's send buffer
	UDP_SYNC_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	UDP_SYNC_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
#ifdef ASCS_EXPOSE_SEND_INTERFACE
	using super::send_msg;
#endif
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	Matrix* get_matrix() {return matrix;}
	const Matrix* get_matrix() const {return matrix;}

	virtual bool bind(const asio::ip::udp::endpoint& local_addr) {return true;}
	virtual bool connect(const asio::ip::udp::endpoint& peer_addr) {return false;}

	virtual bool do_start()
	{
		auto& lowest_object = this->lowest_layer();
		if (!lowest_object.is_open()) //user maybe has opened this socket (to set options for example)
		{
			asio::error_code ec;
			lowest_object.open(local_addr.protocol(), ec); assert(!ec);
			if (ec)
			{
				unified_out::error_out("cannot create socket: %s", ec.message().data());
				return (is_bound = false);
			}

#ifndef ASCS_NOT_REUSE_ADDRESS
			lowest_object.set_option(asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		}

		if (!bind(local_addr))
			return (is_bound = false);
		else if (connect_mode)
			is_connected = connect(peer_addr);

		return (is_bound = true) && super::do_start();
	}

	//msg was failed to send and udp::generic_reliable_socket will not hold it any more, if you want to re-send it in the future,
	// you must take over it and re-send (at any time) it via direct_send_msg.
	//DO NOT hold msg for further usage, just swap its content with your own message in this virtual function.
	virtual void on_send_error(const asio::error_code& ec, typename super::in_msg& msg)
		{unified_out::error_out(ASCS_LLF " send msg error (%d %s)", this->id(), ec.value(), ec.message().data());}

	virtual void on_recv_error(const asio::error_code& ec)
	{
		if (asio::error::operation_aborted != ec)
			unified_out::error_out(ASCS_LLF " recv msg error (%d %s)", this->id(), ec.value(), ec.message().data());
#ifndef ASCS_CLEAR_OBJECT_INTERVAL
		else if (nullptr != matrix)
			matrix->del_socket(this->id());
#endif
	}

	virtual bool on_heartbeat_error()
	{
		stat.last_recv_time = time(nullptr); //avoid repetitive warnings
		unified_out::warning_out(ASCS_LLF " %s is not available", this->id(), endpoint_to_string(peer_addr).data());
		return true;
	}

#ifdef ASCS_SYNC_SEND
	virtual void on_close() {if (sending_msg.p) sending_msg.p->set_value(sync_call_result::NOT_APPLICABLE); super::on_close();}
#endif

private:
	using super::close;
	using super::handle_error;
	using super::handle_msg;
	using super::do_direct_send_msg;
#ifdef ASCS_SYNC_SEND
	using super::do_direct_sync_send_msg;
#endif

	virtual void do_recv_msg()
	{
#ifdef ASCS_PASSIVE_RECV
		if (reading)
			return;
#endif
		auto recv_buff = this->unpacker()->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);
		if (0 == asio::buffer_size(recv_buff))
			unified_out::error_out(ASCS_LLF " the unpacker returned an empty buffer, quit receiving!", this->id());
		else
		{
#ifdef ASCS_PASSIVE_RECV
			reading = true;
#endif
			if (is_connected)
				this->next_layer().async_receive(recv_buff, make_strand_handler(rw_strand,
					this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);})));
			else
				this->next_layer().async_receive_from(recv_buff, temp_addr, make_strand_handler(rw_strand,
					this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);})));
		}
	}

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			stat.last_recv_time = time(nullptr);

			typename Unpacker::container_type msg_can;
			this->unpacker()->parse_msg(bytes_transferred, msg_can);

#ifdef ASCS_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
			ascs::do_something_to_all(msg_can, [this](typename Unpacker::msg_type& msg) {
				this->temp_msg_can.emplace_back(this->is_connected ? this->peer_addr : this->temp_addr, std::move(msg));
			});
			if (handle_msg()) //if macro ASCS_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
		else
		{
#ifdef ASCS_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__)
			if (ec && asio::error::connection_refused != ec && asio::error::connection_reset != ec)
#else
			if (ec)
#endif
			{
				handle_error();
				on_recv_error(ec);
			}
			else if (handle_msg()) //if macro ASCS_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
	}

	virtual bool do_send_msg(bool in_strand = false)
	{
		if (!in_strand && sending)
			return true;

		if (send_buffer.try_dequeue(sending_msg))
		{
			sending = true;
			stat.send_delay_sum += statistic::now() - sending_msg.begin_time;
			sending_msg.restart();
			if (is_connected)
				this->next_layer().async_send(asio::buffer(sending_msg.data(), sending_msg.size()), make_strand_handler(rw_strand,
					this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);})));
			else
				this->next_layer().async_send_to(asio::buffer(sending_msg.data(), sending_msg.size()), sending_msg.peer_addr, make_strand_handler(rw_strand,
					this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);})));
			return true;
		}

		sending = false;
		return false;
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			stat.last_send_time = time(nullptr);

			stat.send_byte_sum += bytes_transferred;
			stat.send_time_sum += statistic::now() - sending_msg.begin_time;
			++stat.send_msg_sum;
#ifdef ASCS_SYNC_SEND
			if (sending_msg.p)
				sending_msg.p->set_value(sync_call_result::SUCCESS);
#endif
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
			this->on_msg_send(sending_msg);
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
			if (send_buffer.empty())
				this->on_all_msg_send(sending_msg);
#endif
		}
		else
		{
#ifdef ASCS_SYNC_SEND
			if (sending_msg.p)
				sending_msg.p->set_value(sync_call_result::NOT_APPLICABLE);
#endif
			on_send_error(ec, sending_msg);
		}
		sending_msg.clear(); //clear sending message after on_send_error, then user can decide how to deal with it in on_send_error

		if (ec && (asio::error::not_socket == ec || asio::error::bad_descriptor == ec))
			return;

		//send msg in sequence
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequent sending.
		if (!do_send_msg(true) && !send_buffer.empty())
			do_send_msg(true); //just make sure no pending msgs
	}

private:
	using super::stat;
	using super::temp_msg_can;

	using super::send_buffer;
	using super::sending;

#ifdef ASCS_PASSIVE_RECV
	using super::reading;
#endif
	using super::rw_strand;

	bool is_bound, is_connected, connect_mode;
	typename super::in_msg sending_msg;
	asio::ip::udp::endpoint local_addr;
	asio::ip::udp::endpoint temp_addr; //used when receiving messages
	asio::ip::udp::endpoint peer_addr;
	sync_send_buffers sync_send_buff;
	sync_recv_buffers sync_recv_buff;

	Matrix* matrix;
};

template <typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class reliable_socket_base : public generic_reliable_socket<Packer, Unpacker, Matrix, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef generic_reliable_socket<Packer, Unpacker, Matrix, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	reliable_socket_base(asio::io_context& io_context_) : super(io_context_) {}
	reliable_socket_base(Matrix& matrix_) : super(matrix_) {}

protected:
	virtual bool bind(const asio::ip::udp::endpoint& local_addr)
	{
		if (0 != local_addr.port() || !local_addr.address().is_unspecified())
		{
			asio::error_code ec;
			this->lowest_layer().bind(local_addr, ec);
			if (ec && asio::error::invalid_argument != ec)
			{
				unified_out::error_out("cannot bind socket: %s", ec.message().data());
				return false;
			}
		}

		return true;
	}

	virtual bool connect(const asio::ip::udp::endpoint& peer_addr)
	{
		if (0 != peer_addr.port() || !peer_addr.address().is_unspecified())
		{
			asio::error_code ec;
			this->lowest_layer().connect(peer_addr, ec);
			if (ec)
				unified_out::error_out("cannot connect to the peer");
			else
				return true;
		}
		else
			unified_out::error_out("invalid peer ip address");

		return false;
	}
};

}} //namespace

#endif /* _ASCS_UDP_RELIABLE_SOCKET_H_ */
