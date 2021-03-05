/*
 * service_pump.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at both server and client endpoint
 */

#ifndef _ASCS_SERVICE_PUMP_H_
#define _ASCS_SERVICE_PUMP_H_

#include "base.h"

namespace ascs
{

class service_pump : public asio::io_context
{
public:
	class i_service
	{
	protected:
		i_service(service_pump& service_pump_) : sp(service_pump_), started_(false), id_(0), data(nullptr) {service_pump_.add(this);}
		virtual ~i_service() {}

	public:
		//for the same i_service, start_service and stop_service are not thread safe,
		//to resolve this defect, we must add a mutex member variable to i_service, it's not worth
		void start_service() {if (!started_) started_ = init();}
		void stop_service() {if (started_) uninit(); started_ = false;}
		bool service_started() const {return started_;}

		void id(int id) {id_ = id;}
		int id() const {return id_;}
		void user_data(void* data_) {data = data_;}
		void* user_data() const {return data;}

		service_pump& get_service_pump() {return sp;}
		const service_pump& get_service_pump() const {return sp;}

	protected:
		virtual bool init() = 0;
		virtual void uninit() = 0;

	protected:
		service_pump& sp;

	private:
		bool started_;
		int id_;
		void* data; //magic data, you can use it in any way
	};

public:
	typedef i_service* object_type;
	typedef const object_type object_ctype;
	typedef std::list<object_type> container_type;

#if ASIO_VERSION >= 101200
	service_pump(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) : asio::io_context(concurrency_hint), started(false)
#else
	service_pump() : started(false)
#endif
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
		, real_thread_num(0), del_thread_num(0)
#endif
#ifdef ASCS_AVOID_AUTO_STOP_SERVICE
#if ASIO_VERSION >= 101100
		, work(get_executor())
#else
		, work(std::make_shared<asio::io_service::work>(*this))
#endif
#endif
	{}
	virtual ~service_pump() {stop_service();}

	object_type find(int id)
	{
		std::lock_guard<std::mutex> lock(service_can_mutex);
		auto iter = std::find_if(std::begin(service_can), std::end(service_can), [&](object_ctype& item) {return id == item->id();});
		return iter == std::end(service_can) ? nullptr : *iter;
	}

	void remove(object_type i_service_)
	{
		assert(nullptr != i_service_);

		std::unique_lock<std::mutex> lock(service_can_mutex);
		service_can.remove(i_service_);
		lock.unlock();

		stop_and_free(i_service_);
	}

	void remove(int id)
	{
		std::unique_lock<std::mutex> lock(service_can_mutex);
		auto iter = std::find_if(std::begin(service_can), std::end(service_can), [&](object_ctype& item) {return id == item->id();});
		if (iter != std::end(service_can))
		{
			auto i_service_ = *iter;
			service_can.erase(iter);
			lock.unlock();

			stop_and_free(i_service_);
		}
	}

	void clear()
	{
		container_type temp_service_can;

		std::unique_lock<std::mutex> lock(service_can_mutex);
		temp_service_can.splice(std::end(temp_service_can), service_can);
		lock.unlock();

		ascs::do_something_to_all(temp_service_can, [this](object_type& item) {this->stop_and_free(item);});
	}

	void start_service(int thread_num = ASCS_SERVICE_THREAD_NUM) {if (!is_service_started()) do_service(thread_num);}
	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	void stop_service()
	{
		if (is_service_started())
		{
			end_service();
			wait_service();
		}
	}

	//if you add a service after start_service(), use this to start it
	void start_service(object_type i_service_, int thread_num = ASCS_SERVICE_THREAD_NUM)
	{
		assert(nullptr != i_service_);

		if (is_service_started())
			i_service_->start_service();
		else
			start_service(thread_num);
	}
	//doesn't like stop_service(), this will asynchronously stop service i_service_, you must NOT free i_service_ immediately,
	// otherwise, you may lead segment fault if you freed i_service_ before any async operation ends.
	//my suggestion is, DO NOT free i_service_, we can suspend it (by your own implementation and invocation rather than
	// stop_service(object_type)).
	//BTW, all i_services managed by this service_pump can be safely freed after stop_service().
	void stop_service(object_type i_service_) {assert(nullptr != i_service_); i_service_->stop_service();}

