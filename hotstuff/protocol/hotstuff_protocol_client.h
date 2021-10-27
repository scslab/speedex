#pragma once

#include "config/replica_config.h"

#include "rpc/rpcconfig.h"

#include "utils/nonblocking_rpc_client.h"

#include "xdr/hotstuff.h"

#include <memory>
#include <variant>
#include <vector>

#include <xdrpp/srpc.h>

namespace hotstuff {

class HotstuffProtocolClient : public speedex::NonblockingRpcClient<xdr::srpc_client<HotstuffProtocolV1>> {

	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	using vote_t = std::shared_ptr<VoteMessage>;
	using proposal_t = std::shared_ptr<ProposeMessage>;

	using msg_t = std::variant<vote_t, proposal_t>;

	std::vector<msg_t> work;

	void do_work(std::vector<msg_t> const& todo);

	void run();

	bool exists_work_to_do() override final {
		return work.size() > 0;
	}

public:

	HotstuffProtocolClient (speedex::ReplicaInfo const& info)
		: NonblockingRpcClient<xdr::srpc_client<HotstuffProtocolV1>>(info)
		, work()
		{
			start_async_thread([this] {run();});
		}

	~HotstuffProtocolClient() {
		wait_for_async_task();
		end_async_thread();
	}

	void
	propose(proposal_t proposal);

	void
	vote(vote_t vote);

	const char* get_service() const override final {
		return HOTSTUFF_PROTOCOL_PORT;
	}


};


} /* hotstuff */