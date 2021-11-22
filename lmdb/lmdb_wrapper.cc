#include "lmdb/lmdb_wrapper.h"

namespace speedex 
{

MDB_dbi BaseLMDBInstance::open_db(const char* name) {
  if (!env_open) {
    throw std::runtime_error("env not open");
  }

  auto rtx = env.rbegin();

  MDB_dbi dbi = rtx.open(name);
  if (!metadata_dbi_open) {
    metadata_dbi = rtx.open("metadata");
    metadata_dbi_open = true;
  }

  auto persisted_round_number_opt = rtx.get(metadata_dbi, dbval("persisted block"));
  if (!persisted_round_number_opt) {
    throw std::runtime_error("missing metadata contents");
  }

  //persisted_round_number = persisted_round_number_opt -> uint64();
  rtx.commit();
  return dbi;
}

uint64_t
BaseLMDBInstance::get_persisted_round_number() const {
  if (!env_open) {
    return 0;
  }
  auto rtx = env.rbegin();
  auto persisted_round_number = *rtx.get(metadata_dbi, dbval("persisted block"));
  uint64_t out = persisted_round_number.uint64();
  rtx.commit();
  return out;
}

MDB_dbi BaseLMDBInstance::create_db(const char* name) {
  if (!env_open) {
    throw std::runtime_error("env not open");
  }
  auto wtx = env.wbegin();
  MDB_dbi dbi = wtx.open(name, MDB_CREATE);

  if (!metadata_dbi_open) {
    metadata_dbi = wtx.open("metadata", MDB_CREATE);
    metadata_dbi_open = true;
  }

  auto preexist_check = wtx.get(metadata_dbi, dbval("persisted block"));

  if (preexist_check) {
    if (preexist_check -> uint64() != 0) {
      throw std::runtime_error("database already existed, can't create new");
    }
  }

  write_persisted_round_number(wtx, 0);
  wtx.commit();

  //persisted_round_number = 0;
  return dbi;
}

void 
BaseLMDBInstance::write_persisted_round_number(dbenv::wtxn& wtx, uint64_t round_number) {
  dbval persisted_key = dbval("persisted block");
  dbval round_value = dbval(&round_number, sizeof(uint64_t));
  wtx.put(metadata_dbi, &persisted_key, &round_value);
}


void BaseLMDBInstance::commit_wtxn(dbenv::wtxn& txn, uint64_t persisted_round, bool do_sync) {

  if (persisted_round < get_persisted_round_number()) {
    throw std::runtime_error("can't overwrite later round with earlier round");
  }

 // persisted_round_number = persisted_round;

  if (!env_open) {
    return;
  }

  //careful if moving db to a different endian machine
  //txn.put(metadata_dbi, dbval("persisted block"), dbval(&persisted_round, sizeof(uint64_t)));
  write_persisted_round_number(txn, persisted_round);
  txn.commit();

  if (do_sync) { 
    sync();
  }
}

LMDBInstance::LMDBInstance(size_t mapsize)
    : BaseLMDBInstance(mapsize)
    , dbi(0)
    , dbi_valid(false)
    {}

const MDB_dbi& 
LMDBInstance::get_data_dbi() const {
  if (!dbi_valid) {
    throw std::runtime_error("invalid data dbi access before opening!");
  }
  return dbi;
}

MDB_stat 
LMDBInstance::stat() const {
  auto rtx = rbegin();
  auto stat = rtx.stat(dbi);
  rtx.abort();
  return stat;
}

void 
LMDBInstance::create_db(const char* name) {
  dbi = BaseLMDBInstance::create_db(name);
  dbi_valid = true;
}

void 
LMDBInstance::open_db(const char* name) {
  dbi = BaseLMDBInstance::open_db(name);
  dbi_valid = true;
}

SharedLMDBInstance::SharedLMDBInstance(BaseLMDBInstance& base_lmdb)
  : base_lmdb(base_lmdb)
  , local_dbi{0}
  , local_dbi_valid(false)
  {}

const MDB_dbi& 
SharedLMDBInstance::get_data_dbi() {
  if (!local_dbi_valid) {
    throw std::runtime_error("shared lmdb dbi access before opening");
  }
  return local_dbi;
}

void 
SharedLMDBInstance::create_db(const char* name) {
  local_dbi = base_lmdb.create_db(name);
  local_dbi_valid = true;
}

void 
SharedLMDBInstance::open_db(const char* name) {
  local_dbi = base_lmdb.open_db(name);
  local_dbi_valid = true;
}

} /* speedex */
