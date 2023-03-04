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

#include "speedex/speedex_management_structures.h"

#include "automation/get_experiment_vars.h"

namespace speedex
{

//
//SpeedexRuntimeConfigs::SpeedexRuntimeConfigs()
  //  : check_sigs(get_check_sigs())
//{}

void
SpeedexManagementStructures::open_lmdb_env()
{
    db.open_lmdb_env();
    orderbook_manager.open_lmdb_env();
    block_header_hash_map.open_lmdb_env();
}

void
SpeedexManagementStructures::create_lmdb()
{
    db.create_lmdb();
    orderbook_manager.create_lmdb();
    block_header_hash_map.create_lmdb();
}

void
SpeedexManagementStructures::open_lmdb()
{
    db.open_lmdb();
    orderbook_manager.open_lmdb();
    block_header_hash_map.open_lmdb();
}

} // namespace speedex
