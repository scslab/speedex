#include "speedex/speedex_management_structures.h"

#include "automation/get_experiment_vars.h"

namespace speedex
{

SpeedexRuntimeConfigs::SpeedexRuntimeConfigs()
    : check_sigs(get_check_sigs())
{}

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