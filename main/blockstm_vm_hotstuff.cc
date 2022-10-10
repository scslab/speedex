#include "automation/command_line_args.h"
#include "automation/experiment_control.h"
#include "automation/get_experiment_vars.h"
#include "automation/get_replica_id.h"

#include "config/replica_config.h"

#include "hotstuff/hotstuff_app.h"
#include "hotstuff/liveness.h"

#include "speedex/speedex_options.h"
#include "speedex/speedex_static_configs.h"
#include "speedex/vm/speedex_vm.h"

#include "synthetic_data_generator/synthetic_data_gen.h"

#include "utils/manage_data_dirs.h"

#include "xdr/experiments.h"

#include <optional>

#include <libfyaml.h>

#include <tbb/global_control.h>

using namespace hotstuff;
using namespace speedex;

using namespace std::chrono_literals;

int main(int argc, char* const* argv)
{
	auto args = parse_cmd(argc, argv, "blockstm_vm_hotstuff");

	if (!args.self_id) {
		args.self_id = get_replica_id();
	}
	if (!args.config_file) {
		args.config_file = get_config_file();
	}

	if ((args.batch_size == 0 || args.num_accounts == 0) && (*args.self_id == 0))
	{
		throw std::runtime_error("failed to set options req'd for blockstm comparison");
	}

	if (MAX_SEQ_NUMS_PER_BLOCK < 2 * args.batch_size / args.num_accounts)
	{
		throw std::runtime_error("insufficient seqno buffer warning");
	}

	struct fy_document* fyd = fy_document_build_from_file(NULL, args.config_file->c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", args.config_file->c_str());
		exit(1);
	}

	auto [config, sk] = parse_replica_config(fyd, *args.self_id);

	fy_document_destroy(fyd);

	if (args.experiment_results_folder.size() == 0) {
		args.experiment_results_folder = get_experiment_results_folder();
	}

	if (args.experiment_options_file.size() == 0)
	{
		args.experiment_options_file = "synthetic_data_config/blockstm_base.yaml";
	}

	if (args.speedex_options_file.size() == 0)
	{
		args.speedex_options_file = "experiment_config/blockstm_params.yaml";
	}

	std::minstd_rand gen(0);
	GenerationOptions options;

	auto parsed = options.parse(args.experiment_options_file.c_str());
	if (!parsed) {
		throw std::runtime_error("failed to parse experiment options file");
	}

	options.num_accounts = args.num_accounts;
	options.block_size = args.batch_size;

	GeneratorState generator (gen, options, "experiment_data/blockstm_comparison_data/");

	ExperimentParameters params;

	params.num_assets = 1;
	params.default_amount = options.new_account_balance;
	params.account_list_filename = "blockstm_accounts";
	params.num_blocks = 100;
	params.n_replicas = config.nreplicas;

	if (options.num_assets != params.num_assets)
	{
		throw std::runtime_error("asset amount mismatch");
	}

	generator.dump_account_list(params.account_list_filename);

	SpeedexOptions speedex_options;
	speedex_options.parse_options(args.speedex_options_file.c_str());

	if (speedex_options.num_assets != params.num_assets) {
		throw std::runtime_error("mismatch in num assets between speedex_options and experiment_options");
	}

	if (config.nreplicas != params.n_replicas) {
		std::printf("WARNING: mismatch between experiment data sharding and num replicas\n");
	}

	speedex_options.block_size = args.batch_size;

	make_all_data_dirs(config.get_info(*args.self_id));

	auto configs = get_runtime_configs();

	size_t num_threads = get_num_threads();

	make_all_data_dirs(config.get_info(*args.self_id));

	auto vm = std::make_shared<SpeedexVM>(params, speedex_options, args.experiment_results_folder, configs);

	auto app = hotstuff::make_speculative_hotstuff_instance(config, *args.self_id, sk, vm);

	if (args.load_from_lmdb)
	{
		std::printf("no loading from lmdb in blockstm_vm_hotstuff experiments\n");
		exit(1);
	}
	app -> init_clean();

	std::vector<double> prices;
	std::vector<ExperimentBlock> blocks;

	if (*args.self_id == 0) {
		for (size_t i = 0; i < params.num_blocks; i++)
		{
			blocks.push_back(generator.make_block(prices));
		}
	}

	auto& mp = vm -> get_mempool();

	tbb::global_control control(
		tbb::global_control::max_allowed_parallelism, num_threads);

	std::string measurement_name_suffix = std::string("bstm_compare_nacc=") + std::to_string(args.num_accounts) + "_nbatch=" + std::to_string(args.batch_size);

	ExperimentController control_server(vm, measurement_name_suffix);
	control_server.wait_for_breakpoint_signal();

	PaceMakerWaitQC pmaker(app);

	if (*args.self_id == 0) {
		pmaker.set_self_as_proposer();
	}

	std::this_thread::sleep_for(2000ms);

	bool self_signalled_end = false;

	size_t idx = 0;
	while (true) {
		if (pmaker.should_propose()) {
			app->put_vm_in_proposer_mode();

			if (idx < blocks.size())
			{
				auto& block = blocks.at(idx);
				idx++;

				mp.chunkify_and_add_to_mempool_buffer(std::move(block));
				mp.push_mempool_buffer_to_mempool();
			}

			pmaker.do_propose();
			pmaker.wait_for_qc();
		} else {
			std::this_thread::sleep_for(1000ms);
		}

		/* Experiment control conditions */

		// conditions only activate for current producer
		if (vm -> experiment_is_done()) {
			app->stop_proposals();
			self_signalled_end = true;
		}
		if (app->proposal_buffer_is_empty()) {
			std::printf("done with experiment\n");

			auto measurements = vm -> get_measurements();
			for (auto const& b : measurements.block_results)
			{
				auto const& bcm = b.results.productionResults().block_creation_measurements;

				if (bcm.number_of_transactions != args.batch_size)
				{
					throw std::runtime_error("mismatch ntx != batch_size");
				}
			}

			//flush proposal buffers
			pmaker.do_empty_propose();
			pmaker.wait_for_qc();
			pmaker.do_empty_propose();
			pmaker.wait_for_qc();
			pmaker.do_empty_propose();
			pmaker.wait_for_qc();

			control_server.wait_for_breakpoint_signal();
			vm -> write_measurements();
			exit(0);
		}

		// conditions for validator nodes
		if (control_server.producer_is_done_signal_was_received() && (!self_signalled_end)) {
			std::printf("leader terminated experiment, waiting for signal\n");
			control_server.wait_for_breakpoint_signal();
			vm -> write_measurements();
			exit(0);
		}
	}
}
