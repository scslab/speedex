#pragma once 


/*! \file async_worker.h
	Asynchronous background worker base class.
*/

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace speedex
{

/*!
  Generic class for running asynchronous tasks on background threads.
  Extend class to use.

  Extensions should ensure that they clean up any threads that they create
  in their destructors.
*/
class AsyncWorker {

protected:
	mutable std::mutex mtx;
	std::condition_variable cv;
	//! Flag for signaling worker thread to terminate
	std::atomic<bool> done_flag = false;

private:
	//! Flag for signaling that worker thread has shut down
	std::atomic<bool> worker_shutdown = false;

	bool started = false;
	bool terminate_correctly = false;

	void wait_for_async_thread_terminate() {
		std::unique_lock lock(mtx);
		if (!worker_shutdown) {
			cv.wait(lock, [this] () -> bool { return worker_shutdown;});
		}
	}

	//! call in dtor of extender
	void end_async_thread() {
		std::lock_guard lock(mtx);
		done_flag = true;
		cv.notify_all();
	}

	void run_wrapper(auto run_lambda)
	{
		run_lambda();
		signal_async_thread_shutdown();
	}

	//! call when worker thread terminates
	void signal_async_thread_shutdown() {
		std::lock_guard lock(mtx);
		worker_shutdown = true;
		cv.notify_all();		
	}

protected:

	//! extenders override this to tell worker when there's work waiting to be done.
	virtual bool exists_work_to_do() = 0;

public:

	//! call in ctor of derived class
	//! can't pass in lambda in ctor if lambda depends on
	//! data in derived class that hasn't yet been initialized
	void start_async_thread(auto run_lambda)
	{
		if (started)
		{
			throw std::runtime_error("double start on async worker");
		}
		std::thread([this, run_lambda] {run_wrapper(run_lambda); }).detach();
		started = true;
	}

	//! wait for background task to finish
	//! cannot be called in dtor of this class because of virtual call
	//! in most cases will be called in dtor of derived class
	void wait_for_async_task() {
		std::unique_lock lock(mtx);
		if (!exists_work_to_do()) return;
		cv.wait(lock, [this] {return !exists_work_to_do();});
	}

protected:

	// must be called in dtor of derived class
	void terminate_worker()
	{
		wait_for_async_task();
		end_async_thread();
		wait_for_async_thread_terminate();
		terminate_correctly = true;
	}
	
	// http://www.gotw.ca/publications/mill18.htm
	~AsyncWorker()
	{
		if (!terminate_correctly)
		{
			std::printf("terminated async worker incorrectly\n");
			std::fflush(stdout);
			std::abort();
		}
	}
};

} /* speedex */
