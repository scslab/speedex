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

#include "speedex/speedex_static_configs.h"

#include <cstdio>

namespace speedex
{

void
log_static_configs()
{
	std::printf("========== static configs ==========\n");
	std::printf("USE_TATONNEMENT_TIMEOUT_THREAD = %u\n", USE_TATONNEMENT_TIMEOUT_THREAD);
	std::printf("DISABLE_PRICE_COMPUTATION      = %u\n", DISABLE_PRICE_COMPUTATION);
	std::printf("DISABLE_LMDB                   = %u\n", DISABLE_LMDB);
	std::printf("DETAILED_MOD_LOGGING           = %u\n", DETAILED_MOD_LOGGING);
	std::printf("PREALLOC_BLOCK_FILES           = %u\n", PREALLOC_BLOCK_FILES);
	std::printf("ACCOUNT_DB_SYNC_IMMEDIATELY    = %u\n", ACCOUNT_DB_SYNC_IMMEDIATELY);
	std::printf("MAX_SEQ_NUMS_PER_BLOCK         = %lu\n", MAX_SEQ_NUMS_PER_BLOCK);
	std::printf("LOG_TRANSFERS                  = %u\n", LOG_TRANSFERS);
	std::printf("NUM_ACCOUNT_DB_SHARDS          = %u\n", NUM_ACCOUNT_DB_SHARDS);
	std::printf("====================================\n");
}

}