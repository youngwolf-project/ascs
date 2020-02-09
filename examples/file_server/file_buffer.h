#ifndef FILE_BUFFER_H_
#define FILE_BUFFER_H_

#include <ascs/base.h>
using namespace ascs;

#include "common.h"

class file_buffer : public i_buffer, public asio::noncopyable
{
public:
	file_buffer(FILE* file, fl_type total_len_) : _file(file), total_len(total_len_)
	{
		assert(nullptr != _file);

		buffer = new char[asio::detail::default_max_transfer_size];
		assert(nullptr != buffer);

		read();
	}
	~file_buffer() {delete[] buffer;}

public:
	virtual bool empty() const {return 0 == data_len;}
	virtual size_t size() const {return data_len;}
	virtual const char* data() const {return buffer;}

	void read()
	{
		if (0 == total_len)
			data_len = 0;
		else
		{
			data_len = total_len > asio::detail::default_max_transfer_size ? asio::detail::default_max_transfer_size : (size_t) total_len;
			total_len -= data_len;
			if (data_len != fread(buffer, 1, data_len, _file))
			{
				printf("fread(" ASCS_SF ") error!\n", data_len);
				data_len = 0;
			}
		}
	}

protected:
	FILE* _file;
	char* buffer;
	size_t data_len;

	fl_type total_len;
};

#endif //FILE_BUFFER_H_
