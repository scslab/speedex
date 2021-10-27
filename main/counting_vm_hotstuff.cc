#include "config/replica_config.h"

#include "hotstuff/hotstuff.h"
#include "hotstuff/liveness.h"
#include "hotstuff/vm/counting_vm.h"

#include "xdr/hotstuff.h"

#include <optional>

#include <getopt.h>
#include <libfyaml.h>

using namespace hotstuff;
using namespace speedex;

using namespace std::chrono_literals;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: counting_vm_hotstuff --replica_id=<id> --config_file=<filename>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE
};

static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{nullptr, 0, nullptr, 0}
};

int main(int argc, char **argv)
{
	std::optional<ReplicaID> self_id;
	std::string config_file;
	
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

	ReplicaConfig config;

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file.c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file.c_str());
		usage();
	}

	auto sk = config.parse(fyd, *self_id);

	auto vm = std::make_shared<CountingVM>();

	HotstuffApp app(config, *self_id, sk, vm);

	PaceMakerWaitQC pmaker(app);

	while (true) {
		if (pmaker.should_propose()) {
			app.put_vm_in_proposer_mode();
			pmaker.do_propose();
			pmaker.wait_for_qc();
		}
		std::this_thread::sleep_for(1000ms);
	}
}
