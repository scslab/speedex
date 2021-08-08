#pragma once

#include "xdr/consensus_api.h"
#include "rpc/rpcconfig.h"
#include "xdr/database_commitments.h"
#include "utils/async_worker.h"

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <xdrpp/arpc.h>


namespace speedex {

class BlockForwarder : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	HashedBlock header_to_send;

	std::unique_ptr<AccountModificationBlock> block_to_send;
	std::unique_ptr<SerializedBlock> block_to_send2;

	using forwarding_client_t = xdr::srpc_client<BlockTransferV1>;
	using socket_t = xdr::unique_sock;
	using request_client_t = xdr::srpc_client<RequestBlockForwardingV1>;

	std::vector<std::unique_ptr<forwarding_client_t>> forwarding_targets;
	std::vector<socket_t> sockets;

	std::vector<std::string> shutdown_notification_hosts;

	std::atomic<size_t> num_forwarding_targets = 0;

	bool exists_work_to_do() override final {
		return (block_to_send != nullptr) ||
			(block_to_send2 != nullptr);
	}

	void send_block_(const HashedBlock& header, const SerializedBlock& serialized_data);

	void send_block_(const HashedBlock& header, const AccountModificationBlock& block);

	void run();

public:

	BlockForwarder()
		: AsyncWorker() {
			start_async_thread([this] {run();});
		}

	~BlockForwarder() {
		wait_for_async_task();
		end_async_thread();
	}

	void send_block(const HashedBlock& header, std::unique_ptr<AccountModificationBlock> block);
	void send_block(const HashedBlock& header, std::unique_ptr<SerializedBlock> block);

	void shutdown_target_connections();

	void add_forwarding_target(const std::string& hostname);

	void request_forwarding_from(const std::string& hostname);
};

} /* speedex */
