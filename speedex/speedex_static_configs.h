#pragma once

#include <cstdint>

namespace speedex
{

// default setup
#ifndef _MAX_SEQ_NUMS_PER_BLOCK
  #define _MAX_SEQ_NUMS_PER_BLOCK 64
#endif

#ifndef _DISABLE_TATONNEMENT_TIMEOUT
	constexpr static bool USE_TATONNEMENT_TIMEOUT_THREAD = true;
#else
	constexpr static bool USE_TATONNEMENT_TIMEOUT_THREAD = false;
#endif

#ifdef _DISABLE_PRICE_COMPUTATION
	constexpr static bool DISABLE_PRICE_COMPUTATION = true;
#else
	constexpr static bool DISABLE_PRICE_COMPUTATION = false;
#endif

#ifdef _DISABLE_LMDB
	constexpr static bool DISABLE_LMDB = true;
#else
	constexpr static bool DISABLE_LMDB = false;
#endif

constexpr static uint32_t MAX_SEQ_NUMS_PER_BLOCK = _MAX_SEQ_NUMS_PER_BLOCK;

#ifndef _DISABLE_MOD_LOGGING
	constexpr static bool DETAILED_MOD_LOGGING = true;
#else
	constexpr static bool DETAILED_MOD_LOGGING = false;
#endif

constexpr static bool PREALLOC_BLOCK_FILES = true;

constexpr static bool LOG_TRANSFERS = true;

#if 0
// setup for blockstm replication
constexpr static uint32_t MAX_SEQ_NUMS_PER_BLOCK = 16'000;
constexpr static bool DETAILED_MOD_LOGGING = false;
constexpr static bool PREALLOC_BLOCK_FILES = false;
#endif

// general
constexpr static bool ACCOUNT_DB_SYNC_IMMEDIATELY = false;

void
__attribute__((constructor))
log_static_configs();

} /* speedex */
