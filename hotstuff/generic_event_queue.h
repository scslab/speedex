#pragma once

#include "utils/async_worker.h"

#include <vector>

namespace hotstuff {

template<typename EventT>
class GenericEventQueue : public speedex::AsyncWorker {
	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	std::vector<EventT> events;

	virtual void on_event(EventT& e) = 0;

	void run_events(std::vector<EventT>& es) {
		for (auto& e : es) {
			on_event(e);
		}
	}

	bool exists_work_to_do() override final {
		return events.size() > 0;
	}

	void run();

protected:

	GenericEventQueue()
		: speedex::AsyncWorker()
		, events()
		{
			speedex::AsyncWorker::start_async_thread([this] () {run();});
		}

	~GenericEventQueue() {
		speedex::AsyncWorker::wait_for_async_task();
		end_async_thread();
	}

	void add_event_(EventT const& e) {
		std::lock_guard lock(mtx);
		events.push_back(e);
		cv.notify_all();
	}

	void add_events_(std::vector<EventT> const& es) {
		std::lock_guard lock(mtx);
		events.insert(
			events.end(),
			es.begin(),
			es.end());
		cv.notify_all();
	}
};

template<typename EventT>
void 
GenericEventQueue<EventT>::run() {
	while(true)
	{
		std::vector<EventT> work_list;
		{
			std::unique_lock lock(mtx);
			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(
					lock, 
					[this] () {
						return done_flag || exists_work_to_do();
					});
			}
			if (done_flag) return;
			work_list = std::move(events);
			events.clear();
			cv.notify_all();
		}
		run_events(work_list);
	}
}

} /* hotstuff */