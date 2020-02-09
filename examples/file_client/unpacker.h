#ifndef UNPACKER_H_
#define UNPACKER_H_

#include <ascs/base.h>
using namespace ascs;
using namespace ascs::tcp;

#include "../file_server/common.h"

extern std::atomic_int_fast64_t received_size;

class file_unpacker : public i_unpacker<std::string>, public asio::noncopyable
{
public:
	file_unpacker(FILE* file, fl_type total_len_)  : _file(file), total_len(total_len_)
	{
		assert(nullptr != _file);

		buffer = new char[asio::detail::default_max_transfer_size];
		assert(nullptr != buffer);
	}
	~file_unpacker() {delete[] buffer;}

	bool is_finished() const {return 0 == total_len;}

	virtual void reset() {_file = nullptr; delete[] buffer; buffer = nullptr; total_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		assert(total_len >= (fl_type) bytes_transferred && bytes_transferred > 0);
		total_len -= bytes_transferred;
		received_size += bytes_transferred;

		if (bytes_transferred == fwrite(buffer, 1, bytes_transferred, _file))
			return true;

		printf("fwrite(" ASCS_SF ") error!\n", bytes_transferred);
		return false;
	}

	virtual size_t completion_condition(const asio::error_code& ec, size_t bytes_transferred) {return ec ? 0 : asio::detail::default_max_transfer_size;}
	virtual buffer_type prepare_next_recv()
	{
		auto data_len = total_len > asio::detail::default_max_transfer_size ? asio::detail::default_max_transfer_size : (size_t) total_len;
#ifdef ASCS_SCATTERED_RECV_BUFFER
		return buffer_type(1, asio::buffer(buffer, data_len));
#else
		return asio::buffer(buffer, data_len);
#endif
	}

protected:
	FILE* _file;
	char* buffer;

	fl_type total_len;
};

#endif //UNPACKER_H_
