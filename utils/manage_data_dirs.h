#pragma once

#include <string>

namespace hotstuff
{
	class ReplicaInfo;
} /* hotstuff */

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

void clear_all_data_dirs(const hotstuff::ReplicaInfo&);
void make_all_data_dirs(const hotstuff::ReplicaInfo&);

namespace test
{

struct speedex_dirs
{
	speedex_dirs()
	{
		clear_memory_database_lmdb_dir();
		make_memory_database_lmdb_dir();
		clear_orderbook_lmdb_dir();
		make_orderbook_lmdb_dir();
		clear_header_hash_lmdb_dir();
		make_header_hash_lmdb_dir();
	}

	~speedex_dirs()
	{
		clear_memory_database_lmdb_dir();
		clear_orderbook_lmdb_dir();
		clear_header_hash_lmdb_dir();
	}
};

}

} /* speedex */
