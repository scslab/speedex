#pragma once

#include "synthetic_data_generator/data_stream.h"

#include "hotstuff/config/replica_config.h"

#include "utils/nonblocking_rpc_client.h"

#include "rpc/rpcconfig.h"

#include "xdr/overlay.h"

#include <atomic>
#include <cstdint>
#include <optional>

#include <xdrpp/srpc.h>

namespace speedex {

class OverlayClient : public NonblockingRpcClient<xdr::srpc_client<OverlayV1>> {

	using client_t = xdr::srpc_client<OverlayV1>;

	std::atomic<uint64_t> foreign_mempool_size;
	std::atomic<uint64_t> local_buffer_size;
	std::atomic<bool> connected_to_foreign_mempool;

	using forward_t = DataBuffer;

	std::vector<forward_t> txs_to_forward;

	std::atomic<bool> force_repoll;

	bool exists_work_to_do() override final;

	void run();

	void on_connection_clear() override {
		connected_to_foreign_mempool = false;
	}

	void on_connection_open() override {
		connected_to_foreign_mempool = true;
	}

	ReplicaID self_id;

	const std::string port;

public:

	OverlayClient(const hotstuff::ReplicaInfo& info, ReplicaID self_id, const std::string& target_port = std::string(OVERLAY_PORT))
		: NonblockingRpcClient<client_t>(info)
		, foreign_mempool_size(0)
		, connected_to_foreign_mempool(false)
		, force_repoll(false)
		, self_id(self_id)
		, port(target_port)
		{
			start_async_thread([this] {run();});
		}

	std::optional<uint64_t> get_cached_foreign_mempool_size() const;

	void poll_foreign_mempool_size();

	void send_txs(forward_t txs);

	const char* get_service() const override final {
		return port.c_str();
	}

	~OverlayClient()
	{
		terminate_worker();
	}

};


} /* speedex */
