
#ifndef FILE_CLIENT_H_
#define FILE_CLIENT_H_

#include <ascs/ext/tcp.h>
using namespace ascs;
using namespace ascs::tcp;
using namespace ascs::ext;
using namespace ascs::ext::tcp;

#include "../file_common/file_buffer.h"
#include "../file_common/unpacker.h"

extern int link_num;
extern fl_type file_size;
extern std::atomic_int_fast64_t transmit_size;

class file_socket : public base_socket, public client_socket
{
public:
	file_socket(i_matrix& matrix_) : client_socket(matrix_), index(-1) {}
	virtual ~file_socket() {clear();}

	//reset all, be ensure that there's no any operations performed on this file_socket when invoke it
	virtual void reset() {trans_end(); client_socket::reset();}

	bool is_idle() const {return TRANS_IDLE == state;}
	void set_index(int index_) {index = index_;}

	bool get_file(const std::string& file_name)
	{
		assert(!file_name.empty());

		if (TRANS_IDLE != state)
			return false;

		if (0 == index)
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
		send_msg(std::move(order), true);

		return true;
	}

	bool put_file(const std::string& file_name)
	{
		assert(!file_name.empty());

		if (TRANS_IDLE != state)
			return false;

		file = fopen(file_name.data(), "rb");
		if (nullptr == file)
		{
			printf("can't open file %s.\n", file_name.data());
			return false;
		}

		char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN + 1];
		*buffer = 10; //head

		fseeko(file, 0, SEEK_END);
		auto length = ftello(file);
		if (link_num - 1 == index)
			file_size = length;

		auto my_length = length / link_num;
		auto offset = my_length * index;
		fseeko(file, offset, SEEK_SET);

		if (link_num - 1 == index)
			my_length = length - offset;
		if (my_length > 0)
		{
			memcpy(std::next(buffer, ORDER_LEN), &offset, OFFSET_LEN);
			memcpy(std::next(buffer, ORDER_LEN + OFFSET_LEN), &my_length, DATA_LEN);
			*std::next(buffer, ORDER_LEN + OFFSET_LEN + DATA_LEN) = link_num - 1 == index ? 0 : 1;

			std::string order(buffer, sizeof(buffer));
			order += file_name;

			state = TRANS_PREPARE;
			send_msg(std::move(order), true);
		}
		else
			trans_end(false);

		return true;
	}

	void talk(const std::string& str)
	{
		if (TRANS_IDLE == state && !str.empty())
		{
			std::string order("\2", ORDER_LEN);
			order += str;
			send_msg(std::move(order), true);
		}
	}

protected:
	//msg handling
#ifdef ASCS_SYNC_DISPATCH
	virtual size_t on_msg(std::list<out_msg_type>& msg_can)
	{
		//ascs will never append empty message automatically for on_msg (if no message nor error returned from the unpacker) even with
		// macro ASCS_PASSIVE_RECV, but will do it for on_msg_handle (with macro ASCS_PASSIVE_RECV), please note.
		if (msg_can.empty())
			handle_msg(out_msg_type()); //we need empty message as a notification, it's just our business logic.
		else
		{
			ascs::do_something_to_all(msg_can, [this](out_msg_type& msg) {handle_msg(msg);});
			msg_can.clear();
		}

		recv_msg(); //we always handled all messages, so calling recv_msg() at here is very reasonable.
		return 1;
		//if we indeed handled some messages, do return 1
		//if we handled nothing, return 1 is also okey but will very slightly impact performance (if msg_can is not empty), return 0 is suggested
	}
#endif
#ifdef ASCS_DISPATCH_BATCH_MSG
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		//msg_can can't be empty, with macro ASCS_PASSIVE_RECV, ascs will append an empty message automatically for on_msg_handle if no message nor
		// error returned from the unpacker to provide a chance to call recv_msg (calling recv_msg out of on_msg and on_msg_handle is forbidden), please note.
		assert(!msg_can.empty());
		out_container_type tmp_can;
		msg_can.swap(tmp_can);

		ascs::do_something_to_all(tmp_can, [this](out_msg_type& msg) {handle_msg(msg);});

		recv_msg(); //we always handled all messages, so calling recv_msg() at here is very reasonable.
		return 1;
		//if we indeed handled some messages, do return 1, else, return 0
		//if we handled nothing, but want to re-dispatch messages immediately, return 1
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); if (0 == get_pending_recv_msg_size()) recv_msg(); return true;}
	//only raise recv_msg() invocation after recveiving buffer becomes empty, it's very important, otherwise we must use mutex to guarantee that at any time,
	//there only exists one or zero asynchronous reception.
#endif
	//msg handling end

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg)
	{
		auto buffer = dynamic_cast<file_buffer*>(&*msg.raw_buffer());
		if (nullptr != buffer)
		{
			buffer->read();
			if (buffer->empty())
			{
				puts("file sending end successfully");
				trans_end(false);
			}
			else
				direct_send_msg(std::move(msg), true);
		}
	}
