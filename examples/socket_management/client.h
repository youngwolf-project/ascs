#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <map>

//demonstrates how to call multi_client_base in client_socket_base (just like server_socket_base call server_base)
class my_matrix : public i_matrix
{
public:
	virtual bool del_link(const std::string& name) = 0;
};

class my_client_socket : public client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, my_matrix>
{
public:
	my_client_socket(my_matrix& matrix_) : client_socket_base(matrix_)
	{
#if 3 == PACKER_UNPACKER_TYPE
		std::dynamic_pointer_cast<ASCS_DEFAULT_PACKER>(packer())->prefix_suffix("", "\n");
		std::dynamic_pointer_cast<ASCS_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("", "\n");
#endif
	}

	void name(const std::string& name_) {_name = name_;}
	const std::string& name() const {return _name;}

protected:
	//msg handling
#if 2 == PACKER_UNPACKER_TYPE
	//fixed_length_unpacker uses basic_buffer as its message type, it doesn't append additional \0 to the end of the message as std::string does,
	//so it cannot be printed by printf.
	virtual bool on_msg_handle(out_msg_type& msg) {printf("received: " ASCS_SF ", I'm %s\n", msg.size(), _name.data()); return true;}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {printf("received: %s, I'm %s\n", msg.data(), _name.data()); return true;}
#endif
	//msg handling end

	virtual void on_recv_error(const boost::system::error_code& ec) {get_matrix()->del_link(_name); client_socket_base::on_recv_error(ec);}

private:
	std::string _name;
};

class my_client : public multi_client_base<my_client_socket, object_pool<my_client_socket>, my_matrix>
{
public:
	my_client(service_pump& service_pump_) : multi_client_base(service_pump_) {}

	bool add_link(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(link_map_mutex);
		if (link_map.count(name) > 0)
			printf("%s already exists.\n", name.data());
		else
		{
			auto socket_ptr = create_object();
			assert(socket_ptr);

			socket_ptr->name(name);
			//socket_ptr->set_server_addr(9527, "127.0.0.1"); //if you want to set server ip, do it at here like this

			if (add_socket(socket_ptr)) //exceed ASCS_MAX_OBJECT_NUM
			{
				printf("add socket %s.\n", name.data());
				link_map[name] = socket_ptr;

				return true;
			}
		}

		return false;
	}

	object_type find_link(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(link_map_mutex);
		auto iter = link_map.find(name);
		return iter != std::end(link_map) ? iter->second : object_type();
	}

	object_type find_and_remove_link(const std::string& name)
	{
		object_type socket_ptr;

		std::lock_guard<std::mutex> lock(link_map_mutex);
		auto iter = link_map.find(name);
		if (iter != std::end(link_map))
		{
			socket_ptr = iter->second;
			link_map.erase(iter);
		}

		return socket_ptr;
	}

	bool shutdown_link(const std::string& name)
	{
		auto socket_ptr = find_and_remove_link(name);
		return socket_ptr ? (socket_ptr->force_shutdown(), true) : false;
	}

	template<typename T> bool send_msg(const std::string& name, T&& msg)
	{
		auto socket_ptr = find_link(name);
		return socket_ptr ? socket_ptr->send_msg(std::forward<T>(msg)) : false;
	}

protected:
	//from my_matrix, pure virtual function, we must implement it.
	virtual bool del_link(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(link_map_mutex);
		return link_map.erase(name) > 0;
	}

private:
	std::map<std::string, object_type> link_map;
	std::mutex link_map_mutex;
};

#endif //#define _CLIENT_H_
