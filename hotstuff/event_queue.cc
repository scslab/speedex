#include "hotstuff/event_queue.h"

#include "hotstuff/consensus.h"

namespace hotstuff {

EventQueue::EventQueue(HotstuffCore& core)
	: GenericEventQueue<Event>()
	, core(core)
	{}

void
EventQueue::validate_and_add_event(Event&& e)
{
	if (!e.validate(core.get_config())) {
		return;
	}
	add_event_(e);
}

void
EventQueue::on_event(Event& e)
{
	e(core);
}

} /* hotstuff */