#endif

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
		if (nullptr != file)
		{
			fclose(file);
			file = nullptr;
		}
	}

	void trans_end(bool reset_unpacker = true)
	{
		clear();

		if (reset_unpacker)
			unpacker(std::make_shared<ASCS_DEFAULT_UNPACKER>());
		state = TRANS_IDLE;
	}

	void handle_msg(out_msg_ctype& msg)
	{
		if (TRANS_BUSY == state)
		{
			assert(msg.empty());

			auto unp = std::dynamic_pointer_cast<file_unpacker>(unpacker());
			if (!unp)
				trans_end();
			else if (unp->is_finished())
			{
				puts("file accepting end successfully");
				trans_end();
			}

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

						printf("start to accept the file from " ASCS_LLF " with legnth " ASCS_LLF "\n", offset, my_length);

						state = TRANS_BUSY;
						fseeko(file, offset, SEEK_SET);
						unpacker(std::make_shared<file_unpacker>(file, my_length, &transmit_size));

						send_msg(buffer, sizeof(buffer), true); //replace the unpacker first, then response get file request
					}
					else
						trans_end();
				}
			}
			break;
		case 10:
			if (ORDER_LEN + DATA_LEN + 1 == msg.size() && nullptr != file && TRANS_PREPARE == state)
			{
				if ('\0' != *std::next(msg.data(), ORDER_LEN + DATA_LEN))
				{
					if (link_num - 1 == index)
						puts("put file failed!");
					trans_end();
				}
				else
				{
					fl_type length;
					memcpy(&length, std::next(msg.data(), ORDER_LEN), DATA_LEN);

					printf("start to send the file with length " ASCS_LLF "\n", length);

					state = TRANS_BUSY;
					direct_send_msg(in_msg_type(new file_buffer(file, length, &transmit_size)), true);
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
	static const tid TIMER_END = TIMER_BEGIN + 5;

	file_client(service_pump& service_pump_) : multi_client_base<file_socket>(service_pump_) {}

	void get_file(const std::list<std::string>& files)
	{
		ascs::do_something_to_all(files, file_list_mutex, [this](const std::string& filename) {this->file_list.emplace_back(std::make_pair(0, filename));});
		transmit_file();
	}

	void put_file(const std::list<std::string>& files)
	{
		ascs::do_something_to_all(files, file_list_mutex, [this](const std::string& filename) {this->file_list.emplace_back(std::make_pair(1, filename));});
		transmit_file();
	}

	bool is_end()
	{
		size_t idle_num = 0;
		do_something_to_all([&](object_ctype& item) {if (item->is_idle()) ++idle_num;});
		return idle_num == size();
	}

private:
	void transmit_file()
	{
		std::lock_guard<std::mutex> lock(file_list_mutex);

		if (is_timer(UPDATE_PROGRESS))
			return;

		while (!file_list.empty())
		{
			auto file_name(std::move(file_list.front().second));
			auto type = file_list.front().first;
			file_list.pop_front();

			file_size = -1;
			transmit_size = 0;

			printf("transmit %s begin.\n", file_name.data());
			auto re = false;
			if (0 == type)
			{
				if ((re = find(0)->get_file(file_name)))
					//do_something_to_all([&](object_ctype& item) {if (0 != item->id()) item->get_file(file_name);});
					//if you always return false, do_something_to_one will be equal to do_something_to_all.
					do_something_to_one([&](object_ctype& item) {if (0 != item->id()) item->get_file(file_name); return false;});
			}
			else if ((re = find(link_num - 1)->put_file(file_name)))
				do_something_to_all([&](object_ctype& item) {if ((unsigned) (link_num - 1) != item->id()) item->put_file(file_name);});

			if (re)
			{
				begin_time.restart();
				set_timer(UPDATE_PROGRESS, 50, [this](tid id)->bool {return update_progress_handler(id, -1);});
				break;
			}
			else
				printf("transmit %s failed!\n", file_name.data());
		}
	}

	bool update_progress_handler(tid id, unsigned last_percent)
	{
		assert(UPDATE_PROGRESS == id);

		if (file_size < 0)
		{
			if (!is_end())
				return true;

			change_timer_status(id, timer_info::TIMER_CANCELED);
			transmit_file();

			return false;
		}
		else if (file_size > 0)
		{
			auto new_percent = (unsigned) (transmit_size * 100 / file_size);
			if (last_percent != new_percent)
			{
				printf("\r%u%%", new_percent);
				fflush(stdout);

				change_timer_call_back(id, ASCS_COPY_ALL_AND_THIS(tid id)->bool {return update_progress_handler(id, new_percent);});
			}
		}

		if (transmit_size < file_size)
		{
			if (!is_end())
				return true;

			change_timer_status(id, timer_info::TIMER_CANCELED);
			transmit_file();

			return false;
		}

		printf("\r100%%\nend, speed: %f MBps.\n\n", file_size / begin_time.elapsed() / 1024 / 1024);
		change_timer_status(id, timer_info::TIMER_CANCELED);

		//wait all file_socket to clean up themselves
		while (!is_end()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		transmit_file();

		return false;
	}

protected:
	cpu_timer begin_time;

	std::list<std::pair<int, std::string>> file_list;
	std::mutex file_list_mutex;
};

#endif //#ifndef FILE_CLIENT_H_
