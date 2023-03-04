#pragma once

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
