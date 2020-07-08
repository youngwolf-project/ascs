
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
	file_socket(i_matrix& matrix_) : client_socket(matrix_), index(-1) {}
	virtual ~file_socket() {clear();}

	//reset all, be ensure that there's no any operations performed on this file_socket when invoke it
	virtual void reset() {clear(); client_socket::reset();}

	bool is_idle() const {return TRANS_IDLE == state;}
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
		send_msg(std::move(order), true);

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
			ascs::do_something_to_all(msg_can, [this](out_msg_type& msg) {this->handle_msg(msg);});
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

		ascs::do_something_to_all(tmp_can, [this](out_msg_type& msg) {this->handle_msg(msg);});

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

		unpacker(std::make_shared<ASCS_DEFAULT_UNPACKER>());
		state = TRANS_IDLE;
	}
	void trans_end() {clear();}

	void handle_msg(out_msg_ctype& msg)
	{
		if (TRANS_BUSY == state)
		{
			assert(msg.empty());

			auto unp = std::dynamic_pointer_cast<file_unpacker>(unpacker());
			if (!unp || unp->is_finished())
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
						unpacker(std::make_shared<file_unpacker>(file, my_length));
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
	static const tid TIMER_END = TIMER_BEGIN + 5;

	file_client(service_pump& service_pump_) : multi_client_base<file_socket>(service_pump_) {}

	void get_file(std::list<std::string>&& files)
	{
		std::unique_lock<std::mutex> lock(file_list_mutex);
		file_list.splice(std::end(file_list), files);
		lock.unlock();

		get_file();
	}
	void get_file(const std::list<std::string>& files) {get_file(std::list<std::string>(files));}

	bool is_end()
	{
		size_t idle_num = 0;
		do_something_to_all([&](object_ctype& item) {if (item->is_idle()) ++idle_num;});
		return idle_num == size();
	}

private:
	void get_file()
	{
		std::lock_guard<std::mutex> lock(file_list_mutex);

		if (is_timer(UPDATE_PROGRESS))
			return;

		while (!file_list.empty())
		{
			std::string file_name(std::move(file_list.front()));
			file_list.pop_front();

			file_size = -1;
			received_size = 0;

			printf("transfer %s begin.\n", file_name.data());
			if (find(0)->get_file(file_name))
			{
				//do_something_to_all([&file_name](object_ctype& item) {if (0 != item->id()) item->get_file(file_name);});
				//if you always return false, do_something_to_one will be equal to do_something_to_all.
				do_something_to_one([&file_name](object_ctype& item)->bool {if (0 != item->id()) item->get_file(file_name); return false;});
				begin_time.restart();
				set_timer(UPDATE_PROGRESS, 50, [this](tid id)->bool {return this->update_progress_handler(id, -1);});

				break;
			}
			else
				printf("transfer %s failed!\n", file_name.data());
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
			get_file();

			return false;
		}
		else if (file_size > 0)
		{
			auto new_percent = (unsigned) (received_size * 100 / file_size);
			if (last_percent != new_percent)
			{
				printf("\r%u%%", new_percent);
				fflush(stdout);

				change_timer_call_back(id, [new_percent, this](tid id)->bool {return this->update_progress_handler(id, new_percent);});
			}
		}

		if (received_size < file_size)
		{
			if (!is_end())
				return true;

			change_timer_status(id, timer_info::TIMER_CANCELED);
			get_file();

			return false;
		}

		printf("\r100%%\nend, speed: %f MBps.\n\n", file_size / begin_time.elapsed() / 1024 / 1024);
		change_timer_status(id, timer_info::TIMER_CANCELED);

		//wait all file_socket to clean up themselves
		while (!is_end()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		get_file();

		return false;
	}

protected:
	cpu_timer begin_time;

	std::list<std::string> file_list;
	std::mutex file_list_mutex;
};

#endif //#ifndef FILE_CLIENT_H_
