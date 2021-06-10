#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <lmdb.h>

#include "utils/cleanup.h"
#include "lmdb/lmdb_types.h"

#include "xdr/transaction.h"

namespace speedex {

#define DEFAULT_LMDB_FLAGS MDB_WRITEMAP | MDB_NOSYNC // | MDB_NOTLS

/*! Utility methods around an LMDB instance.
    Beyond dbenv, tracks persisted round number and MDB_dbi for object data and metadata
    databases.
*/
class LMDBInstance {
  dbenv env;
  MDB_dbi dbi;
  bool env_open;

  MDB_dbi metadata_dbi;

  uint64_t persisted_round_number; // state up to and including persisted_round_number has been persisted.
  //Starts at 0 - means that we can't have a block number 0, except as a base/bot instance

public:

  const uint64_t& get_persisted_round_number() const {
    return persisted_round_number;
  }

  LMDBInstance(std::size_t mapsize = 0x1000000000) 
    : env{mapsize}
    , dbi{0}
    , env_open{false}
    , persisted_round_number(0) {};

  void open_env(const std::string path, unsigned flags = DEFAULT_LMDB_FLAGS, mdb_mode_t mode = 0666) {
    env.open(path.c_str(), flags, mode);
    env_open = true;
  }

  void create_db(const char* name);
  void open_db(const char* name);

  MDB_stat stat() {
    auto rtx = env.rbegin();
    auto stat = rtx.stat(dbi);
    rtx.abort();
    return stat;
  }

  operator bool() {
    return env_open;
  }

  dbenv::wtxn wbegin() {
    return env.wbegin();
  }

  dbenv::txn rbegin() {
    return env.rbegin();
  }

  const MDB_dbi& get_data_dbi() {
    return dbi;
  }

  void sync() {
    env.sync();
  }

  //! Commits a write transaction to the database.
  //! Optionally performs an msync.  Updates persisted_round counter.
  void commit_wtxn(dbenv::wtxn& txn, uint64_t persisted_round, bool do_sync = true);
};

namespace {

//! Generic mapping from return type to signifier of successful completion.
//! Useful for mocking out parts of speedex's internals when reloading
//! from a set of lmdb instances with different persisted round numbers.
template<typename ret_type>
struct generic_success {
};

template<>
struct generic_success<TransactionProcessingStatus> {
  static constexpr TransactionProcessingStatus success() {
    return TransactionProcessingStatus::SUCCESS;
  }
};

template<>
struct generic_success<bool> {
  static constexpr bool success() {
    return true;
  }
};

template<>
struct generic_success<void> {
  static constexpr void success() {}
};

}


//! Extend to use.  Base class for mocking out parts of speedex internals.
//! Actions passed to generic_do are no-ops is persisted_round_number is too
//! high (i.e. actions to be done are already reflected in the lmdb instance).
template<typename wrapped_type>
struct LMDBLoadingWrapper {
  wrapped_type wrapped;
  const bool do_operations;

  template<auto func, typename... Args>
  auto
  generic_do(Args... args) {
    using ret_type = decltype((wrapped.*func)(args...));
    if (do_operations) {
      return (wrapped.*func)(args...);
    }
    return generic_success<ret_type>::success();
  }

  template<typename... Args>
  LMDBLoadingWrapper(const uint64_t current_block_number, Args&... args)
    : wrapped(args...)
    , do_operations(wrapped.get_persisted_round_number() < current_block_number) {}
};

} /* speedex */
