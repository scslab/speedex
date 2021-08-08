#include "rpc/rpcconfig.h"
#include "utils/save_load_xdr.h"
#include "xdr/consensus_api.h"
#include "xdr/experiments.h"

#include <xdrpp/srpc.h>
#include <thread>
#include <chrono>

#include <cstdint>
#include <vector>

using namespace speedex;

std::string hostname_from_idx(int idx) {
	return std::string("10.10.1.") + std::to_string(idx);
}

void send_breakpoint_signal(int idx) {
	auto fd = xdr::tcp_connect(hostname_from_idx(idx).c_str(), SERVER_CONTROL_PORT);
	auto client = xdr::srpc_client<ExperimentControlV1>(fd.get());
	client.signal_start();
}

void connect_validators(int num_replicas) {
	for (int i = 2; i <=num_replicas; i++) {
		send_breakpoint_signal(i);
	}
}

void start_production() {
	send_breakpoint_signal(1);
}

bool node_is_ready(int idx) {
	try {
		std::printf("querying to see if node %d is ready\n", idx);
		auto fd = xdr::tcp_connect(hostname_from_idx(idx).c_str(), SERVER_CONTROL_PORT);
		auto client = xdr::srpc_client<ExperimentControlV1>(fd.get());
		auto res = client.is_ready_to_start();
		if (res) {
			return *res == 1;
		}
		return false;
	} catch(...) {
		std::printf("node %d is not yet responding to messages\n", idx);
		return false;
	}
}

void wait_for_node_ready(int idx) {
	while (true) {
		if (node_is_ready(idx)) {
			return;
		}
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}

void wait_for_all_nodes_ready(int num_replicas) {
	for (int i = 1; i <= num_replicas; i++) {

		wait_for_node_ready(i);
		std::printf("node %d online\n", i);
	}
}

ExperimentResultsUnion
poll_node(int idx) {
	
	auto fd = xdr::tcp_connect(hostname_from_idx(idx).c_str(), SERVER_CONTROL_PORT);
	auto client = xdr::srpc_client<ExperimentControlV1>(fd.get());
	while(true) {

		std::printf("polling node %d\n", idx);

		auto res = client.is_running();
		if (*res == 0) {
			break;
		} 
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	std::printf("client finished, getting measurements\n");
	return *client.get_measurements();
}

std::string measurements_filename(int idx, std::string base) {
	return base + "/" + std::to_string(idx) + "_measurements";
}

int main(int argc, char const *argv[]) {

	if (argc < 3 || argc > 4) {
		std::printf("usage: ./experiment_controller output_folder num_replicas <skip_intro> \n");
		return 0;
	}

	std::string base_name = std::string("replication_experiment_results/");

	std::string output_folder = base_name + std::string(argv[1]);

	if (mkdir_safe(base_name.c_str())) {
		std::printf("directory %s already exists, continuing\n", base_name.c_str());
	}

	if (mkdir_safe(output_folder.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_folder.c_str());
	}

	int num_replicas = std::stoi(argv[2]);

	if (argc == 3) {
		wait_for_all_nodes_ready(num_replicas);
		connect_validators(num_replicas);
		std::this_thread::sleep_for(std::chrono::seconds(1));
		start_production();
	}
	for (int i = 1; i <= num_replicas; i++) {
		auto measurements = poll_node(i);
		std::printf("got measurements");

		auto m_filename = measurements_filename(i, output_folder);
		if (save_xdr_to_file(measurements, m_filename.c_str())) {
			std::printf("was unable to save file to %s !!!\n", m_filename.c_str());
		}
		send_breakpoint_signal(i);
	}
}
