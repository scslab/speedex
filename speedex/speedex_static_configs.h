#pragma once

#include <cstdint>

namespace speedex
{

// default setup
constexpr static uint32_t MAX_SEQ_NUMS_PER_BLOCK = 64;
constexpr static bool DETAILED_MOD_LOGGING = true;
constexpr static bool PREALLOC_BLOCK_FILES = true;

#if 0
// setup for blockstm replication
constexpr static uint32_t MAX_SEQ_NUMS_PER_BLOCK = 16'000;
constexpr static bool DETAILED_MOD_LOGGING = false;
constexpr static bool PREALLOC_BLOCK_FILES = false;
#endif

} /* speedex */
