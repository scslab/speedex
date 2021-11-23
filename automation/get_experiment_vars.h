#pragma once

namespace speedex {

constexpr static const char* CONFIG_FILE_FILENAME = "automation/config_file";
constexpr static const char* SPEEDEX_OPTIONS_FILENAME = "automation/speedex_options";
constexpr static const char* DATA_FOLDER_FILENAME = "automation/data_folder";
constexpr static const char* RESULTS_FOLDER_FILENAME = "automation/results_folder";

static std::string
get_experiment_var(const char* var) {
	FILE* f = std::fopen(var, "r");

	if (f == nullptr) {
		throw std::runtime_error(std::string("failed to open file ") + var);
	}

	constexpr static size_t buf_size = 1024;

	std::array<char, buf_size> buf;

	if (std::fread(buf.data(), sizeof(char), buf_size, f) == 0) {
		throw std::runtime_error("failed to read replica file");
	}

	std::string str = std::string(buf.data());

	std::fclose(f);

	return str;
}

static std::string
get_config_file() {
	return get_experiment_var(CONFIG_FILE_FILENAME);
}

static std::string
get_speedex_options() {
	return get_experiment_var(SPEEDEX_OPTIONS_FILENAME);
}

static std::string
get_experiment_data_folder() {
	return get_experiment_var(DATA_FOLDER_FILENAME);
}

static std::string
get_experiment_results_folder() {
	return get_experiment_var(RESULTS_FOLDER_FILENAME);
}

} /* speedex */
