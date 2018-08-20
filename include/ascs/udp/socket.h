/*
 * socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * UDP socket
 */

#ifndef _ASCS_UDP_SOCKET_H_
#define _ASCS_UDP_SOCKET_H_

#include "../socket.h"
#include "../container.h"

namespace ascs { namespace udp {

template <typename Packer, typename Unpacker, typename Socket = asio::ip::udp::socket,
	template<typename, typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class socket_base : public socket<Socket, Packer, Unpacker, udp_msg<typename Packer::msg_type>, udp_msg<typename Unpacker::msg_type>, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef udp_msg<typename Packer::msg_type> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

private:
	typedef socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	socket_base(asio::io_context& io_context_) : super(io_context_), unpacker_(std::make_shared<Unpacker>()), strand(io_context_) {}

	virtual bool is_ready() {return this->lowest_layer().is_open();}
	virtual void send_heartbeat()
	{
		in_msg_type msg(peer_addr, this->packer_->pack_heartbeat());
		this->do_direct_send_msg(std::move(msg));
	}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function
	virtual void reset()
	{
		asio::error_code ec;
		if (!this->lowest_layer().is_open()) {this->lowest_layer().open(local_addr.protocol(), ec); assert(!ec);} //user maybe has opened this socket (to set options for example)
#ifndef ASCS_NOT_REUSE_ADDRESS
		this->lowest_layer().set_option(asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		this->lowest_layer().bind(local_addr, ec); assert(!ec);
		if (ec)
			unified_out::error_out("bind failed.");

		last_send_msg.clear();
		unpacker_->reset();
		super::reset();
	}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(local_addr, port, ip);}
	const asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}
	bool set_peer_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(peer_addr, port, ip);}
	const asio::ip::udp::endpoint& get_peer_addr() const {return peer_addr;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {show_info("link:", "been shutting down."); this->dispatch_strand(strand, [this]() {this->shutdown();});}
	void graceful_shutdown() {force_shutdown();}

	void show_info(const char* head, const char* tail) const {unified_out::info_out("%s %s:%hu %s", head, local_addr.address().to_string().data(), local_addr.port(), tail);}

	void show_status() const
	{
		unified_out::info_out(
			"\n\tid: " ASCS_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ASCS_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\trecv suspended: %d",
			this->id(), this->started(), this->is_sending(),
#ifdef ASCS_PASSIVE_RECV
			this->is_reading(),
#endif
			this->is_dispatching(), this->is_recv_idle());
	}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	std::shared_ptr<i_unpacker<typename Unpacker::msg_type>> unpacker() {return unpacker_;}
	std::shared_ptr<const i_unpacker<typename Unpacker::msg_type>> unpacker() const {return unpacker_;}
#ifdef ASCS_PASSIVE_RECV
	//changing unpacker must before calling ascs::socket::recv_msg, and define ASCS_PASSIVE_RECV macro.
	void unpacker(const std::shared_ptr<i_unpacker<typename Unpacker::msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}
	virtual void recv_msg() {if (!this->reading && is_ready()) this->dispatch_strand(strand, [this]() {this->do_recv_msg();});}

	bool sync_recv_msg(typename Unpacker::msg_type& msg)
	{
		auto recv_buff = unpacker_->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);
		if (0 == asio::buffer_size(recv_buff))
			unified_out::error_out("The unpacker returned an empty buffer, quit receiving!");
		else
		{
			asio::error_code ec;
			auto bytes_transferred = this->next_layer().receive_from(recv_buff, temp_addr, 0, ec);

#ifdef _MSC_VER
			if (ec && asio::error::connection_refused != ec && asio::error::connection_reset != ec)
#else
			if (ec)
#endif
				on_recv_error(ec);
			else
			{
				if (bytes_transferred > 0)
				{
					this->stat.last_recv_time = time(nullptr);
					msg = in_msg_type(temp_addr, unpacker_->parse_msg(bytes_transferred));
				}

				return true;
			}
		}

		return false;
	}
#endif

	///////////////////////////////////////////////////
	//msg sending interface
	UDP_SEND_MSG(send_msg, false, do_direct_send_msg) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true, do_direct_send_msg) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::socket_base's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ASCS_SYNC_SEND
	UDP_SEND_MSG(sync_send_msg, false, do_direct_sync_send_msg) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(sync_send_native_msg, true, do_direct_sync_send_msg) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	UDP_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	UDP_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	virtual void on_recv_error(const asio::error_code& ec)
	{
		if (asio::error::operation_aborted != ec)
			unified_out::error_out("recv msg error (%d %s)", ec.value(), ec.message().data());
	}

	virtual bool on_heartbeat_error()
	{
		this->stat.last_recv_time = time(nullptr); //avoid repetitive warnings
		unified_out::warning_out("%s:%hu is not available", peer_addr.address().to_string().data(), peer_addr.port());
		return true;
	}

