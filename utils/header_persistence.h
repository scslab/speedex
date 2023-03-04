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

/*! \file header_persistence.h

Utility functions for saving and loading block headers

*/

#include "xdr/block.h"

namespace speedex {

//! Name of header file on disk
std::string header_filename(const uint64_t round_number);

//! check whether a header file exists on disk
bool check_if_header_exists(const uint64_t round_number);

//! Load a saved header file
HashedBlock load_header(const uint64_t round_number);

//! Save a header to disk
void save_header(const HashedBlock& header);

} /* speedex */
