
#ifndef FILE_SOCKET_H_
#define FILE_SOCKET_H_

#include <ascs/ext/tcp.h>
using namespace ascs::tcp;
using namespace ascs::ext::tcp;

#include "file_buffer.h"

class file_socket : public base_socket, public server_socket
{
public:
	file_socket(i_server& server_);
	virtual ~file_socket();

public:
	//because we don't use objects pool(we don't defined ASCS_REUSE_OBJECT), so this virtual function will
	//not be invoked, and can be omitted, but we keep it for possibly future using
	virtual void reset();
	virtual void take_over(std::shared_ptr<server_socket> socket_ptr); //move socket_ptr into this socket
//	virtual void take_over(std::shared_ptr<file_socket> socket_ptr); //this works too, but brings warnings with -Woverloaded-virtual option.

protected:
	//msg handling
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	//we can handle msg very fast, so we don't use recv buffer
	virtual bool on_msg(out_msg_type& msg);
#endif
	virtual bool on_msg_handle(out_msg_type& msg);
	//msg handling end

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg);
#endif

private:
	void trans_end();
	void handle_msg(out_msg_ctype& msg);
};

#endif //#ifndef FILE_SOCKET_H_
