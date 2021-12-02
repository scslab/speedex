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

	//! Flag for signaling that worker thread has shut down
	std::atomic<bool> worker_shutdown = false;

public:

	//! extenders override this to tell worker when there's work waiting to be done.
	virtual bool exists_work_to_do() = 0;

	//! wait for background task to finish
	void wait_for_async_task() {
		std::unique_lock lock(mtx);
		if (!exists_work_to_do()) return;
		cv.wait(lock, [this] {return !exists_work_to_do();});
	}

	//! call in ctor of extender
	void start_async_thread(auto run_lambda) {
		std::thread(run_lambda).detach();
	}

	//! call in dtor of extender
	void end_async_thread() {
		std::lock_guard lock(mtx);
		done_flag = true;
		cv.notify_all();
	}

	void signal_async_thread_shutdown() {
		std::lock_guard lock(mtx);
		worker_shutdown = true;
		cv.notify_all();		
	}

	void wait_for_async_thread_terminate() {
		std::unique_lock lock(mtx);
		if (!worker_shutdown) {
			cv.wait(lock, [this] () -> bool { return worker_shutdown;});
		}
	}
};

} /* speedex */
