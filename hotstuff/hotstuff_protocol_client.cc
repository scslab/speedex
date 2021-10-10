#include "hotstuff/hotstuff_protocol_client.h"

namespace hotstuff {

void
HotstuffProtocolClient::propose(proposal_t proposal)
{
	std::lock_guard lock(mtx);
	work.emplace_back(proposal);
	cv.notify_all();
}

void
HotstuffProtocolClient::vote(vote_t vote)
{
	std::lock_guard lock(mtx);
	work.emplace_back(vote);
	cv.notify_all();
}

void HotstuffProtocolClient::do_work(std::vector<msg_t> const& todo)
{
	wait_for_try_open_connection();
	if (done_flag) return;

	for (auto const& work : todo)
	{
		bool success = false;
		while (!success) {
			try {
				// with ptr types, compiler had trouble getting an invoke_result on std::visit
				switch(work.index()) {
					case 0: // vote
						client -> vote(*std::get<0>(work));
						break;
					case 1: // proposal
						client -> propose(*std::get<1>(work));
						break;
					default:
						throw std::runtime_error("unknown call type");
				}
				success = true;
			} catch(...) {
				HOTSTUFF_INFO("error when sending nonblocking rpc");
			}

			if (!success) {
				wait_for_try_open_connection();
				if (done_flag) return;
			}
		}
	}
}

void
HotstuffProtocolClient::run()
{
	while(true) {
		std::vector<msg_t> todo;
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
			todo = std::move(work);
			work.clear();
			// used for shutdown wait
			cv.notify_one();
		}
		wait_for_try_open_connection();

		do_work(todo);
	}
}


} /* hotstuff */