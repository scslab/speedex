#pragma once

#include "speedex/speedex_management_structures.h"
#include "speedex/speedex_operation.h"

#include "xdr/types.h"

namespace speedex {

uint64_t init_management_structures_from_lmdb(SpeedexManagementStructures& management_structures);

void init_management_structures_no_lmdb(SpeedexManagementStructures& management_structures, AccountID num_accounts, int num_assets, uint64_t default_amount);

} /* speedex */
