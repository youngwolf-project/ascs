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

class service_pump
{
public:
	class i_service
	{
	protected:
		i_service(service_pump& service_pump_) : sp(service_pump_), started_(false), id_(0), data(nullptr) {sp.add(this);}
		virtual ~i_service() {sp.remove(this);}

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
		virtual void finalize() {} //clean up after stop_service

	protected:
		friend service_pump;
		service_pump& sp;

	private:
		bool started_;
		int id_;
		void* data; //magic data, you can use it in any way
	};

protected:
	struct context
	{
		asio::io_context io_context;
		unsigned refs;
#ifdef ASCS_AVOID_AUTO_STOP_SERVICE
#if ASIO_VERSION > 101100
		asio::executor_work_guard<asio::io_context::executor_type> work;
#else
		std::shared_ptr<asio::io_context::work> work;
#endif
#endif
		std::list<std::thread> threads;

#if ASIO_VERSION >= 101200
		context(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) : io_context(concurrency_hint), refs(0)
#else
		context() : refs(0)
#endif
#ifdef ASCS_AVOID_AUTO_STOP_SERVICE
#if ASIO_VERSION > 101100
			, work(io_context.get_executor())
#else
			, work(std::make_shared<asio::io_context::work>(io_context))
#endif
#endif
		{}
	};

public:
	typedef i_service* object_type;
	typedef const object_type object_ctype;
	typedef std::list<object_type> container_type;

#if ASIO_VERSION >= 101200
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	service_pump(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) : started(false), first(true), real_thread_num(0), del_thread_num(0), single_ctx(true)
		{context_can.emplace_back(concurrency_hint);}
#else
	//basically, the parameter multi_ctx is designed to be used by single_service_pump, which means single_service_pump always think it's using multiple io_context
	//for service_pump, you should use set_io_context_num function instead if you really need multiple io_context.
	service_pump(int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE, bool multi_ctx = false) : started(false), first(true), single_ctx(!multi_ctx)
		{context_can.emplace_back(concurrency_hint);}
	bool set_io_context_num(int io_context_num, int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE) //call this before construct any services on this service_pump
	{
		if (io_context_num < 1 || is_service_started() || context_can.size() > 1) //can only be called once
			return false;

		for (auto i = 1; i < io_context_num; ++i)
			context_can.emplace_back(concurrency_hint);
		if (context_can.size() > 1)
			single_ctx = false;

		return true;
	}
#endif
#else
#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	service_pump() : started(false), first(true), real_thread_num(0), del_thread_num(0), single_ctx(true), context_can(1) {}
#else
	//basically, the parameter multi_ctx is designed to be used by single_service_pump, which means single_service_pump always think it's using multiple io_context
	//for service_pump, you should use set_io_context_num function instead if you really need multiple io_context.
	service_pump(bool multi_ctx = false) : started(false), first(true), single_ctx(!multi_ctx), context_can(1) {}
	bool set_io_context_num(int io_context_num) //call this before construct any services on this service_pump
	{
		if (io_context_num < 1 || is_service_started() || context_can.size() > 1) //can only be called once
			return false;

		context_can.resize(io_context_num);
		if (context_can.size() > 1)
			single_ctx = false;

		return true;
	}
#endif
#endif
	virtual ~service_pump() {stop_service();}

	int get_io_context_num() const {return (int) context_can.size();}
	void get_io_context_refs(std::list<unsigned>& refs)
		{if (!single_ctx) ascs::do_something_to_all(context_can, context_can_mutex, [&](context& item) {refs.push_back(item.refs);});}

	//do not call below function implicitly or explicitly, before 1.6, a service_pump is also an io_context, so we already have it implicitly,
	// but in 1.6 and later, a service_pump is not an io_context anymore, then it is just provided to accommodate legacy usage in class
	// (unix_)server_base, (unix_)server_socket_base, (unix_)client_socket_base, udp::(unix_)socket_base and their subclasses.
	//according to the implementation, it picks the io_context who has the least references, so the return values are not consistent.
	operator asio::io_context& () {return assign_io_context();}

	asio::io_context& assign_io_context(bool increase_ref = true) //pick the context which has the least references
	{
		if (single_ctx)
			return context_can.front().io_context;

		context* ctx = nullptr;
		unsigned refs = 0;

		std::lock_guard<std::mutex> lock(context_can_mutex);
		ascs::do_something_to_one(context_can, [&](context& item) {
			if (0 == item.refs || 0 == refs || refs > item.refs)
			{
				refs = item.refs;
				ctx = &item;
			}

			return 0 == item.refs;
		});

		if (nullptr != ctx)
		{
			if (increase_ref)
				++ctx->refs;

			return ctx->io_context;
		}

		throw std::runtime_error("no available io_context");
	}

	void return_io_context(const asio::execution_context& io_context, unsigned refs = 1)
	{
		if (!single_ctx)
			ascs::do_something_to_one(context_can, context_can_mutex, [&](context& item) {return &io_context != &item.io_context ? false : (item.refs -= refs, true);});
	}
	void assign_io_context(const asio::execution_context& io_context, unsigned refs = 1)
	{
		if (!single_ctx)
			ascs::do_something_to_one(context_can, context_can_mutex, [&](context& item) {return &io_context != &item.io_context ? false : (item.refs += refs, true);});
	}

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

