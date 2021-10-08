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
