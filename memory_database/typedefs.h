#pragma once

#include <cstdint>

namespace speedex {

//! Handle for an account within the database.
typedef uint64_t account_db_idx;

constexpr static uint32_t NUM_ACCOUNT_DB_SHARDS = 16;

}