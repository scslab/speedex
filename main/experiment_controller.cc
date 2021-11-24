#include "automation/get_experiment_vars.h"

#include "config/replica_config.h"

#include "rpc/rpcconfig.h"

#include "utils/save_load_xdr.h"

#include "xdr/consensus_api.h"

#include <optional>
#include <string>

#include <getopt.h>
#include <libfyaml.h>

#include <xdrpp/srpc.h>

using namespace speedex;

using namespace std::chrono_literals;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: experiment_controller --config_file=<filename, required>
                             --output_folder=<filename, required>
)");
	exit(1);
}

enum opttag {
	OPT_CONFIG_FILE = 0x100,
	OUTPUT_FOLDER
};

static const struct option opts[] = {
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{"output_folder", required_argument, nullptr, OUTPUT_FOLDER},
	{nullptr, 0, nullptr, 0}
};

bool node_is_online(const ReplicaInfo& info) {
	try {
		std::printf("querying to see if node %s is ready\n", info.hostname.c_str());
		auto fd = xdr::tcp_connect(info.hostname.c_str(), EXPERIMENT_CONTROL_PORT);
		auto client = xdr::srpc_client<HotstuffVMControlV1>(fd.get());
		return true;
	} catch(...) {
		std::printf("node %s is not yet responding to messages\n", info.hostname.c_str());
		return false;
	}
}

void wait_for_all_online(const ReplicaConfig& config) {
	auto infos = config.list_info();
	while(true) {
		bool failed = false;
		for (auto& info : infos) {
			if (!node_is_online(info)) {
				failed = true;
			}
		}
		if (!failed) {
			return;
		}
		std::this_thread::sleep_for(1000ms);
	}
}

bool send_one_breakpoint(const ReplicaInfo& info) {
	try {
		auto fd = xdr::tcp_connect(info.hostname.c_str(), EXPERIMENT_CONTROL_PORT);
		auto client = xdr::srpc_client<HotstuffVMControlV1>(fd.get());
		client.signal_breakpoint();
		return true;
	} catch(...) {
		std::printf("node %s is not responding to messages\n", info.hostname.c_str());
		return false;
	}
}

void send_all_breakpoints(const ReplicaConfig& config) {
	auto infos = config.list_info();
	for (auto& info : infos) {
		while (!send_one_breakpoint(info)) {}
	}
}

bool experiment_done(const ReplicaInfo& info) {
	try {
		std::printf("querying to see if node %s marked experiment done\n", info.hostname.c_str());
		auto fd = xdr::tcp_connect(info.hostname.c_str(), EXPERIMENT_CONTROL_PORT);
		auto client = xdr::srpc_client<HotstuffVMControlV1>(fd.get());
		auto res = client.experiment_is_done();
		if (!res) {
			return false;
		}
		return *res;
	} catch(...) {
		std::printf("node %s is not responding to messages\n", info.hostname.c_str());
		return false;
	}
}

void wait_for_one_experiment_done(const ReplicaConfig& config) {
	auto infos = config.list_info();
	while(true) {
		bool finished = false;
		for (auto& info : infos) {
			if (experiment_done(info)) {
				finished = true;
			}
		}
		if (finished) {
			return;
		}
		std::this_thread::sleep_for(1000ms);
	}
}

bool send_experiment_done_signal(const ReplicaInfo& info) {
	try {
		std::printf("querying to see if node %s marked experiment done\n", info.hostname.c_str());
		auto fd = xdr::tcp_connect(info.hostname.c_str(), EXPERIMENT_CONTROL_PORT);
		auto client = xdr::srpc_client<HotstuffVMControlV1>(fd.get());
		client.send_producer_is_done_signal();
		return true;
	} catch(...) {
		std::printf("node %s is not responding to messages\n", info.hostname.c_str());
		return false;
	}
}

void
send_all_experiment_done_signals(const ReplicaConfig& config) {
	auto infos = config.list_info();
	while(true) {
		bool failed = false;
		for (auto& info : infos) {
			if (!send_experiment_done_signal(info)) {
				failed = true;
			}
		}
		if (!failed) {
			return;
		}
		std::this_thread::sleep_for(1000ms);
	}
}

std::optional<uint64_t>
get_num_blocks(const ReplicaInfo& info) {
	try {
		std::printf("querying to see if node %s marked experiment done\n", info.hostname.c_str());
		auto fd = xdr::tcp_connect(info.hostname.c_str(), EXPERIMENT_CONTROL_PORT);
		auto client = xdr::srpc_client<HotstuffVMControlV1>(fd.get());
		auto res = client.get_speedex_block_height();
		if (!res) {
			return std::nullopt;
		}
		return {*res};
	} catch(...) {
		std::printf("node %s is not responding to messages\n", info.hostname.c_str());
		return std::nullopt;
	}
}

void
wait_for_all_same_height(const ReplicaConfig& config) {
	auto infos = config.list_info();
	while(true) {
		std::optional<uint64_t> height;
		bool comm_failure = false;
		bool mismatch = false;
		for (auto& info : infos) {
			auto res = get_num_blocks(info);
			if (!res) {
				comm_failure = true;
				break;
			}
			if (!height) {
				height = res;
			} else {
				if (*height != *res) {
					mismatch = true;
				}
			}
		}
		if ((!comm_failure) && (!mismatch)) {
			return;
		}
		std::this_thread::sleep_for(1000ms);
	}
}

std::string get_measurements_filename(std::string const& folder, ReplicaInfo const& info) {
	return folder + std::string("measurements_") + std::to_string(info.id);
}

bool save_measurement(std::string const& folder, const ReplicaInfo& info) {
	try {
		std::printf("querying to see if node %s marked experiment done\n", info.hostname.c_str());
		auto fd = xdr::tcp_connect(info.hostname.c_str(), EXPERIMENT_CONTROL_PORT);
		auto client = xdr::srpc_client<HotstuffVMControlV1>(fd.get());
		client.write_measurements();
		auto measurements = client.get_measurements();
		if (!measurements) {
			return false;
		}
		auto filename = get_measurements_filename(folder, info);
		if (save_xdr_to_file(*measurements, filename.c_str())) {
			std::printf("failed to write to file %s\n", filename.c_str());
			return false;
		}
		return true;
	} catch(...) {
		std::printf("node %s is not responding to messages\n", info.hostname.c_str());
		return false;
	}
}

void wait_for_all_measurements(std::string const& folder, const ReplicaConfig& config) {
	auto infos = config.list_info();
	while(true) {
		bool failed = false;
		for (auto& info : infos) {
			if (!save_measurement(folder, info)) {
				failed = true;
			}
		}
		if (!failed) {
			return;
		}
	}
}

int main(int argc, char* const* argv)
{
	std::string config_file;

	std::string output_folder;
	
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case OPT_CONFIG_FILE:
				config_file = optarg;
				break;
			case OUTPUT_FOLDER:
				output_folder = optarg;
				break;
			default:
				usage();
		}
	}

	if (config_file.size() == 0) {
		config_file = get_config_file();
	}


	if (output_folder.size() == 0) {
		usage();
	}

	if (mkdir_safe(output_folder.c_str())) {
		std::printf("output directory %s already exists, continuing\n", output_folder.c_str());
	}

	ReplicaConfig config;

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file.c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file.c_str());
		usage();
	}

	config.parse(fyd, 0);

	fy_document_destroy(fyd);

	wait_for_all_online(config);
	send_all_breakpoints(config);

	wait_for_one_experiment_done(config);
	send_all_experiment_done_signals(config);

	wait_for_all_same_height(config);

	wait_for_all_measurements(output_folder, config);

	send_all_breakpoints(config);
}



