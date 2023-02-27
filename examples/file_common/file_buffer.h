#ifndef FILE_BUFFER_H_
#define FILE_BUFFER_H_

#include "common.h"

class file_buffer : public i_buffer, public boost::noncopyable
{
public:
	file_buffer(FILE* file, fl_type total_len_, std::atomic_int_fast64_t* transmit_size_ = nullptr) : _file(file), total_len(total_len_), transmit_size(transmit_size_)
	{
		assert(nullptr != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(nullptr != buffer);

		good = read();
	}
	~file_buffer() {delete[] buffer;}

public:
	virtual bool empty() const {return 0 == data_len;}
	virtual size_t size() const {return data_len;}
	virtual const char* data() const {return buffer;}

	bool is_good() const {return good;}
	bool read()
	{
		if (total_len <= 0)
			data_len = 0;
		else
		{
			data_len = total_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) total_len;
			total_len -= data_len;
			if (data_len != fread(buffer, 1, data_len, _file))
			{
				printf("fread(" ASCS_SF ") error!\n", data_len);
				data_len = 0;

				return (good = false);
			}
			else if (nullptr != transmit_size)
				*transmit_size += data_len;
		}

		return true;
	}

protected:
	bool good;
	FILE* _file;
	char* buffer;
	size_t data_len;

	fl_type total_len;
	std::atomic_int_fast64_t* transmit_size;
};

#endif //FILE_BUFFER_H_
