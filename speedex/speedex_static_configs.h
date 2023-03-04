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


#ifdef _LOG_TRANSFERS
	constexpr static bool LOG_TRANSFERS = true;
#else
	constexpr static bool LOG_TRANSFERS = false;
#endif


#ifndef _NUM_ACCOUNT_DB_SHARDS
	constexpr static uint32_t NUM_ACCOUNT_DB_SHARDS = 16;
#else
	constexpr static uint32_t NUM_ACCOUNT_DB_SHARDS = _NUM_ACCOUNT_DB_SHARDS;
#endif

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
