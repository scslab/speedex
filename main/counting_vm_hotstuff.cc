#include "hotstuff/config/replica_config.h"
#include "config/replica_config.h"

#include "hotstuff/hotstuff_app.h"
#include "hotstuff/hotstuff_configs.h"
#include "hotstuff/liveness.h"
#include "generic/counting_vm.h"

//#include "xdr/hotstuff.h"

#include <optional>
#include <chrono>
#include <thread>

#include <getopt.h>
#include <libfyaml.h>

using namespace hotstuff;

using namespace std::chrono_literals;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: counting_vm_hotstuff --replica_id=<id> --config_file=<filename> --load_lmdb<?>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE,
	LOAD_FROM_LMDB
};

static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{"load_lmdb", no_argument, nullptr, LOAD_FROM_LMDB},
	{nullptr, 0, nullptr, 0}
};

int main(int argc, char **argv)
{
	std::optional<ReplicaID> self_id;
	std::string config_file;
	
	bool load_from_lmdb = false;

	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case OPT_REPLICA_ID:
				self_id = std::stol(optarg);
				break;
			case OPT_CONFIG_FILE:
				config_file = optarg;
				break;
			case LOAD_FROM_LMDB:
				load_from_lmdb = true;
				break;
			default:
				usage();
		}
	}

	if (!self_id) {
		usage();
	}

	if (config_file.empty()) {
		usage();
	}

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file.c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file.c_str());
		usage();
	}

	auto [config, sk] = speedex::parse_replica_config(fyd, *self_id);

	//auto sk = config.parse(fyd, *self_id);

	auto vm = std::make_shared<CountingVM>();

	auto app = make_speculative_hotstuff_instance(config, *self_id, sk, vm, HotstuffConfigs());

	//HotstuffApp app(config, *self_id, sk, vm);

	if (load_from_lmdb) {
		app->init_from_disk();
	} else {
		app->init_clean();
	}

	std::printf("finished initializing HotstuffApp\n");

	PaceMakerWaitQC pmaker(app);
	pmaker.set_self_as_proposer();

	std::printf("initialized pacemaker\n");

	while (true) {
		if (pmaker.should_propose()) {
			std::printf("attempting propose\n");
			app->put_vm_in_proposer_mode();
			pmaker.do_propose();
			pmaker.wait_for_qc();
		} else {
			std::printf("no propose\n");
		}
		std::this_thread::sleep_for(1000ms);
	}
}
