#include "utils/manage_data_dirs.h"

#include "utils/save_load_xdr.h"

#include <filesystem>
#include <system_error>

#include "config.h"

#include "hotstuff/manage_data_dirs.h"
#include "hotstuff/config/replica_config.h"

namespace speedex {

std::string memory_database_lmdb_dir() {
	return std::string(ROOT_DB_DIRECTORY) + std::string(ACCOUNT_DB);
}

void
make_memory_database_lmdb_dir() {
	mkdir_safe(ROOT_DB_DIRECTORY);
	auto path = memory_database_lmdb_dir();
	mkdir_safe(path.c_str());
}

void
clear_memory_database_lmdb_dir() {
	auto path = memory_database_lmdb_dir();
	std::error_code ec;
	std::filesystem::remove_all({path}, ec);
	if (ec) {
		throw std::runtime_error("failed to clear memory_database dir");
	}
}

std::string orderbook_lmdb_dir() {
	return std::string(ROOT_DB_DIRECTORY) + std::string(OFFER_DB);
}
void
make_orderbook_lmdb_dir() {
	mkdir_safe(ROOT_DB_DIRECTORY);
	auto path = orderbook_lmdb_dir();
	mkdir_safe(path.c_str());
}

void
clear_orderbook_lmdb_dir() {
	auto path = orderbook_lmdb_dir();
	std::error_code ec;
	std::filesystem::remove_all({path}, ec);
	if (ec) {
		throw std::runtime_error("failed to clear orderbook dir");
	}
}


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

void clear_all_data_dirs(const hotstuff::ReplicaInfo& info) {
	clear_memory_database_lmdb_dir();
	clear_orderbook_lmdb_dir();
	clear_header_hash_lmdb_dir();
	hotstuff::clear_all_data_dirs(info);
}

void make_all_data_dirs(const hotstuff::ReplicaInfo& info) {
	make_memory_database_lmdb_dir();
	make_orderbook_lmdb_dir();
	make_header_hash_lmdb_dir();
	hotstuff::make_all_data_dirs(info);
}

} /* speedex */
