#pragma once


/*! \file prefix.h

Two implementations of a trie prefix.  One is an arbitrary-length
byte array, and one is specialized for accountid keys.
*/
#include <atomic>
#include <compare>
#include <concepts>
#include <cstdint>
#include <mutex>

#include <tbb/blocked_range.h> // to get tbb::split

#include <xdrpp/endian.h>
#include <xdrpp/marshal.h>

#include "utils/big_endian.h"
#include "utils/debug_utils.h"

#include "xdr/types.h"

namespace speedex {

namespace {
inline static size_t num_prefix_bytes_(const unsigned int x){ 
	return ((x/8) + (x % 8 == 0?0:1));
}
}

/*! Typesafe way of storing the length of a key in bits.
Too many bugs were caused by accidentaly bits/bytes unit conversions.
*/
struct PrefixLenBits {

	uint16_t len;
	//! Number of bytes needed to store len bits of a prefix.
	size_t num_prefix_bytes() const{
		return num_prefix_bytes_(len);
	}

	//! Number of bytes that are fully used by len bits.
	size_t num_fully_covered_bytes() const {
		return len / 8;
	}

	std::strong_ordering operator<=>(const PrefixLenBits& other) const {
		return len <=> other.len;
	}

	bool operator==(const PrefixLenBits& other) const = default;

	PrefixLenBits operator+(const uint16_t other_bits) const {
		PrefixLenBits out;
		out.len = len + other_bits;
		return out;
	}

	constexpr unsigned int bytes_to_write_len() {
		return 2;
	}
};

namespace {
template<class T>
concept TriePrefix_get_prefix_match_len 
	= requires(
		const T self, 
		const PrefixLenBits& a, 
		const T& b,
		const PrefixLenBits& c) 
	{
		std::same_as<PrefixLenBits, decltype(
			self.get_prefix_match_len(a, b, c))>;
	};

template<class T>
concept TriePrefix_get_branch_bits
	= requires(
		T self,
		const PrefixLenBits& a) {

		std::same_as<uint8_t, decltype(self.get_branch_bits(a))>;
	};

template<class T>
concept TriePrefix_truncate
	= requires(
		T self,
		const PrefixLenBits& a) {

		self.truncate(a);
	};
template<class T>
concept TriePrefix_spaceship
	= 	requires(const T a, const T b) {
		a <=> b;
		a == b;
	};
}
//! Concept describing required methods for trie prefixes.
template <class T>
concept TriePrefix = TriePrefix_get_prefix_match_len<T>
	&& TriePrefix_get_branch_bits<T>
	&& TriePrefix_truncate<T>
	&& TriePrefix_spaceship<T>
	&& requires {
	{ T() };

	std::same_as<size_t, decltype(T::size_bytes())>;
};

/*! Generic prefix of arbitrary length.
Prefix is broken into pieces of width BRANCH_BITS, although
in practice we always use BRANCH_BITS=4.
*/
template<uint16_t MAX_LEN_BYTES, uint8_t BRANCH_BITS = 4>
struct ByteArrayPrefix {
	static_assert(!xdr::is_big_endian, "big endian is unimplemented");
	static_assert(BRANCH_BITS == 4, "unimplemented otherwise");

	constexpr static uint16_t WORDS 
		= (MAX_LEN_BYTES / 8) + (MAX_LEN_BYTES % 8 == 0?0:1); //round up
	
	std::array<uint64_t, WORDS> data;

	constexpr static uint16_t MAX_LEN_BITS = 8 * MAX_LEN_BYTES;

	constexpr static uint64_t BRANCH_MASK 
		= (((uint64_t)1) << (BRANCH_BITS)) - 1;

	ByteArrayPrefix()
		: data() {
			data.fill(0);
		}

	ByteArrayPrefix(const std::array<unsigned char, MAX_LEN_BYTES>& input)
		: data() {
			data.fill(0);
			auto* ptr = reinterpret_cast<unsigned char*>(data.data());
			memcpy(ptr, input.data(), MAX_LEN_BYTES);
	}

