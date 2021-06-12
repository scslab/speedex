#include "utils/header_persistence.h"
#include "utils/save_load_xdr.h"

#include "config.h"

namespace speedex {

inline std::string 
header_filename(const uint64_t round_number) {
	return std::string(ROOT_DB_DIRECTORY) + std::string(HEADER_DB) + std::to_string(round_number) + std::string(".header");
}

HashedBlock
load_header(const uint64_t round_number) {
	HashedBlock out;
	auto filename = header_filename(round_number);
	auto res = load_xdr_from_file(out, filename.c_str());
	if (res != 0) {
		throw std::runtime_error((std::string("can't find header file ") + filename).c_str());
	}
	return out;
}

bool check_if_header_exists(const uint64_t round_number) {
	auto filename = header_filename(round_number);
	if (FILE* file = fopen(filename.c_str(), "r")) {
		fclose(file);
		return true;
	}
	return false;
}

void save_header(const HashedBlock& header) {
	auto filename = header_filename(header.block.blockNumber);
	if (save_xdr_to_file(header, filename.c_str())) {
		throw std::runtime_error("could not save header file!");
	}
}

}
