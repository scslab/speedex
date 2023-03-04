#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <optional>

#include <getopt.h>

#include "config/replica_config.h"

namespace speedex
{

[[noreturn]]
static void usage(std::string binary_name) {
	std::printf("usage: %s", binary_name.c_str());
	std::printf(R"(
	--speedex_options=<options_yaml, required> 
	--exp_data_folder=<experiment_data path, required>
	--replica_id=<id, required> 
	--config_file=<filename, required>
	--results_folder=<filename, required> (really a prefix to output filenames)
	--load_from_lmdb <flag, optional>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE,

	SPEEDEX_OPTIONS,
	EXPERIMENT_DATA_FOLDER,
	RESULTS_FOLDER,
	LOAD_FROM_LMDB,

	EXPERIMENT_OPTIONS,
	EXPERIMENT_NAME,
	JUST_PARAMS,

	// blockstm_comparison-specific
	NUM_ACCOUNTS,
	BATCH_SIZE,
};

static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},

	{"speedex_options", required_argument, nullptr, SPEEDEX_OPTIONS},
	{"exp_data_folder", required_argument, nullptr, EXPERIMENT_DATA_FOLDER},
	{"results_folder", required_argument, nullptr, RESULTS_FOLDER},
	{"load_lmdb", no_argument, nullptr, LOAD_FROM_LMDB},

	{"exp_options", required_argument, nullptr, EXPERIMENT_OPTIONS},
	{"exp_name", required_argument, nullptr, EXPERIMENT_NAME},
	{"just_params", no_argument, nullptr, JUST_PARAMS},

	//blockstm comparison
	{"num_accounts", required_argument, nullptr, NUM_ACCOUNTS},
	{"batch_size", required_argument, nullptr, BATCH_SIZE},
	{nullptr, 0, nullptr, 0}
};

//unified arg parsing across different binaries
struct CommandLineArgs
{
	std::optional<ReplicaID> self_id;
	std::optional<std::string> config_file;

	std::string speedex_options_file;
	std::string experiment_data_folder;
	std::string experiment_results_folder;

	bool load_from_lmdb = false;

	std::string experiment_options_file;
	std::string experiment_name;
	bool just_params = false;

	uint64_t num_accounts = 0;
	uint64_t batch_size = 0;
};


[[maybe_unused]]
static CommandLineArgs 
parse_cmd(int argc, char* const* argv, std::string binary_name)
{
	CommandLineArgs out;
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case OPT_REPLICA_ID:
				out.self_id = std::stol(optarg);
				break;
			case OPT_CONFIG_FILE:
				out.config_file = optarg;
				break;
			case SPEEDEX_OPTIONS:
				out.speedex_options_file = optarg;
				break;
			case EXPERIMENT_DATA_FOLDER:
				out.experiment_data_folder = optarg;
				break;
			case RESULTS_FOLDER:
				out.experiment_results_folder = optarg;
				break;
			case LOAD_FROM_LMDB:
				out.load_from_lmdb = true;
				break;
			case EXPERIMENT_OPTIONS:
				out.experiment_options_file = optarg;
				break;
			case EXPERIMENT_NAME:
				out.experiment_name = optarg;
				break;
			case JUST_PARAMS:
				out.just_params = true;
				break;	
			case NUM_ACCOUNTS:
				out.num_accounts = std::stol(optarg);
				break;
			case BATCH_SIZE:
				out.batch_size = std::stol(optarg);
				break;
			default:
				usage(binary_name);
		}
	}
	return out;
}

} /* speedex */
