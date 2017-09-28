
#ifndef FILE_CLIENT_H_
#define FILE_CLIENT_H_

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#include "unpacker.h"

extern int link_num;
extern fl_type file_size;
extern std::atomic_int_fast64_t received_size;

class file_socket : public base_socket, public client_socket
{
public:
	file_socket(asio::io_context& io_context_) : client_socket(io_context_), index(-1) {}
	virtual ~file_socket() {clear();}

	//reset all, be ensure that there's no any operations performed on this file_socket when invoke it
	virtual void reset() {clear(); client_socket::reset();}

	void set_index(int index_) {index = index_;}
	bool get_file(const std::string& file_name)
	{
		assert(!file_name.empty());

		if (TRANS_IDLE != state)
			return false;

		if (0 == id())
			file = fopen(file_name.data(), "w+b");
		else
			file = fopen(file_name.data(), "r+b");

		if (nullptr == file)
		{
			printf("can't create file %s.\n", file_name.data());
			return false;
		}

		std::string order("\0", ORDER_LEN);
		order += file_name;

		state = TRANS_PREPARE;
		send_msg(order, true);

		return true;
	}

	void talk(const std::string& str)
	{
		if (TRANS_IDLE == state && !str.empty())
		{
			std::string order("\2", ORDER_LEN);
			order += str;
			send_msg(order, true);
		}
	}

protected:
	//msg handling
#ifndef ASCS_FORCE_TO_USE_MSG_RECV_BUFFER
	//we always handle messages in on_msg(), so we don't care the type of input queue and input container at all.
	virtual bool on_msg(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//we will change unpacker at runtime, this operation can only be done in on_msg(), reset() or constructor,
	//so we must guarantee all messages to be handled in on_msg()
	//virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
	//msg handling end

	virtual void on_connect()
	{
		uint_fast64_t id = index;
		char buffer[ORDER_LEN + sizeof(uint_fast64_t)];

		*buffer = 3; //head
		memcpy(std::next(buffer, ORDER_LEN), &id, sizeof(uint_fast64_t));
		send_msg(buffer, sizeof(buffer), true);

		client_socket::on_connect();
	}

private:
	void clear()
	{
		state = TRANS_IDLE;
		if (nullptr != file)
		{
			fclose(file);
			file = nullptr;
		}

		unpacker(std::make_shared<ASCS_DEFAULT_UNPACKER>());
	}
	void trans_end() {clear();}

	void handle_msg(out_msg_ctype& msg)
	{
		if (TRANS_BUSY == state)
		{
			assert(msg.empty());
			trans_end();
			return;
		}
		else if (msg.size() <= ORDER_LEN)
		{
			printf("wrong order length: " ASCS_SF ".\n", msg.size());
			return;
		}

		switch (*msg.data())
		{
		case 0:
			if (ORDER_LEN + DATA_LEN == msg.size() && nullptr != file && TRANS_PREPARE == state)
			{
				fl_type length;
				memcpy(&length, std::next(msg.data(), ORDER_LEN), DATA_LEN);
				if (-1 == length)
				{
					if (0 == index)
						puts("get file failed!");
					trans_end();
				}
				else
				{
					if (0 == index)
						file_size = length;

					auto my_length = length / link_num;
					auto offset = my_length * index;

					if (link_num - 1 == index)
						my_length = length - offset;
					if (my_length > 0)
					{
						char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN];
						*buffer = 1; //head
						memcpy(std::next(buffer, ORDER_LEN), &offset, OFFSET_LEN);
						memcpy(std::next(buffer, ORDER_LEN + OFFSET_LEN), &my_length, DATA_LEN);

						state = TRANS_BUSY;
						send_msg(buffer, sizeof(buffer), true);

						fseeko(file, offset, SEEK_SET);
						unpacker(std::make_shared<data_unpacker>(file, my_length));
					}
					else
						trans_end();
				}
			}
			break;
		case 2:
			if (0 == index)
				printf("server says: %s\n", std::next(msg.data(), ORDER_LEN));
			break;
		default:
			break;
		}
	}

private:
	int index;
};

class file_client : public multi_client_base<file_socket>
{
public:
	static const tid TIMER_BEGIN = multi_client_base<file_socket>::TIMER_END;
	static const tid UPDATE_PROGRESS = TIMER_BEGIN;
	static const tid TIMER_END = TIMER_BEGIN + 10;

	file_client(service_pump& service_pump_) : multi_client_base<file_socket>(service_pump_) {}

	bool is_transferring() const {return is_timer(UPDATE_PROGRESS);}
	bool get_file(const std::string& file_name)
	{
		if (is_transferring())
			printf("file transfer is ongoing for file %s", file_name.data());
		else
		{
			printf("transfer %s begin.\n", file_name.data());
			if (find(0)->get_file(file_name))
			{
				//do_something_to_all([&file_name](object_ctype& item) {if (0 != item->id()) item->get_file(file_name);});
				//if you always return false, do_something_to_one will be equal to do_something_to_all.
				do_something_to_one([&file_name](object_ctype& item)->bool {if (0 != item->id()) item->get_file(file_name); return false;});
				begin_time.restart();
				set_timer(UPDATE_PROGRESS, 50, [this](tid id)->bool {return this->update_progress_handler(id, -1);});

				return true;
			}
			else
				printf("transfer %s failed!\n", file_name.data());
		}

		return false;
	}

private:
	bool update_progress_handler(tid id, unsigned last_percent)
	{
		assert(UPDATE_PROGRESS == id);

		if (file_size < 0)
			return true;
		else if (file_size > 0)
		{
			auto new_percent = (unsigned) (received_size * 100 / file_size);
			if (last_percent != new_percent)
			{
				printf("\r%u%%", new_percent);
				fflush(stdout);

				update_timer_info(id, 50, [new_percent, this](tid id)->bool {return this->update_progress_handler(id, new_percent);});
			}
		}

		if (received_size < file_size)
			return true;

		printf("\r100%%\nend, speed: %f MBps.\n", file_size / begin_time.elapsed() / 1024 / 1024);
		return false;
	}

protected:
	cpu_timer begin_time;
};

#endif //#ifndef FILE_CLIENT_H_