	//! Returns the number of bits that match between this and \a other,
	//! rounded down to the nearest multiple of BRANCH_BITS.
	PrefixLenBits get_prefix_match_len(
		const PrefixLenBits& self_len, 
		const ByteArrayPrefix& other, 
		const PrefixLenBits& other_len) const {

		size_t res = MAX_LEN_BYTES * 8;

		for (size_t i = 0; i < WORDS; i++) {
			uint64_t local = data[i] ^ other.data[i];
			if (local) {

				size_t word_offset = __builtin_ctzll(local);
				word_offset -= word_offset % 8;

				if ((((local >> word_offset) & 0xF0) == 0)
				 && (((local >> word_offset) & 0x0F) != 0)){
					word_offset += 4;
				}
				res = std::min((i * 64) + word_offset, res);
				break;
			}
		}
		uint16_t res_final = res - res % BRANCH_BITS;
		return std::min({PrefixLenBits{res_final}, self_len, other_len});
	}

	//! get the BRANCH_BITS bits that follow a specific length point.
	//! I.e. if prefix is 0xABCD, get_branch_bits(4) = B
	unsigned char get_branch_bits(const PrefixLenBits branch_point) const {
		if (branch_point.len >= MAX_LEN_BITS) {
			throw std::runtime_error("can't branch beyond end");
		}

		uint16_t word_idx = branch_point.len / 64;

		uint16_t byte_offset = (branch_point.len % 64) / 8;

		uint8_t byte = (data[word_idx] >> (8 * byte_offset)) & 0xFF;

		return (branch_point.len % 8 == 0 ? byte >> 4 : byte) & 0x0F;
	}

	//! Truncate a prefix to a specific length.  Bits beyond truncate_point
	//! are set to 0.
	void truncate(const PrefixLenBits truncate_point) {
		if (truncate_point.len >= MAX_LEN_BITS) {
			throw std::runtime_error("can't truncate beyond end");
		}

		uint16_t word_idx = truncate_point.len / 64;
		uint16_t byte_offset = (truncate_point.len % 64) / 8;
		uint16_t word_offset = 8 * byte_offset;

		uint64_t truncate_mask = (((uint64_t)1) << word_offset) - 1;
		if (truncate_point.len % 8 != 0) {
			truncate_mask |= ((uint64_t)0xF0) <<word_offset;
		}
		data[word_idx] &= truncate_mask;
		for (size_t i = word_idx + 1; i < WORDS; i++) {
			data[i] = 0;
		}
	}

	//! Get byte at position i.
	//! Primary use is when writing the prefix.
	unsigned char& operator[](size_t i) {
		unsigned char* data_ptr 
			= reinterpret_cast<unsigned char*>(data.data());
		return data_ptr[i];
	}

	//! const access to byte at position i.
	unsigned char operator[](size_t i) const {
		const unsigned char* data_ptr 
			= reinterpret_cast<const unsigned char*>(data.data());
		return data_ptr[i];
	}

	//! Set the byte at a particular index.
	void set_byte(size_t i, unsigned char byte) {
			if (i >= MAX_LEN_BYTES) {
			throw std::runtime_error("invalid prefix array access!");
		}

		unsigned char* data_ptr = reinterpret_cast<unsigned char*>(data.data());
		data_ptr[i] = byte;
	}

	//! Bounds checked byte access.
	unsigned char& at(size_t i) {
		if (i >= MAX_LEN_BYTES) {
			throw std::runtime_error("invalid prefix array access!");
		}

		unsigned char* data_ptr = reinterpret_cast<unsigned char*>(data.data());
		return data_ptr[i];
	}

	//! Set prefix to be the maximum possible prefix.
	void set_max() {
		data.fill(UINT64_MAX);
	}

