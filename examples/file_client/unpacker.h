#ifndef UNPACKER_H_
#define UNPACKER_H_

#include <ascs/base.h>
using namespace ascs;
using namespace ascs::tcp;

#include "../file_server/common.h"

extern std::atomic_int_fast64_t received_size;

class data_unpacker : public i_unpacker<replaceable_buffer>
{
public:
	data_unpacker(FILE* file, fl_type data_len)  : _file(file), _data_len(data_len)
	{
		assert(nullptr != _file);

		buffer = new char[asio::detail::default_max_transfer_size];
		assert(nullptr != buffer);
	}
	~data_unpacker() {delete[] buffer;}

	virtual void reset() {_file = nullptr; delete[] buffer; buffer = nullptr; _data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		assert(_data_len >= (fl_type) bytes_transferred && bytes_transferred > 0);
		_data_len -= bytes_transferred;
		received_size += bytes_transferred;

		if (bytes_transferred != fwrite(buffer, 1, bytes_transferred, _file))
		{
			printf("fwrite(" ASCS_SF ") error!\n", bytes_transferred);
			return false;
		}

		if (0 == _data_len)
			msg_can.emplace_back();

		return true;
	}

	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) {return ec ? 0 : asio::detail::default_max_transfer_size;}
	virtual buffer_type prepare_next_recv()
	{
		auto buffer_len = _data_len > asio::detail::default_max_transfer_size ? asio::detail::default_max_transfer_size : (size_t) _data_len;
#ifdef ASCS_SCATTERED_RECV_BUFFER
		return buffer_type(1, asio::buffer(buffer, buffer_len));
#else
		return asio::buffer(buffer, buffer_len);
#endif
	}

protected:
	FILE* _file;
	char* buffer;

	fl_type _data_len;
};

#endif //UNPACKER_H_
