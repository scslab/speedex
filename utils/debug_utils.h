#pragma once
/*! \file debug_utils.h 
Miscellaneous debug utility functions.
*/

#include <sstream>
#include <iomanip>

namespace debug {
//! Convert a byte array to a hex string.
static std::string array_to_str(const unsigned char* array, const int len) {
	std::stringstream s;
	s.fill('0');
	for (int i = 0; i < len; i++) {
		s<< std::setw(2) << std::hex << (unsigned short)array[i];
	}
	return s.str();
}

} /* debug */
