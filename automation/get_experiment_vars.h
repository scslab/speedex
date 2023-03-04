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

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "utils/debug_macros.h"

#include "speedex/speedex_runtime_configs.h"

namespace speedex {

constexpr static const char* CONFIG_FILE_FILENAME = "automation/config_file";
constexpr static const char* SPEEDEX_OPTIONS_FILENAME = "automation/speedex_options";
constexpr static const char* DATA_FOLDER_FILENAME = "automation/data_folder";
constexpr static const char* RESULTS_FOLDER_FILENAME = "automation/results_folder";
constexpr static const char* NUM_THREADS_FILENAME = "automation/num_threads";
constexpr static const char* CHECK_SIGS_FILENAME = "automation/check_sigs";

static std::string
get_experiment_var(const char* var) {
	FILE* f = std::fopen(var, "r");

	const std::string whitespace = " \t\r\n";

	if (f == nullptr) {
		throw std::runtime_error(std::string("failed to open file ") + var);
	}

	constexpr static size_t buf_size = 1024;

	std::array<char, buf_size> buf;
	buf.fill(0);

	if (std::fread(buf.data(), sizeof(char), buf_size, f) == 0) {
		throw std::runtime_error("failed to read replica file");
	}

	std::string str = std::string(buf.data());

	size_t last_idx = str.find_last_not_of(whitespace);

	str = str.substr(0, last_idx + 1);

	LOG("variable \"%s\" assigned value \"%s\"", var, str.c_str());

	std::fclose(f);

	return str;
}

[[maybe_unused]]
static std::string
get_config_file() {
	return get_experiment_var(CONFIG_FILE_FILENAME);
}

[[maybe_unused]]
static std::string
get_speedex_options() {
	return get_experiment_var(SPEEDEX_OPTIONS_FILENAME);
}

[[maybe_unused]]
static std::string
get_experiment_data_folder() {
	return get_experiment_var(DATA_FOLDER_FILENAME);
}

[[maybe_unused]]
static std::string
get_experiment_results_folder() {
	return get_experiment_var(RESULTS_FOLDER_FILENAME);
}

[[maybe_unused]]
static uint32_t
get_num_threads() {
	return std::stoi(get_experiment_var(NUM_THREADS_FILENAME));
}

[[maybe_unused]]
static bool
get_check_sigs() {
	return (std::stoi(get_experiment_var(CHECK_SIGS_FILENAME)) == 1);
}

[[maybe_unused]]
static
SpeedexRuntimeConfigs
get_runtime_configs()
{
	return SpeedexRuntimeConfigs
	{
		.check_sigs = get_check_sigs()
	};
}

} /* speedex */