	//! Set prefix to empty (all zeros).
	void clear() {
		data.fill(0);
	}

	//! Return an array of bytes representing the prefix's contents.
	xdr::opaque_array<MAX_LEN_BYTES> get_bytes_array() const {
		xdr::opaque_array<MAX_LEN_BYTES> out;
		const unsigned char* ptr 
			= reinterpret_cast<const unsigned char*>(data.data());

		memcpy(out.data(), ptr, MAX_LEN_BYTES);
		return out;
	}

	//! Return a vector of bytes with the prefix's contents.
	std::vector<unsigned char> get_bytes(const PrefixLenBits prefix_len) const {
		std::vector<unsigned char> bytes_out;

		const unsigned char* ptr 
			= reinterpret_cast<const unsigned char*>(data.data());

		bytes_out.insert(
			bytes_out.end(), ptr, ptr + prefix_len.num_prefix_bytes());

		return bytes_out;
	}

	constexpr static size_t size_bytes() {
		return MAX_LEN_BYTES;
	}

	//! memcpy into this prefix from a given buffer.
	void set_from_raw(const unsigned char* src, size_t len) {
		auto* dst = reinterpret_cast<unsigned char*>(data.data());
		if (len > MAX_LEN_BYTES) {
			throw std::runtime_error("len is too long!");
		}
		memcpy(dst, src, len); 
	}


	constexpr static PrefixLenBits len() {
		return PrefixLenBits{MAX_LEN_BITS};
	}

	std::strong_ordering operator<=>(const ByteArrayPrefix& other) const {

		if (&other == this) return std::strong_ordering::equal;


		//TODO try the other candidate(compare word by word, in loop);

		auto res = memcmp(
			reinterpret_cast<const unsigned char*>(data.data()),
			reinterpret_cast<const unsigned char*>(other.data.data()),
			MAX_LEN_BYTES);
		if (res < 0) {
			return std::strong_ordering::less;
		}
		if (res > 0) {
			return std::strong_ordering::greater;
		}
		return std::strong_ordering::equal;
	}

	bool operator==(const ByteArrayPrefix& other) const = default;

	std::string to_string(const PrefixLenBits len) const {
		
		auto bytes = get_bytes(len);
		return debug::array_to_str(bytes.data(), len.num_prefix_bytes());
	}

	//! Sets the bits immediately following the first fixed_len_bits bits
	//! to branch_bits (which should be a valid branch value)
	void
	set_next_branch_bits(
		const PrefixLenBits fixed_len_bits, const unsigned char branch_bits) {
		
		unsigned int byte_index = fixed_len_bits.len / 8;
		uint8_t remaining_bits = fixed_len_bits.len % 8;

		if (byte_index >= MAX_LEN_BYTES) {
			throw std::runtime_error("invalid set_next_branch_bits access");
		}

		uint8_t next_byte = at(byte_index);

		next_byte &= (0xFF << (8-remaining_bits));

		uint8_t branch_bits_offset = 8-remaining_bits-BRANCH_BITS;
		next_byte |= (branch_bits << (branch_bits_offset));

		set_byte(byte_index, next_byte);
	}
};

//! Prefix specialized to case where key is an account ID.
//! Specializing it makes it slightly easier to manage.
//! In particular, it makes it easy to truncate/get next branch bits/etc,
//! since most operations can be done with just one or two bitwise ops
//! (and we don't have to worry about cross-word actions).
class AccountIDPrefix {
	AccountID prefix;

	static_assert(sizeof(AccountID) == 8, 
		"this won't work with diff size accountid");

	constexpr static uint8_t BRANCH_BITS = 4;
	constexpr static uint8_t BRANCH_MASK = 0x0F;

	constexpr static uint8_t MAX_LEN_BITS = 64;
	constexpr static uint8_t MAX_LEN_BYTES = 8;

public:

	AccountIDPrefix(AccountID id = 0) : prefix(id) {}

