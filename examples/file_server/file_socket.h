
#ifndef FILE_SOCKET_H_
#define FILE_SOCKET_H_

#include <ascs/ext/tcp.h>
using namespace ascs::tcp;
using namespace ascs::ext::tcp;

#include "../file_common/common.h"

class file_socket : public base_socket, public server_socket
{
public:
	file_socket(i_server& server_);
	virtual ~file_socket();

public:
	//because we don't use objects pool(we don't defined ASCS_REUSE_OBJECT), so this virtual function will
	//not be invoked, and can be omitted, but we keep it for the possibility of using it in the future
	virtual void reset();
	virtual void take_over(std::shared_ptr<file_socket> socket_ptr);

protected:
	//msg handling
	virtual bool on_msg_handle(out_msg_type& msg);
	//msg handling end

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg);
#endif

private:
	void clear();
	void trans_end(bool reset_unpacker = true);
	void handle_msg(out_msg_ctype& msg);
};

#endif //#ifndef FILE_SOCKET_H_
