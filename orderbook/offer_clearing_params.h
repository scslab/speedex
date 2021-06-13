#pragma once

#include <cstdint>
#include <vector>

#include "utils/fixed_point_value.h"

namespace speedex {

/*! Specify how much of the available supply in one orderbook
is activated.
*/
struct OrderbookClearingParams {
	FractionalAsset supply_activated;
};

struct ClearingParams {
	uint8_t tax_rate;
	std::vector<OrderbookClearingParams> orderbook_params;
};

} /* speedex */