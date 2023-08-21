#ifndef _SERVER_H_
#define _SERVER_H_

class my_server_socket : public ext::tcp::server_socket
{
public:
	my_server_socket(i_server& server_) : ext::tcp::server_socket(server_)
	{
#if 3 == PACKER_UNPACKER_TYPE
		std::dynamic_pointer_cast<ASCS_DEFAULT_PACKER>(packer())->prefix_suffix("", "\n");
		std::dynamic_pointer_cast<ASCS_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("", "\n");
#endif
	}

protected:
	//msg handling
#if 1 == PACKER_UNPACKER_TYPE
	//unpacker2 uses unique_buffer or shared_buffer as its message type
	virtual bool on_msg_handle(out_msg_type& msg)
	{
		auto raw_msg = new string_buffer();
		raw_msg->assign(" (from the server)");

		return send_msg(std::move(msg), ASCS_DEFAULT_PACKER::msg_type(raw_msg)); //new feature introduced in 1.4.0
	}
#elif 2 == PACKER_UNPACKER_TYPE
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(std::move(msg));}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(std::move(msg), std::string(" (from the server)"));} //new feature introduced in 1.4.0
#endif
	//msg handling end
};
typedef server_base<my_server_socket> my_server;

#endif //#define _SERVER_H_
