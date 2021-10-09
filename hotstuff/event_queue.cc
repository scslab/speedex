#include "hotstuff/event_queue.h"

#include "hotstuff/consensus.h"

namespace hotstuff {

bool 
EventQueue::exists_work_to_do() {
	return events.size() != 0;
}

void
EventQueue::run_events(std::vector<Event>& work_list) {
	for (auto& event : work_list) {
		event(core);
	}
}

EventQueue::EventQueue(HotstuffCore& core)
	: AsyncWorker()
	, events()
	, core(core)
	{
		start_async_thread([this] () {run();});
	}

EventQueue::~EventQueue() {
	wait_for_async_task();
	end_async_thread();
}

void
EventQueue::validate_and_add_event(Event&& e)
{
	if (!e.validate(core.get_config())) {
		return;
	}

	wait_for_async_task();
	std::lock_guard lock(mtx);
	events.emplace_back(std::move(e));
	cv.notify_one();
}

void
EventQueue::run()
{
	while(true)
	{
		std::vector<Event> work_list;
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
			cv.notify_one();
		}
		run_events(work_list);
	}
}

} /* hotstuff */