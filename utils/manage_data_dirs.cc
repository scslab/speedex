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

#include "utils/manage_data_dirs.h"

#include "utils/save_load_xdr.h"

#include <filesystem>
#include <system_error>

#include "config.h"

#include <utils/mkdir.h>

#include "hotstuff/manage_data_dirs.h"
#include "hotstuff/config/replica_config.h"

#include "speedex/speedex_static_configs.h"

namespace speedex {

using utils::mkdir_safe;

std::string log_dir() {
	return std::string(ROOT_DB_DIRECTORY) + std::string(LOG_DIR);
}

void make_log_dir()
{
	mkdir_safe(ROOT_DB_DIRECTORY);
	mkdir_safe(log_dir());
}

void clear_log_dir()
{
	auto path = log_dir();
	std::error_code ec;
	std::filesystem::remove_all({path}, ec);
	if (ec) {
		throw std::runtime_error("failed to clear header hash dir");
	}
}

std::string memory_database_lmdb_dir() {
	return std::string(ROOT_DB_DIRECTORY) + std::string(ACCOUNT_DB);
}

std::string memory_database_lmdb_shard_dir(uint32_t shard)
{
	return memory_database_lmdb_dir() + std::to_string(shard) + "/";
}

void
make_memory_database_lmdb_dir() {
	mkdir_safe(ROOT_DB_DIRECTORY);
	auto path = memory_database_lmdb_dir();
	mkdir_safe(path.c_str());

	for (uint32_t shard = 0; shard < NUM_ACCOUNT_DB_SHARDS; shard++)
	{
		path = memory_database_lmdb_shard_dir(shard);
		mkdir_safe(path.c_str());
	}
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
	clear_log_dir();
	clear_memory_database_lmdb_dir();
	clear_orderbook_lmdb_dir();
	clear_header_hash_lmdb_dir();
	hotstuff::clear_all_data_dirs(info);
}

void make_all_data_dirs(const hotstuff::ReplicaInfo& info) {
	make_log_dir();
	make_memory_database_lmdb_dir();
	make_orderbook_lmdb_dir();
	make_header_hash_lmdb_dir();
	hotstuff::make_all_data_dirs(info);
}

} /* speedex */
