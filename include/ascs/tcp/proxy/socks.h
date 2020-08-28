/*
 * socks4.h
 *
 *  Created on: 2020-8-25
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * SOCKS4/5 proxy support (CONNECT only).
 */

#ifndef _ASCS_PROXY_SOCKS_H_
#define _ASCS_PROXY_SOCKS_H_

#include "../client_socket.h"

namespace ascs { namespace tcp { namespace proxy {

namespace socks4 {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = asio::ip::tcp::socket,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class client_socket_base : public ascs::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef ascs::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	client_socket_base(asio::io_context& io_context_) : super(io_context_), req_len(0) {}
	client_socket_base(Matrix& matrix_) : super(matrix_), req_len(0) {}

	virtual const char* type_name() const {return "SOCKS4 (client endpoint)";}
	virtual int type_id() const {return 5;}

	bool set_peer_addr(unsigned short port, const std::string& ip)
	{
		req_len = 0;
		if (!super::set_addr(peer_addr, port, ip) || !peer_addr.address().is_v4())
			return false;

		req[0] = 4;
		req[1] = 1;
		*((unsigned short*) std::next(req, 2)) = htons(peer_addr.port());
		memcpy(std::next(req, 4), peer_addr.address().to_v4().to_bytes().data(), 4);
		memcpy(std::next(req, 8), "ascs", sizeof("ascs"));
		req_len = 8 + sizeof("ascs");

		return true;
	}
	const asio::ip::tcp::endpoint& get_peer_addr() const {return peer_addr;}

private:
	virtual void connect_handler(const asio::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (ec || 0 == req_len)
			return super::connect_handler(ec);

		asio::async_write(this->next_layer(), asio::buffer(req, req_len),
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec || req_len != bytes_transferred)
		{
			unified_out::error_out(ASCS_LLF " socks4 write error", this->id());
			this->force_shutdown(false);
		}
		else
			asio::async_read(this->next_layer(), asio::buffer(res, sizeof(res)),
				this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);}));
	}

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec || sizeof(res) != bytes_transferred)
		{
			unified_out::error_out(ASCS_LLF " socks4 read error", this->id());
			this->force_shutdown(false);
		}
		else if (90 != res[1])
		{
			unified_out::info_out(ASCS_LLF " socks4 server error: %d", this->id(), (int) (unsigned char) res[1]);
			this->force_shutdown(false);
		}
		else
			super::connect_handler(ec);
	}

private:
	char req[16], res[8];
	size_t req_len;

	asio::ip::tcp::endpoint peer_addr;
};

}

namespace socks5 {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = asio::ip::tcp::socket,
	template<typename> class InQueue = ASCS_INPUT_QUEUE, template<typename> class InContainer = ASCS_INPUT_CONTAINER,
	template<typename> class OutQueue = ASCS_OUTPUT_QUEUE, template<typename> class OutContainer = ASCS_OUTPUT_CONTAINER>
class client_socket_base : public ascs::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef ascs::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	client_socket_base(asio::io_context& io_context_) : super(io_context_), req_len(0), res_len(0), step(-1) {}
	client_socket_base(Matrix& matrix_) : super(matrix_), req_len(0), res_len(0), step(-1) {}

	virtual const char* type_name() const {return "SOCKS4 (client endpoint)";}
	virtual int type_id() const {return 5;}

	bool set_peer_addr(unsigned short port, const std::string& ip)
	{
		step = -1;
		if (ip.empty())
			return false;
		else if (!super::set_addr(peer_addr, port, ip))
			peer_domain = ip;
		else if (!peer_addr.address().is_v4() && !peer_addr.address().is_v6())
			return false;

		step = 0;
		return true;
	}
	const asio::ip::tcp::endpoint& get_peer_addr() const {return peer_addr;}

	void set_auth(const std::string& usr, const std::string& pwd) {username = usr, password = pwd;}

private:
	virtual void connect_handler(const asio::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (ec || -1 == step)
			return super::connect_handler(ec);

		step = 0;
		send_method();
	}

