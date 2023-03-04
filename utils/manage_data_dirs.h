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

#pragma once

#include <string>

namespace hotstuff
{
	class ReplicaInfo;
} /* hotstuff */

namespace speedex {

std::string log_dir();

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
