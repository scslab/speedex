#include "lmdb/lmdb_wrapper.h"

namespace speedex 
{

void LMDBInstance::open_db(const char* name) {
  if (!env_open) {
    throw std::runtime_error("env not open");
  }

  auto rtx = env.rbegin();

  dbi = rtx.open(name);
  metadata_dbi = rtx.open("metadata");

  auto persisted_round_number_opt = rtx.get(metadata_dbi, dbval("persisted block"));
  if (!persisted_round_number_opt) {
    throw std::runtime_error("missing metadata contents");
  }

  persisted_round_number = persisted_round_number_opt -> uint64();
  rtx.commit();
}

void LMDBInstance::create_db(const char* name) {
  if (!env_open) {
    throw std::runtime_error("env not open");
  }
  auto wtx = env.wbegin();
  dbi = wtx.open(name, MDB_CREATE);
  metadata_dbi = wtx.open("metadata", MDB_CREATE);
  wtx.commit();

  persisted_round_number = 0;
  commitment_state = LMDBCommitmentState::BLOCK_END;
}

void LMDBInstance::commit_wtxn(dbenv::wtxn& txn, uint64_t persisted_round, bool do_sync, LMDBCommitmentState state) {

  if (persisted_round < persisted_round_number) {
    throw std::runtime_error("can't overwrite later round with earlier round");
  }

  persisted_round_number = persisted_round;

  if (!env_open) {
    return;
  }

  //careful if moving db to a different endian machine
  txn.put(metadata_dbi, dbval("persisted block"), dbval(&persisted_round, sizeof(uint64_t)));
  txn.put(metadata_dbi, dbval("persisted state"), dbval(&state, 1));
  txn.commit();

  if (do_sync) { 
    sync();
  }
}

} /* edce */
