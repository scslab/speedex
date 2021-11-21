#pragma once

/*! \file children_map.h

Organizes pointers to children of a trie node.

This implementation stores a literal list of pointers.

*/

#include "utils/bitvector.h"

namespace speedex {

namespace {
template<typename ptr_t>
struct kv_pair_t {
	uint8_t first;
	ptr_t& second;
};
}

/*! Stores the main contents of a trie node.  
Main contents are either a list of pointers to valid child nodes
or a value type.
*/
template<typename trie_ptr_t, typename ValueType>
class FixedChildrenMap {

	//! Use 4 bits to branch, at most 16 children.
	constexpr static unsigned int BRANCH_BITS = 4; 
	constexpr static unsigned int NUM_CHILDREN = 1<<BRANCH_BITS;

	using underlying_ptr_t = trie_ptr_t::pointer;
	union {
		underlying_ptr_t map[NUM_CHILDREN];
		ValueType value_;
	};

public:
	using bv_t = BitVector<uint16_t>;
private:

	bv_t bv;
	bool value_active;

	//! Delete all open child nodes
	void clear_open_links() {
		if (!value_active) {
			for (auto iter = begin(); iter != end(); iter++) {
				delete (*iter).second;
			}
			bv.clear();
		}
	}

	//! Steal ownership of a set of children from another
	//! instance of this object.
	//! Typically called when moving the contents
	//! of one trie node to another node.
	void steal_ptr_map(FixedChildrenMap& other) {
		for (size_t i = 0; i < NUM_CHILDREN; i++) {
			map[i] = other.map[i];
			other.map[i] = nullptr;
		}
		bv = other.bv;
		other.bv.clear();
	}

public:

	//! Set this instance to storing a trie value.
	void set_value(ValueType new_value) {
		if (!value_active) {
			clear_open_links();
			new (&value_) ValueType();
			value_active = true;
		}
		value_ = new_value;
	}

	uint16_t get_bv() const {
		return bv.get();
	}

	const ValueType& value() const {
		if (value_active) {
			return value_;
		}
		throw std::runtime_error("can't get value from non leaf");
	}

	ValueType& value() {
		return value_;
	}

	//! leaves map as active union member
	void clear() {
		if (value_active) {
			value_active = false;
			value_.~ValueType();
		} else {
			clear_open_links();
		}
		bv.clear();
	}

	FixedChildrenMap(ValueType value) 
		: value_(value), bv{0}, value_active(true) {}

	FixedChildrenMap() : value_(), bv{0}, value_active(true) {}	

	FixedChildrenMap(FixedChildrenMap&& other) 
		: value_()
		, bv{other.bv}
		, value_active{other.value_active} {

		if (other.value_active) {
			value_ = other.value_;
		} else {
			value_.~ValueType();
			steal_ptr_map(other);
		}
	}

	FixedChildrenMap& operator=(FixedChildrenMap&& other) {
		if (other.value_active) {
			if (!value_active) {
				clear_open_links();
				new (&value_) ValueType();
				value_active = true;
			}
			value_ = other.value_;
		} else /* !other.value_active */{
			if (value_active) {
				value_.~ValueType();
				value_active = false;
			} else {
				clear_open_links();
			}
			steal_ptr_map(other);
		}
		return *this;
	}

	underlying_ptr_t at(uint8_t idx) {
		if (idx >= NUM_CHILDREN) {
			throw std::runtime_error("out of bounds!");
		}
		if (!map[idx]) {
			throw std::runtime_error("attempt to dereference null ptr");
		}
		return map[idx];
	}

	underlying_ptr_t operator[](uint8_t idx) {
		return map[idx];
	}

	~FixedChildrenMap() {
		if (value_active) {
			value_.~ValueType();
			value_active = false;
		} else {
			clear_open_links();
		}
	}

	/*! Implementation of a standard iterator class for the set of children
		nodes. Starts at lowest valued position and increases.
	*/
	template<bool is_const>
	struct iterator_ {
		bv_t bv;

		using ptr_t = typename std::conditional<
			is_const, const underlying_ptr_t, underlying_ptr_t>::type;

		ptr_t* map;

		kv_pair_t<ptr_t> operator*() {
			uint8_t branch = bv.lowest();
			return {branch, map[branch]};
		}

		template<bool other_const>
		bool operator==(const iterator_<other_const>& other) {
			return bv == other.bv;
		}

		iterator_& operator++(int) {
			bv.pop();
			return *this;
		}
	};

	using iterator = iterator_<false>;
	using const_iterator = iterator_<true>;

	//! Set an input pointer to be the child at offset branch_bits.
	void emplace(uint8_t branch_bits, trie_ptr_t&& ptr) {
		if (value_active) {
			throw std::runtime_error("can't emplace ptr if value active!");
		}
		if (bv.contains(branch_bits)) {
			delete map[branch_bits];
		}
		map[branch_bits] = ptr.release();
		bv.add(branch_bits);
	}

	//! Extract pointer to trie node for child with prefix extension 
	//! \a branch_bits
	trie_ptr_t extract(uint8_t branch_bits) {
		if (value_active) {
			throw std::runtime_error("can't erase if value active");
		}

		auto* out = map[branch_bits];
		if (!bv.contains(branch_bits)) {
			std::printf("bad extraction of bb %u! bv was %x\n"
				, branch_bits, bv.get());
			throw std::runtime_error("can't extract invalid node!");
		}
		bv.erase(branch_bits);

		std::unique_ptr<typename trie_ptr_t::element_type> out_ptr(out);
		return out_ptr;
	}

	//! Erase child at position \a loc from map
	iterator erase(iterator loc) {
		if (value_active) {
			throw std::runtime_error("can't erase if value active");
		}

		auto bb = (*loc).first;
		if (bv.contains(bb)) {
			delete map[bb];
			bv.erase(bb);
			loc++;
		} else {
			throw std::runtime_error("cannot erase nonexistent iter!");
		}

		return loc;
	}

	//! Erase child at position \a loc (i.e. with prefix extension loc)
	void erase(uint8_t loc) {
		if (bv.contains(loc)) {
			delete map[loc];
		} else {
			throw std::runtime_error("cannot erase nonexistent loc!");
		}
		bv.erase(loc);
	}

	iterator begin() {
		if (!bv.empty()) {
			return iterator{bv, map};
		}
		return end();
	}

	const_iterator begin() const {
		if (!bv.empty()) {
			return const_iterator{bv, map};
		}
		return cend();
	}

	constexpr static iterator end() {
		return iterator{0, nullptr};
	}

	constexpr static const_iterator cend() {
		return const_iterator{0, nullptr};
	}


	//! Find the child at offset \a bb.
	iterator find(uint8_t bb) {
		if (bv.contains(bb)) {
			return iterator{bv.drop_lt(bb), map};
		}
		return end();
	}

	//! Find the child at offset \a bb.
	const_iterator find(uint8_t bb) const {
		if (bv.contains(bb)) {
			return const_iterator{bv.drop_lt(bb), map};
		}
		return cend();
	}

	bool empty() const {
		return bv.empty();
	}

	//! Return the number of active child nodes.
	size_t size() const {
		return bv.size();
	}
};


} /* speedex */