	std::strong_ordering 
	operator<=>(const AccountIDPrefix& other) const = default;

	bool 
	operator==(const AccountIDPrefix& other) const = default;

	//! Get the bits of the prefix just beyond branch_point
	unsigned char get_branch_bits(const PrefixLenBits branch_point) const {
		if (branch_point.len >= MAX_LEN_BITS) {
			std::printf("Bad branch bits was %u\n", branch_point.len);
			throw std::runtime_error("can't branch beyond end");
		}
		return (prefix >> (60 - branch_point.len)) & BRANCH_MASK;
	}

	//! Compute the length of the longest matching initial subsequence
	//! of this prefix and the other prefix.
	PrefixLenBits get_prefix_match_len(
		const PrefixLenBits& self_len, 
		const AccountIDPrefix& other, 
		const PrefixLenBits& other_len) const {

		uint64_t diff = prefix ^ other.prefix;
		PrefixLenBits computed = PrefixLenBits{MAX_LEN_BITS};

		if (diff != 0) {
			size_t matching_bits = __builtin_clzll(diff);
			uint16_t match_rounded 
				= matching_bits - (matching_bits % BRANCH_BITS);
			computed = PrefixLenBits{match_rounded};
		}
		return std::min({computed, self_len, other_len});
	}

	//! Truncate the prefix to a defined length
	void truncate(const PrefixLenBits truncate_point) {
		prefix &= (UINT64_MAX << (64 - truncate_point.len));
	}

	//! Convert prefix to an array of bytes.
	xdr::opaque_array<MAX_LEN_BYTES> get_bytes_array() const {
		xdr::opaque_array<MAX_LEN_BYTES> out;
		write_unsigned_big_endian(out, prefix);
		return out;
	}

	std::vector<uint8_t> get_bytes(PrefixLenBits prefix_len_bits) const {
		std::vector<uint8_t> out;
		auto full = get_bytes_array();
		auto num_bytes = prefix_len_bits.num_prefix_bytes();
		out.insert(out.end(), full.begin(), full.begin() + num_bytes);
		return out;
	}

	AccountID get_account() const {
		return prefix;
	}

	std::string to_string(const PrefixLenBits len) const {
		auto bytes = get_bytes_array();
		return debug::array_to_str(bytes.data(), len.num_prefix_bytes());
	}

	//! Modify the prefix by setting the bits after fixed_len to bb
	void set_next_branch_bits(PrefixLenBits fixed_len, const uint8_t bb) {
		uint8_t offset = (60-fixed_len.len);	
		uint64_t mask = ((uint64_t) BRANCH_MASK) << offset;
		uint64_t adjust = ((uint64_t) bb) << offset;
		prefix = (prefix & (~mask)) | adjust;
	}

	constexpr static size_t size_bytes() {
		return sizeof(AccountID); // 8
	}
};




template<typename prefix_t>
static void write_node_header(std::vector<unsigned char>& buf, const prefix_t prefix, const PrefixLenBits prefix_len_bits) {

	append_unsigned_big_endian(buf, prefix_len_bits.len);

	auto prefix_bytes = prefix.get_bytes(prefix_len_bits);
	buf.insert(buf.end(), prefix_bytes.begin(), prefix_bytes.end());
}

[[maybe_unused]]
static void write_node_header(unsigned char* buf, const unsigned char* prefix, const PrefixLenBits prefix_len, const uint8_t last_byte_mask = 255) {
	write_unsigned_big_endian(buf, prefix_len.len);
	int num_prefix_bytes = prefix_len.num_prefix_bytes();
	memcpy(buf+2, prefix, num_prefix_bytes);
	buf[num_prefix_bytes + 1] &= last_byte_mask; // +1 from -1 +2
}


[[maybe_unused]]
static int get_header_bytes(const PrefixLenBits prefix_len) {
	return prefix_len.num_prefix_bytes() + 2;
}

} /* speedex */
