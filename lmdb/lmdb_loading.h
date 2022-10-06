#pragma once

#include "xdr/transaction.h"

#include "lmdb/lmdb_types.h"

#include <xdrpp/marshal.h>

namespace speedex
{

//! Convert dbval to xdr
template<typename xdr_type>
void dbval_to_xdr(const lmdb::dbval& d, xdr_type& value) {
  auto bytes = d.bytes();
  xdr::xdr_from_opaque(bytes, value);
}

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

  template<auto func, typename... Args>
  auto
  unconditional_do(Args... args) {
    return (wrapped.*func)(args...);
  }

  template<typename... Args>
  LMDBLoadingWrapper(const uint64_t current_block_number, Args&... args)
    : wrapped(args...)
    , do_operations(wrapped.get_persisted_round_number() < current_block_number) {}
};

} /* speedex */
