#pragma once

#include "utils/async_worker.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <xdrpp/types.h>

namespace hotstuff {

class HotstuffLMDB;

template<typename VMType>
class VMControlInterface : public speedex::AsyncWorker {
	std::shared_ptr<VMType> vm_instance;

	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	using proposal_buffer_t = std::unique_ptr<typename VMType::block_type>;
	using submission_t = std::unique_ptr<typename VMType::block_type>;

	size_t PROPOSAL_BUFFER_TARGET = 3;

	std::vector<submission_t> blocks_to_validate;

	std::vector<proposal_buffer_t> proposal_buffer;

	size_t additional_proposal_requests;

	bool is_proposer;

	std::optional<typename VMType::block_id> highest_committed_id;

	void clear_proposal_settings();

	//vm runner side
	void run();

	bool exists_work_to_do() override final {
		return (blocks_to_validate.size() > 0)
			|| ((additional_proposal_requests > 0) && is_proposer)
			|| ((bool) highest_committed_id);
	}

public:

	VMControlInterface(std::shared_ptr<VMType> vm)
		: speedex::AsyncWorker()
		, vm_instance(vm)
		, blocks_to_validate()
		, proposal_buffer()
		, additional_proposal_requests(0)
		, is_proposer(false)
		, highest_committed_id(std::nullopt)
		{
			start_async_thread([this] {run();});
		}

	~VMControlInterface() {
		end_async_thread();
	}

	// from hotstuff side
	void set_proposer();
	proposal_buffer_t get_proposal();
	void submit_block_for_exec(submission_t submission);
	void log_commitment(typename VMType::block_id block_id);
	void finish_work_and_force_rewind();

	//call before any usage
	void init_clean() {
		vm_instance -> init_clean();
	}
	void init_from_disk(HotstuffLMDB const& decided_block_cache) {
		vm_instance -> init_from_disk(decided_block_cache);
	}
	bool proposal_buffer_is_empty() const {
		std::lock_guard lock(mtx);
		return proposal_buffer.empty() && (PROPOSAL_BUFFER_TARGET == 0) && (additional_proposal_requests == 0);
	}
	void stop_proposals() {
		std::lock_guard lock(mtx);
		PROPOSAL_BUFFER_TARGET = 0;
	}
};

template<typename VMType>
void
VMControlInterface<VMType>::set_proposer()
{
	std::lock_guard lock(mtx);
	HOTSTUFF_INFO("VM INTERFACE: entering proposer mode");
	is_proposer = true;
}

template<typename VMType>
typename VMControlInterface<VMType>::proposal_buffer_t
VMControlInterface<VMType>::get_proposal()
{
	std::unique_lock lock(mtx);
	HOTSTUFF_INFO("VM INTERFACE: start get_proposal");
	if (!is_proposer) {
		HOTSTUFF_INFO("VM INTERFACE: not in proposer mode, returning empty proposal");
		return nullptr;
	}

	if (done_flag) return nullptr;

	if (proposal_buffer.empty()) {
		if (additional_proposal_requests < PROPOSAL_BUFFER_TARGET) {
			additional_proposal_requests = PROPOSAL_BUFFER_TARGET;
		}
		if (PROPOSAL_BUFFER_TARGET == 0) {
			HOTSTUFF_INFO("VM INTERFACE: cannot get proposal from empty buffer after proposals are stopped");
			return nullptr;
		}
		cv.notify_all();
		HOTSTUFF_INFO("VM INTERFACE: waiting on new proposal from vm");
		cv.wait(lock, [this] { return done_flag || (!proposal_buffer.empty());});
	}

	if (done_flag) return nullptr;

	HOTSTUFF_INFO("VM INTERFACE: got new proposal from vm");

	auto out = std::move(proposal_buffer.front());
	proposal_buffer.erase(proposal_buffer.begin());

	additional_proposal_requests 
		= (proposal_buffer.size() > PROPOSAL_BUFFER_TARGET)
			? 0 
			: PROPOSAL_BUFFER_TARGET - proposal_buffer.size();
	if (additional_proposal_requests > 0) {
		cv.notify_all();
	}
	return out;
}

template<typename VMType>
void
VMControlInterface<VMType>::clear_proposal_settings() {
	is_proposer = false;
	proposal_buffer.clear();
	additional_proposal_requests = 0;
}

template<typename VMType>
void
VMControlInterface<VMType>::submit_block_for_exec(submission_t submission)
{
	std::lock_guard lock(mtx);
	
	clear_proposal_settings();

	blocks_to_validate.push_back(std::move(submission));

	cv.notify_all();
}

//! log_commitment should be called in order in which things are committed
template<typename VMType>
void
VMControlInterface<VMType>::log_commitment(typename VMType::block_id block_id) {
	std::lock_guard lock(mtx);

	if (block_id) {
		highest_committed_id = block_id;
	}
	if (highest_committed_id) {
		cv.notify_all();
	}

	/*if (highest_committed_id) {
		*highest_committed_id = std::max(*highest_committed_id, block_id);
	} else {
		highest_committed_id = std::make_optional(block_id);
	}*/
}


template<typename VMType>
void
VMControlInterface<VMType>::run() {
	HOTSTUFF_INFO("VM INTERFACE: start run()");
	while(true) {
		std::unique_lock lock(mtx);

		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(
				lock, 
				[this] () {
					return done_flag || exists_work_to_do();
				});
		}

		if (done_flag) return;

		while (!blocks_to_validate.empty()) {
			auto block_to_validate = std::move(blocks_to_validate.front());
			blocks_to_validate.erase(blocks_to_validate.begin());

			//block_to_validate is nullptr iff parsing failed.
			//In this case, vm doesn't do anything.
			if (block_to_validate) {
				// exec_block is responsible for reverting any speculative state
				// left over from calls to propose()
				HOTSTUFF_INFO("VM INTERFACE: start exec_block");
				vm_instance -> exec_block(*block_to_validate);
				HOTSTUFF_INFO("VM INTERFACE: end exec block");
			}
		} 
		if (highest_committed_id) {
			vm_instance -> log_commitment(*highest_committed_id);
			highest_committed_id = std::nullopt;
		}
		if (additional_proposal_requests > 0) {
			//get additional proposals
			if (!is_proposer) {
				throw std::runtime_error("vm thread woke up early");
			}

			// vm->propose() leaves vm in speculative future state.
			proposal_buffer.emplace_back(vm_instance -> propose());
			additional_proposal_requests--;
		}
		cv.notify_all();
	}
}

template<typename VMType>
void
VMControlInterface<VMType>::finish_work_and_force_rewind() {
	wait_for_async_task();
	vm_instance -> rewind_to_last_commit();
}


} /* hotstuff */