	//stop io_context directly, call this only if the stop_service invocation cannot stop the io_context
	void stop() {ascs::do_something_to_all(context_can, [&](context& item) {item.io_context.stop();});}

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
			do_service(thread_num, true);
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
			ascs::do_something_to_all(context_can, [](context& item) {item.work.reset();});
#endif
			do_something_to_all([](object_type& item) {item->stop_service();});
		}
	}

	bool is_running() const
	{
		auto running = false;
		ascs::do_something_to_one(context_can, [&](const context& item) {return (running = !item.io_context.stopped());});
		return running;
	}

	bool is_service_started() const {return started;}
	bool is_first_running() const {return first;}

	//not thread safe
#if ASIO_VERSION >= 101200
	void add_service_thread(int thread_num, bool block = false, int io_context_num = 0, int concurrency_hint = ASIO_CONCURRENCY_HINT_SAFE)
#else
	void add_service_thread(int thread_num, bool block = false, int io_context_num = 0)
#endif
	{
		if (io_context_num > 0)
		{
			if (thread_num < io_context_num)
			{
				unified_out::error_out("thread_num must be bigger than or equal to io_context_num.");
				return;
			}
			else
			{
				single_ctx = false;
				std::lock_guard<std::mutex> lock(context_can_mutex);
#if ASIO_VERSION >= 101200
				for (int i = 0; i < io_context_num; ++i)
					context_can.emplace_back(concurrency_hint);
#else
				context_can.resize((size_t) io_context_num + context_can.size());
#endif
			}
		}

		for (auto i = 0; i < thread_num; ++i)
		{
			auto ctx = assign_thread();
			if (nullptr == ctx)
				unified_out::error_out("no available io_context!");
			else if (block && i + 1 == thread_num)
				run(ctx); //block at here
			else
				ctx->threads.emplace_back([this, ctx]() {this->run(ctx);});
		}
	}

#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	void del_service_thread(int thread_num) {if (thread_num > 0) del_thread_num += thread_num;}
	int service_thread_num() const {return real_thread_num;}
#endif

protected:
	void do_service(int thread_num, bool block = false)
	{
		if (thread_num <= 0 || (size_t) thread_num < context_can.size())
		{
			unified_out::error_out("thread_num must be bigger than or equal to io_context_num.");
			return;
		}

#ifdef ASCS_AVOID_AUTO_STOP_SERVICE
		if (!is_first_running())
			ascs::do_something_to_all(context_can, [](context& item) {
#if ASIO_VERSION > 101100
				(&item.work)->~executor_work_guard();
				new(&item.work) asio::executor_work_guard<asio::io_context::executor_type>(item.io_context.get_executor());
#else
				item.work = std::make_shared<asio::io_context::work>(item.io_context);
#endif
			});
#endif

		started = true;
		unified_out::info_out("service pump started.");

#if ASIO_VERSION >= 101100
		ascs::do_something_to_all(context_can, [](context& item) {item.io_context.restart();}); //this is needed when restart service
#else
		ascs::do_something_to_all(context_can, [](context& item) {item.io_context.reset();}); //this is needed when restart service
#endif
		do_something_to_all([](object_type& item) {item->start_service();});
		add_service_thread(thread_num, block);
	}

	void wait_service()
	{
		ascs::do_something_to_all(context_can, [](context& item) {ascs::do_something_to_all(item.threads, [](std::thread& t) {t.join();}); item.threads.clear();});
		do_something_to_all([](object_type& item) {item->finalize();});

		started = first = false;
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

	size_t run(context* ctx)
	{
		size_t n = 0;

		std::stringstream os;
		os << "service thread[" << std::this_thread::get_id() << "] begin.";
		unified_out::info_out(os.str().data());

#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
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
			this_n = ctx->io_context.run_one();
#else
			try {this_n = ctx->io_context.run_one();} catch (const std::exception& e) {if (!on_exception(e)) break;}
#endif
			if (this_n > 0)
				n += this_n; //n can overflow, please note.
			else
			{
				--real_thread_num;
				break;
			}
		}
#elif defined(ASCS_NO_TRY_CATCH)
		n += ctx->io_context.run();
#else
		while (true) try {n += ctx->io_context.run(); break;} catch (const std::exception& e) {if (!on_exception(e)) break;}
#endif
		os.str("");
		os << "service thread[" << std::this_thread::get_id() << "] end.";
		unified_out::info_out(os.str().data());

		return n;
	}

	DO_SOMETHING_TO_ALL_MUTEX(service_can, service_can_mutex, std::lock_guard<std::mutex>)
	DO_SOMETHING_TO_ONE_MUTEX(service_can, service_can_mutex, std::lock_guard<std::mutex>)

private:
	context* assign_thread() //pick the context which has the least threads
	{
		context* ctx = nullptr;
		size_t num = 0;

		ascs::do_something_to_one(context_can, [&](context& item) {
			auto this_num = item.threads.size();
			if (0 == this_num || 0 == num || num > this_num)
			{
				num = this_num;
				ctx = &item;
			}

			return 0 == this_num;
		});

		return ctx;
	}

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
	bool started, first;
	container_type service_can;
	std::mutex service_can_mutex;

#ifdef ASCS_DECREASE_THREAD_AT_RUNTIME
	std::atomic_int_fast32_t real_thread_num;
	std::atomic_int_fast32_t del_thread_num;
#endif

	bool single_ctx;
	std::list<context> context_can;
	std::mutex context_can_mutex;
};

} //namespace

#endif /* _ASCS_SERVICE_PUMP_H_ */
