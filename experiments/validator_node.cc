#include "experiments/singlenode_init.h"
#include "experiments/validator_node.h"

#include "rpc/consensus_api_server.h"

#include "speedex/speedex_node.h"
#include "speedex/speedex_management_structures.h"

#include <tbb/global_control.h>

namespace speedex {

void 
SimulatedValidatorNode::run_experiment() {

	SpeedexManagementStructures management_structures(
		options.num_assets,
		ApproximationParameters {
			options.tax_rate,
			options.smooth_mult
		});

	tbb::global_control control(
		tbb::global_control::max_allowed_parallelism, num_threads);

	init_management_structures_from_lmdb(management_structures);

	SpeedexNode node(management_structures, params, options, results_output_root, NodeType::BLOCK_VALIDATOR);

	ConsensusApiServer consensus_api_server(node);
	
	consensus_api_server.set_experiment_ready_to_start();
	consensus_api_server.wait_for_experiment_start();

	node.get_block_forwarder().request_forwarding_from(parent_hostname);

	consensus_api_server.wait_until_upstream_finished();
	BLOCK_INFO("upstream finished");
	consensus_api_server.wait_until_block_buffer_empty();
	BLOCK_INFO("block buffer flushed");
	node.get_block_forwarder().shutdown_target_connections();
	BLOCK_INFO("target connections shutdown");
	consensus_api_server.set_experiment_done();
	BLOCK_INFO("experiment set to done");
	consensus_api_server.wait_for_experiment_start();
	BLOCK_INFO("got shutdown signal from controller, shutting down");

	node.write_measurements();
	BLOCK_INFO("shutting down");
	std::this_thread::sleep_for(std::chrono::seconds(5));
}

} /* speedex */
