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
	std::printf("====================================\n");
}

}