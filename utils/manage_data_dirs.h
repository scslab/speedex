#pragma once

#include <string>

namespace speedex {

std::string memory_database_lmdb_dir();
void
make_memory_database_lmdb_dir();
void
clear_memory_database_lmdb_dir();

std::string orderbook_lmdb_dir();
void
make_orderbook_lmdb_dir();
void
clear_orderbook_lmdb_dir();

std::string header_hash_lmdb_dir();
void
make_header_hash_lmdb_dir();
void
clear_header_hash_lmdb_dir();

std::string hotstuff_index_lmdb_dir();
std::string hotstuff_block_data_dir();
void make_hotstuff_dirs();
void clear_hotstuff_dirs();

void clear_all_data_dirs();
void make_all_data_dirs();

} /* speedex */
