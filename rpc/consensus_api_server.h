#pragma once

#include "speedex/speedex_node.h"
#include "rpc/rpcconfig.h"
#include "rpc/consensus_api.h"
#include "xdr/consensus_api.h"

#include <xdrpp/arpc.h>
#include <xdrpp/srpc.h>
#include <xdrpp/pollset.h>


namespace speedex {

class ConsensusApiServer {

	using BlockTransfer = BlockTransferV1_server;
	using RequestBlockForwarding = RequestBlockForwardingV1_server;
	using ExperimentControl = ExperimentControlV1_server;

	BlockTransfer transfer_server;
	RequestBlockForwarding req_server;
	ExperimentControl control_server;

	xdr::pollset ps;

	xdr::srpc_tcp_listener<> bt_listener;
	xdr::srpc_tcp_listener<rpc_sock_ptr> req_listener;
	xdr::srpc_tcp_listener<> control_listener;

public:

	ConsensusApiServer(SpeedexNode& main_node);

	void wait_for_experiment_start() {
		control_server.wait_for_start();
	}

	void set_experiment_done() {
		control_server.set_experiment_done();
	}

	void set_experiment_ready_to_start() {
		control_server.set_experiment_ready_to_start();
	}

	void wait_until_block_buffer_empty() {
		transfer_server.wait_until_block_buffer_empty();
	}

	void wait_until_upstream_finished() {
		control_server.wait_for_upstream_finish();
	}

};

} /* edce */