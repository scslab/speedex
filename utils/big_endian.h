#pragma once

#include <concepts>
#include <cstdint>

/*! \file big_endian.h Utility functions for reading and writing 
    quantities in big-endian format 
*/

namespace speedex {

//! Reads contents of \a buf into \a value, starting from \a buf[offset]  
template<typename array, std::unsigned_integral T>
static void write_unsigned_big_endian(array& buf, const T& value, const size_t offset = 0) {
	constexpr size_t sz = sizeof(T);
	constexpr size_t buf_sz = sizeof(buf);

	static_assert((sz-1)*8 <= UINT8_MAX, "if this happens we need to account for overflows on mask shift");
	static_assert(sz <= buf_sz, "insufficient buffer size!");

	for (uint8_t loc = 0; loc < sz; loc++) {
		uint8_t shift = ((sz - loc - 1) * 8);
		uint8_t byte = (((value>>shift) & 0xFF));
		buf.at(loc + offset) =  byte;
	}
}

//! Appends \a value to \a buf, written in big endian
template<std::unsigned_integral T>
static void append_unsigned_big_endian(std::vector<unsigned char>& buf, const T& value) {
	constexpr size_t sz = sizeof(T);

	static_assert((sz-1)*8 <= UINT8_MAX, "if this happens we need to account for overflows on mask shift");
	for (uint8_t loc = 0; loc < sz; loc++) {
		uint8_t offset = ((sz - loc - 1) * 8);
		buf.push_back(((value>>offset) & 0xFF));

	}
}

//! Serialize \a value in big-endian and write to \a buf (and subsequent bytes)
template<std::unsigned_integral T>
static void write_unsigned_big_endian(unsigned char* buf, const T& value) {
	constexpr size_t sz = sizeof(T);

	static_assert((sz-1)*8 <= UINT8_MAX, "if this happens we need to account for overflows on mask shift");
	for (uint8_t loc = 0; loc < sz; loc++) {
		uint8_t offset = ((sz - loc - 1) * 8);
		buf[loc] = (unsigned char) ((value>>offset) & 0xFF);
	}
}

//! Reads contents of \a buf into \a output.
template<std::unsigned_integral T>
static void read_unsigned_big_endian(const unsigned char* buf, T& output) {
	constexpr size_t sz = sizeof(T);
	output = 0;
	for (uint8_t loc = 0; loc < sz; loc++) {
		output<<=8;
		output+=buf[loc];
	}
}

//! read into \a output from \a buf.  Assumes buf holds a value written in big endian.
template<typename T, size_t ARRAY_LEN>
static void read_unsigned_big_endian(const std::array<unsigned char, ARRAY_LEN>& buf, T& output) {
	static_assert(sizeof(T) <= ARRAY_LEN, "not enough bytes to read");
	read_unsigned_big_endian(buf.data(), output);
}

//! Reads value from \a buf into \a output.  Any \a buf with a [] operator returning a uint8_t works.
template<typename ArrayLike, std::unsigned_integral T>
static void read_unsigned_big_endian(const ArrayLike& buf, T& output) {
	constexpr size_t sz = sizeof(T);
	output = 0;
	for (uint8_t loc = 0; loc < sz; loc++) {
		output<<=8;
		output+=buf[loc];
	}
}

} /* speedex */
