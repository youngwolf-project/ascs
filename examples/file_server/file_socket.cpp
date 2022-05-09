
//configuration
#define ASCS_DEFAULT_PACKER packer2<>
//#define ASCS_RECV_BUFFER_TYPE std::vector<asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
//#define ASCS_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to ascs
//note, these two macro are not requisite, I'm just showing how to use them.

//all other definitions are in the makefile, because we have two cpp files, defining them in more than one place is risky (
// we may define them to different values between the two cpp files)
//configuration

#include "../file_common/file_buffer.h"
#include "../file_common/unpacker.h"

#include "file_socket.h"

file_socket::file_socket(i_server& server_) : server_socket(server_), retry_num(0) {}
file_socket::~file_socket() {clear();}

void file_socket::reset() {trans_end(); server_socket::reset();}
void file_socket::take_over(std::shared_ptr<file_socket> socket_ptr) {if (socket_ptr) printf("restore user data from invalid object (" ASCS_LLF ").\n", socket_ptr->id());}

//msg handling
bool file_socket::on_msg_handle(out_msg_type& msg) {auto re = handle_msg(msg); if (re && 0 == get_pending_recv_msg_size()) recv_msg(); return re;}
//msg handling end

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
void file_socket::on_msg_send(in_msg_type& msg)
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

void file_socket::clear()
{
	if (nullptr != file)
	{
		fclose(file);
		file = nullptr;
	}
}

void file_socket::trans_end(bool reset_unpacker)
{
	clear();

	stop_timer(server_socket::TIMER_END);
	if (reset_unpacker)
		unpacker(std::make_shared<ASCS_DEFAULT_UNPACKER>());
	state = TRANS_IDLE;
	retry_num = 0;
}

void file_socket::response_put_file(fl_type offset, fl_type length, bool success)
{
	char buffer[ORDER_LEN + DATA_LEN + 1];
	*buffer = 10; //head
	memcpy(std::next(buffer, ORDER_LEN), &length, DATA_LEN);
	*std::next(buffer, ORDER_LEN + DATA_LEN) = success ? '\0' : '\1';
	
	if (success)
	{
		printf("start to accept the file from " ASCS_LLF " with length " ASCS_LLF "\n", offset, length);

		state = TRANS_BUSY;
		fseeko(file, offset, SEEK_SET);
		unpacker(std::make_shared<file_unpacker>(file, length)); //replace the unpacker first, then response put file request
	}
	else
		puts("failed to create the file!");

	send_msg(buffer, sizeof(buffer), true);
}

bool file_socket::handle_msg(out_msg_ctype& msg)
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

		return true;
	}
	else if (msg.size() <= ORDER_LEN)
	{
		printf("wrong order length: " ASCS_SF ".\n", msg.size());
		return true;
	}

	switch (*msg.data())
	{
	case 0:
		//let the client to control the life cycle of file transmission, so we don't care the state
		//avoid accessing the send queue concurrently, because we use non_lock_queue
		if (/*TRANS_IDLE == state && */!is_sending())
		{
			trans_end();

			char buffer[ORDER_LEN + DATA_LEN];
			*buffer = 0; //head

			auto file_name = std::next(msg.data(), ORDER_LEN);
			printf("prepare to send file %s\n", file_name);
			file = fopen(file_name, "rb");
			if (nullptr != file)
			{
				fseeko(file, 0, SEEK_END);
				auto length = ftello(file);
				memcpy(std::next(buffer, ORDER_LEN), &length, DATA_LEN);
				state = TRANS_PREPARE;
			}
			else
			{
				memset(std::next(buffer, ORDER_LEN), -1, DATA_LEN);
				printf("can not open file %s!\n", std::next(msg.data(), ORDER_LEN));
			}

			send_msg(buffer, sizeof(buffer), true);
		}
		break;
	case 1:
		if (TRANS_PREPARE == state && nullptr != file && ORDER_LEN + OFFSET_LEN + DATA_LEN == msg.size())
		{
			fl_type offset;
			memcpy(&offset, std::next(msg.data(), ORDER_LEN), OFFSET_LEN);
			fl_type length;
			memcpy(&length, std::next(msg.data(), ORDER_LEN + OFFSET_LEN), DATA_LEN);
			if (offset >= 0 && length > 0 && offset + length <= ftello(file))
			{
				printf("start to send the file from " ASCS_LLF " with length " ASCS_LLF "\n", offset, length);

				state = TRANS_BUSY;
				fseeko(file, offset, SEEK_SET);
				direct_send_msg(in_msg_type(new file_buffer(file, length)), true);
			}
			else
				trans_end();
		}
		break;
	case 10:
		//let the client to control the life cycle of file transmission, so we don't care the state
		//avoid accessing the send queue concurrently, because we use non_lock_queue
		if (/*TRANS_IDLE == state && */msg.size() > ORDER_LEN + OFFSET_LEN + DATA_LEN + 1 && !is_sending())
		{
			trans_end();

			fl_type offset;
			memcpy(&offset, std::next(msg.data(), ORDER_LEN), OFFSET_LEN);
			fl_type length;
			memcpy(&length, std::next(msg.data(), ORDER_LEN + OFFSET_LEN), DATA_LEN);

			auto file_name = std::next(msg.data(), ORDER_LEN + OFFSET_LEN + DATA_LEN + 1);
			if (0 == *std::next(msg.data(), ORDER_LEN + OFFSET_LEN + DATA_LEN))
			{
				printf("prepare to accept file %s from " ASCS_LLF " with length " ASCS_LLF "\n", file_name, offset, length);
				file = fopen(file_name, "w+b");
				response_put_file(offset, length, nullptr != file);
			}
			else
			{
				auto re = nullptr != (file = fopen(std::next(msg.data(), ORDER_LEN + OFFSET_LEN + DATA_LEN + 1), "r+b"));
				if (!re && ++retry_num < 10) //wait (up to 10 times of ASCS_MSG_HANDLING_INTERVAL) the leader to create the file
					return false;

				response_put_file(offset, length, re);
			}
		}
		break;
	case 2:
		printf("client says: %s\n", std::next(msg.data(), ORDER_LEN));
		break;
	case 3:
		if (ORDER_LEN + sizeof(uint_fast64_t) == msg.size())
		{
			uint_fast64_t id;
			memcpy(&id, std::next(msg.data(), ORDER_LEN), sizeof(uint_fast64_t));
			if (!get_server().restore_socket(shared_from_this(), id, false)) //client restores the peer socket on the server
				get_server().restore_socket(shared_from_this(), id, true); //client controls the id of peer socket on the server
				//although you always want socket restoration, you must set the id of peer socket for the first time
		}
	default:
		break;
	}

	return true;
}
