
//configuration
//#define ASCS_INPUT_QUEUE non_lock_queue
//file_server / file_client is a responsive system, before file_server send each message (except talking message,
//but file_server only receive talking message, don't send talking message proactively), the previous message has been
//sent to file_client, so sending buffer will always be empty, which means we will never operate sending buffer concurrently,
//so need no locks.
#define ASCS_DEFAULT_PACKER packer2<>
//#define ASCS_RECV_BUFFER_TYPE std::vector<asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
//#define ASCS_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to ascs
//note, these two macro are not requisite, i'm just showing how to use them.

//all other definitions are in the makefile, because we have two cpp files, defining them in more than one place is riskful (
// we may define them to different values between the two cpp files)
//configuration

#include "file_socket.h"

file_socket::file_socket(i_server& server_) : server_socket(server_) {}
file_socket::~file_socket() {trans_end();}

void file_socket::reset() {trans_end(); server_socket::reset();}
void file_socket::take_over(std::shared_ptr<file_socket> socket_ptr) {if (socket_ptr) printf("restore user data from invalid object (" ASCS_LLF ").\n", socket_ptr->id());}

//msg handling
bool file_socket::on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
//msg handling end

#ifdef ASCS_WANT_MSG_SEND_NOTIFY
void file_socket::on_msg_send(in_msg_type& msg)
{
	auto buffer = dynamic_cast<file_buffer*>(&*msg.raw_buffer());
	if (nullptr != buffer)
	{
		buffer->read();
		if (buffer->empty())
			trans_end();
		else
			direct_send_msg(std::move(msg), true);
	}
}
#endif

void file_socket::trans_end()
{
	state = TRANS_IDLE;
	if (nullptr != file)
	{
		fclose(file);
		file = nullptr;
	}
}

void file_socket::handle_msg(out_msg_ctype& msg)
{
	if (msg.size() <= ORDER_LEN)
	{
		printf("wrong order length: " ASCS_SF ".\n", msg.size());
		return;
	}

	switch (*msg.data())
	{
	case 0:
		if (TRANS_IDLE == state)
		{
			trans_end();

			char buffer[ORDER_LEN + DATA_LEN];
			*buffer = 0; //head

			file = fopen(std::next(msg.data(), ORDER_LEN), "rb");
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
				state = TRANS_BUSY;
				fseeko(file, offset, SEEK_SET);
				direct_send_msg(in_msg_type(new file_buffer(file, length)), true);
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
			get_server().restore_socket(this->shared_from_this(), id, true);
		}
	default:
		break;
	}
}
