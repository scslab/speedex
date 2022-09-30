#pragma once

#include <cstdint>

namespace speedex
{

enum FilterResult : int32_t
{
    // success
    VALID_NO_TXS = 0,
    VALID_HAS_TXS = 1,

    // failure
    MISSING_REQUIREMENT = -1,
    INVALID_DUPLICATE = -2,
    ACCOUNT_NEXIST = -3,
    OVERFLOW_REQ = -4,
};

}