private:
#ifndef ASCS_PASSIVE_RECV
	virtual void recv_msg() {this->dispatch_strand(strand, [this]() {this->do_recv_msg();});}
#endif
	virtual void send_msg() {this->dispatch_strand(strand, [this]() {this->do_send_msg(false);});}

	void shutdown()
	{
		this->stop_all_timer();
		this->close();

		if (this->lowest_layer().is_open())
		{
			asio::error_code ec;
			this->lowest_layer().shutdown(asio::ip::udp::socket::shutdown_both, ec);
			this->lowest_layer().close(ec);
		}
	}

	void do_recv_msg()
	{
#ifdef ASCS_PASSIVE_RECV
		if (this->reading)
			return;
#endif
		auto recv_buff = unpacker_->prepare_next_recv();
		assert(asio::buffer_size(recv_buff) > 0);
		if (0 == asio::buffer_size(recv_buff))
			unified_out::error_out("The unpacker returned an empty buffer, quit receiving!");
		else
		{
#ifdef ASCS_PASSIVE_RECV
			this->reading = true;
#endif
			this->next_layer().async_receive_from(recv_buff, temp_addr, make_strand_handler(strand,
				this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);})));
		}
	}

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			this->stat.last_recv_time = time(nullptr);
			in_msg_type temp_msg(temp_addr, unpacker_->parse_msg(bytes_transferred));

#ifdef ASCS_PASSIVE_RECV
			this->reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
			if (this->handle_msg(std::move(temp_msg))) //if macro ASCS_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
		else
		{
#ifdef ASCS_PASSIVE_RECV
			this->reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
#ifdef _MSC_VER
			if (ec && asio::error::connection_refused != ec && asio::error::connection_reset != ec)
#else
			if (ec)
#endif
				on_recv_error(ec);
#ifndef ASCS_PASSIVE_RECV
			else
				do_recv_msg(); //receive msg in sequence
#endif
		}
	}

	bool do_send_msg(bool in_strand)
	{
		if (!in_strand && this->sending)
			return true;

		if ((this->sending = this->send_msg_buffer.try_dequeue(last_send_msg)))
		{
			this->stat.send_delay_sum += statistic::now() - last_send_msg.begin_time;

			last_send_msg.restart();
			this->next_layer().async_send_to(asio::buffer(last_send_msg.data(), last_send_msg.size()), last_send_msg.peer_addr, make_strand_handler(strand,
				this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);})));
			return true;
		}

		return false;
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			this->stat.last_send_time = time(nullptr);

			this->stat.send_byte_sum += bytes_transferred;
			this->stat.send_time_sum += statistic::now() - last_send_msg.begin_time;
			++this->stat.send_msg_sum;
#ifdef ASCS_SYNC_SEND
			if (last_send_msg.cv) last_send_msg.cv->notify_one();
#endif
#ifdef ASCS_WANT_MSG_SEND_NOTIFY
			this->on_msg_send(last_send_msg);
#endif
#ifdef ASCS_WANT_ALL_MSG_SEND_NOTIFY
			if (this->send_msg_buffer.empty())
				this->on_all_msg_send(last_send_msg);
#endif
		}
		else
			this->on_send_error(ec);
		last_send_msg.clear(); //clear sending message after on_send_error, then user can decide how to deal with it in on_send_error

		if (ec && (asio::error::not_socket == ec || asio::error::bad_descriptor == ec))
			return;

		//send msg in sequence
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequent sendings.
		if (!do_send_msg(true) && !this->send_msg_buffer.empty())
			do_send_msg(true); //just make sure no pending msgs
	}

	bool set_addr(asio::ip::udp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = asio::ip::udp::endpoint(ASCS_UDP_DEFAULT_IP_VERSION, port);
		else
		{
			asio::error_code ec;
#if ASIO_VERSION >= 101100
			auto addr = asio::ip::make_address(ip, ec);
#else
			auto addr = asio::ip::address::from_string(ip, ec);
#endif
			if (ec)
				return false;

			endpoint = asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}

protected:
	typename super::in_msg last_send_msg;
	std::shared_ptr<i_unpacker<typename Unpacker::msg_type>> unpacker_;
	asio::ip::udp::endpoint local_addr;
	asio::ip::udp::endpoint temp_addr; //used when receiving messages
	asio::ip::udp::endpoint peer_addr;

private:
	asio::io_context::strand strand;
};

}} //namespace

#endif /* _ASCS_UDP_SOCKET_H_ */
