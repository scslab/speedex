#include "utils/manage_data_dirs.h"

#include "utils/save_load_xdr.h"

#include <filesystem>
#include <system_error>

#include "config.h"

namespace speedex {

std::string
header_hash_lmdb_dir() {
	return std::string(ROOT_DB_DIRECTORY) + std::string(HEADER_HASH_DB);
}

void
make_header_hash_lmdb_dir() {
	mkdir_safe(ROOT_DB_DIRECTORY);
	auto path = header_hash_lmdb_dir();
	mkdir_safe(path.c_str());
}

void
clear_header_hash_lmdb_dir() {
	auto path = header_hash_lmdb_dir();
	std::error_code ec;
	std::filesystem::remove_all({path}, ec);
	if (ec) {
		throw std::runtime_error("failed to clear header hash dir");
	}
}

} /* speedex */