	//this function works like start_service() except that it will block until all services run out
	void run_service(int thread_num = ASCS_SERVICE_THREAD_NUM)
	{
		if (!is_service_started())
		{
			do_service(thread_num - 1);
			run();
			wait_service();
		}
	}

	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	//only for service pump started by 'run_service', this function will return immediately,
	//only the return from 'run_service' means service pump ended.
	void end_service()
	{
		if (is_service_started())
		{
#ifdef ASCS_AVOID_AUTO_STOP_SERVICE
			work.reset();
#endif
			do_something_to_all([](object_type& item) {item->stop_service();});
		}
	}

	bool is_running() const {return !stopped();}
	bool is_service_started() const {return started;}

	void add_service_thread(int thread_num) {for (auto i = 0; i < thread_num; ++i) service_threads.emplace_back([this]() {this->run();});}
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	void del_service_thread(int thread_num) {if (thread_num > 0) {del_thread_num += thread_num;}}
	int service_thread_num() const {return real_thread_num;}
#endif

protected:
	void do_service(int thread_num)
	{
		started = true;
		unified_out::info_out("service pump started.");

#if ASIO_VERSION >= 101100
		restart(); //this is needed when restart service
#else
		reset(); //this is needed when restart service
#endif
		do_something_to_all([](object_type& item) {item->start_service();});
		add_service_thread(thread_num);
	}

	void wait_service()
	{
		ascs::do_something_to_all(service_threads, [](std::thread& t) {t.join();});
		service_threads.clear();

		started = false;
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
		del_thread_num = 0;
#endif
		unified_out::info_out("service pump end.");
	}

	void stop_and_free(object_type i_service_)
	{
		assert(nullptr != i_service_);

		i_service_->stop_service();
		free(i_service_);
	}
	virtual void free(object_type i_service_) {} //if needed, rewrite this to free the service

#ifndef ASCS_NO_TRY_CATCH
	virtual bool on_exception(const std::exception& e)
	{
		unified_out::error_out("service pump exception: %s.", e.what());
		return true; //continue, if needed, rewrite this to decide whether to continue or not
	}
#endif

#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	size_t run()
	{
		size_t n = 0;

		std::stringstream os;
		os << "service thread[" << std::this_thread::get_id() << "] begin.";
		unified_out::info_out(os.str().data());
		++real_thread_num;
		while (true)
		{
			if (del_thread_num > 0)
			{
				if (--del_thread_num >= 0)
				{
					if (--real_thread_num > 0) //forbid to stop all service thread
						break;
					else
						++real_thread_num;
				}
				else
					++del_thread_num;
			}

			//we cannot always decrease service thread timely (because run_one can block).
			size_t this_n = 0;
#ifdef ASCS_NO_TRY_CATCH
			this_n = asio::io_context::run_one();
#else
			try {this_n = asio::io_context::run_one();} catch (const std::exception& e) {if (!on_exception(e)) break;}
#endif
			if (this_n > 0)
				n += this_n; //n can overflow, please note.
			else
			{
				--real_thread_num;
				break;
			}
		}
		os.str("");
		os << "service thread[" << std::this_thread::get_id() << "] end.";
		unified_out::info_out(os.str().data());

		return n;
	}
#elif !defined(ASCS_NO_TRY_CATCH)
	size_t run() {while (true) {try {return asio::io_context::run();} catch (const std::exception& e) {if (!on_exception(e)) return 0;}}}
#endif

	DO_SOMETHING_TO_ALL_MUTEX(service_can, service_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(service_can, service_can_mutex)

private:
	void add(object_type i_service_)
	{
		assert(nullptr != i_service_);

		std::unique_lock<std::mutex> lock(service_can_mutex);
		service_can.emplace_back(i_service_);
		lock.unlock();

		if (is_service_started())
			unified_out::warning_out("service been added, please remember to call start_service for it!");
	}

private:
	bool started;
	container_type service_can;
	std::mutex service_can_mutex;
	std::list<std::thread> service_threads;

#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	std::atomic_int_fast32_t real_thread_num;
	std::atomic_int_fast32_t del_thread_num;
#endif

#ifdef ASCS_AVOID_AUTO_STOP_SERVICE
#if ASIO_VERSION >= 101100
	asio::executor_work_guard<executor_type> work;
#else
	std::shared_ptr<asio::io_service::work> work;
#endif
#endif
};

} //namespace

#endif /* _ASCS_SERVICE_PUMP_H_ */
