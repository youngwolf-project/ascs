#ifndef _SERVER_H_
#define _SERVER_H_

class my_server_socket : public server_socket
{
public:
	my_server_socket(i_server& server_) : server_socket(server_)
	{
#if 3 == PACKER_UNPACKER_TYPE
		std::dynamic_pointer_cast<ASCS_DEFAULT_PACKER>(packer())->prefix_suffix("", "\n");
		std::dynamic_pointer_cast<ASCS_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("", "\n");
#endif
	}

protected:
	//msg handling
#if 1 == PACKER_UNPACKER_TYPE || 2 == PACKER_UNPACKER_TYPE
	//replaceable_unpacker uses auto_buffer or shared_buffer as its message type, they don't support + operation,
	//fixed_length_unpacker uses basic_buffer as its message type, it doesn't support + operation too.
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(std::move(msg));}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg + " (from the server)");}
#endif
	//msg handling end
};
typedef server_base<my_server_socket> my_server;

#endif //#define _SERVER_H_