	void send_method()
	{
		res_len = 0;

		req[0] = 5;
		if (username.empty() && password.empty())
		{
			req[1] = 1;
			req_len = 3;
		}
		else
		{
			req[1] = 2;
			req[3] = 2;
			req_len = 4;
		}
		req[2] = 0;

		asio::async_write(this->next_layer(), asio::buffer(req, req_len),
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));
	}

	void send_auth()
	{
		res_len = 0;

		req[0] = 1;
		req[1] = (char) std::min(username.size(), (size_t) 8);
		memcpy(std::next(req, 2), username.data(), (size_t) req[1]);
		req[2 + req[1]] = (char) std::min(password.size(), (size_t) 8);
		memcpy(std::next(req, 3 + req[1]), password.data(), (size_t) req[2 + req[1]]);
		req_len = 1 + 1 + req[1] + 1 + req[2 + req[1]];

		asio::async_write(this->next_layer(), asio::buffer(req, req_len),
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));
	}

	void send_request()
	{
		res_len = 0;

		req[0] = 5;
		req[1] = 1;
		req[2] = 0;
		if (!peer_domain.empty())
		{
			req[3] = 3;
			req[4] = (char) std::min(peer_domain.size(), sizeof(req) - 7);
			memcpy(std::next(req, 5), peer_domain.data(), (size_t) req[4]);
			*((unsigned short*) std::next(req, 5 + req[4])) = htons(peer_port);
			req_len = 7 + req[4];
		}
		else if (peer_addr.address().is_v4())
		{
			req[3] = 1;
			memcpy(std::next(req, 4), peer_addr.address().to_v4().to_bytes().data(), 4);
			*((unsigned short*) std::next(req, 8)) = htons(peer_addr.port());
			req_len = 10;
		}
		else //ipv6
		{
			req[3] = 4;
			memcpy(std::next(req, 4), peer_addr.address().to_v6().to_bytes().data(), 16);
			*((unsigned short*) std::next(req, 20)) = htons(peer_addr.port());
			req_len = 22;
		}

		asio::async_write(this->next_layer(), asio::buffer(req, req_len),
			this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));
	}

	void send_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		if (ec || req_len != bytes_transferred)
		{
			unified_out::error_out(ASCS_LLF " socks5 write error", this->id());
			this->force_shutdown(false);
		}
		else
		{
			++step;
			this->next_layer().async_read_some(asio::buffer(res, sizeof(res)),
				this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);}));
		}
	}

	void recv_handler(const asio::error_code& ec, size_t bytes_transferred)
	{
		res_len += bytes_transferred;
		if (ec)
		{
			unified_out::error_out(ASCS_LLF " socks5 write error", this->id());
			this->force_shutdown(false);
		}
		else
		{
			auto succ = true;
			auto continue_read = false;
			if (1 == step) //parse method
			{
				if (res_len < 2)
					continue_read = true;
				else if (res_len > 2)
				{
					unified_out::info_out(ASCS_LLF " socks5 server error", this->id());
					succ = false;
				}
				else if (0 == res[1])
				{
					++step; //skip auth step
					return send_request();
				}
				else if (2 == res[1])
					return send_auth();
				else
				{
					unified_out::error_out(ASCS_LLF " unsupported socks5 auth %d", this->id(), (int) (unsigned char) res[1]);
					succ = false;
				}
			}
			else if (2 == step) //parse auth
			{
				if (res_len < 2)
					continue_read = true;
				else if (res_len > 2)
				{
					unified_out::info_out(ASCS_LLF " socks5 server error", this->id());
					succ = false;
				}
				else if (0 == res[1])
					return send_request();
				else
				{
					unified_out::error_out(ASCS_LLF " socks5 auth error", this->id());
					succ = false;
				}
			}
			else if (3 == step) //parse request
			{
				size_t len = 6;
				if (res_len < len)
					continue_read = true;
				else if (0 == res[1])
				{
					if (1 == res[3])
						len += 4;
					else if (3 == res[3])
						len += 1 + res[4];
					else if (4 == res[3])
						len += 16;

					if (res_len < len)
						continue_read = true;
					else if (res_len > len)
					{
						unified_out::info_out(ASCS_LLF " socks5 server error", this->id());
						succ = false;
					}
				}
				else
				{
					unified_out::info_out(ASCS_LLF " socks5 server error", this->id());
					succ = false;
				}
			}
			else
			{
				unified_out::info_out(ASCS_LLF " socks5 client error", this->id());
				succ = false;
			}

			if (!succ)
				this->force_shutdown(false);
			else if (continue_read)
				this->next_layer().async_read_some(asio::buffer(res, sizeof(res) + res_len),
					this->make_handler_error_size([this](const asio::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);}));
			else
				super::connect_handler(ec);
		}
	}

private:
	char req[24], res[24];
	size_t req_len, res_len;
	int step;

	asio::ip::tcp::endpoint peer_addr;
	std::string peer_domain;
	unsigned short peer_port;
	std::string username, password;
};

}

}}} //namespace

#endif /* _ASCS_PROXY_SOCKS_H_ */
