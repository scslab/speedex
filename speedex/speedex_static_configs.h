#pragma once

#include <cstdint>

namespace speedex
{

// default setup
#ifndef _MAX_SEQ_NUMS_PER_BLOCK
  #define _MAX_SEQ_NUMS_PER_BLOCK 64
#endif

constexpr static uint32_t MAX_SEQ_NUMS_PER_BLOCK = _MAX_SEQ_NUMS_PER_BLOCK;

constexpr static bool DETAILED_MOD_LOGGING = true;
constexpr static bool PREALLOC_BLOCK_FILES = true;

#if 0
// setup for blockstm replication
constexpr static uint32_t MAX_SEQ_NUMS_PER_BLOCK = 16'000;
constexpr static bool DETAILED_MOD_LOGGING = false;
constexpr static bool PREALLOC_BLOCK_FILES = false;
#endif

// general
constexpr static bool ACCOUNT_DB_SYNC_IMMEDIATELY = false;

} /* speedex */
