#ifndef _CLIENT_H_
#define _CLIENT_H_

//demonstrates how to call multi_client_base in client_socket_base (just like server_socket_base call server_base)
class my_matrix : public i_matrix
{
public:
	virtual bool del_link(const std::string& name) = 0;
};

class my_client_socket : public client_socket_base<ASCS_DEFAULT_PACKER, ASCS_DEFAULT_UNPACKER, my_matrix>
{
public:
	my_client_socket(my_matrix* matrix_) : client_socket_base(matrix_)
	{
		std::dynamic_pointer_cast<prefix_suffix_packer>(packer())->prefix_suffix("", "\n");
		std::dynamic_pointer_cast<prefix_suffix_unpacker>(unpacker())->prefix_suffix("", "\n");
	}

	void name(const std::string& name_) {_name = name_;}
	const std::string& name() const {return _name;}

protected:
	//msg handling
	virtual bool on_msg_handle(out_msg_type& msg) {printf("received: %s, I'm %s\n", msg.data(), _name.data()); return true;}
	//msg handling end

	virtual void on_recv_error(const asio::error_code& ec) {get_matrix()->del_link(_name); client_socket_base::on_recv_error(ec);}

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
				link_map[name] = socket_ptr->id();

				return true;
			}
		}

		return false;
	}

	uint_fast64_t find_link(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(link_map_mutex);
		auto iter = link_map.find(name);
		return iter != std::end(link_map) ? iter->second : -1;
	}

	uint_fast64_t find_and_remove_link(const std::string& name)
	{
		uint_fast64_t id = -1;

		std::lock_guard<std::mutex> lock(link_map_mutex);
		auto iter = link_map.find(name);
		if (iter != std::end(link_map))
		{
			id = iter->second;
			link_map.erase(iter);
		}

		return id;
	}

	bool shutdown_link(const std::string& name)
	{
		auto socket_ptr = find(find_and_remove_link(name));
		return socket_ptr ? (socket_ptr->force_shutdown(), true) : false;
	}

	bool send_msg(const std::string& name, const std::string& msg)
	{
		auto socket_ptr = find(find_link(name));
		return socket_ptr ?  socket_ptr->send_msg(msg) : false;
	}

protected:
	//from my_matrix, pure virtual function, we must implement it.
	virtual bool del_link(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(link_map_mutex);
		return link_map.erase(name) > 0;
	}

private:
	std::map<std::string, uint_fast64_t> link_map;
	std::mutex link_map_mutex;
};

#endif //#define _CLIENT_H_
