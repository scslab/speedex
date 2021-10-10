#pragma once

#include "hotstuff/event.h"
#include "hotstuff/generic_event_queue.h"

#include <vector>

namespace hotstuff {

class HotstuffCore;

//! Handles events on the Hotstuff state machine
class EventQueue: public GenericEventQueue<Event>
{

	HotstuffCore& core;
	void on_event(Event& e) override final;

public:

	EventQueue(HotstuffCore& core);

	void validate_and_add_event(Event&& e);
};

/*
class EventQueue : public speedex::AsyncWorker {
	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	std::vector<Event> events;

	HotstuffCore& core;

	bool exists_work_to_do() override final;
	void run_events(std::vector<Event>& work_list);

	void run();

public:


	EventQueue(HotstuffCore& core);

	~EventQueue();

	void validate_and_add_event(Event&& e);
}; */

} /* hotstuff */