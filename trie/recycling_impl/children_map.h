#pragma once

#include <cstdint>

#include "trie/bitvector.h"
#include "trie/utils.h"

#include "trie/recycling_impl/allocator.h"

#include "utils/debug_macros.h"

namespace speedex {

template<typename ptr_t>
struct bb_ptr_pair_t {
	uint8_t first;
	ptr_t second;
};

//doesn't actually do memory management for children.
template<typename ValueType, typename NodeT>
class AccountChildrenMap {

public:

	constexpr static uint8_t NUM_CHILDREN = 16;
	constexpr static uint8_t BRANCH_BITS = 4;


	using ptr_t = uint32_t;
	using value_ptr_t = uint32_t;
	using bv_t = BitVector;

private:

	using allocator_t = AllocationContext<NodeT>;

	struct ChildrenPtrs {
		ptr_t base_ptr_offset;

		bv_t bv;

		ChildrenPtrs() 
			: base_ptr_offset(UINT32_MAX)
			, bv(0) {}

		void allocate(allocator_t& allocator) {
			base_ptr_offset = allocator.allocate(NUM_CHILDREN);
			bv.clear();
		}

		ChildrenPtrs& operator=(ChildrenPtrs&& other) {
			base_ptr_offset = other.base_ptr_offset;
			other.base_ptr_offset = UINT32_MAX;
			bv = other.bv;
			other.bv.clear();
			return *this;
		}

		void set_child(uint8_t branch_bits, ptr_t ptr, allocator_t& allocator) {
			auto child_ptr = base_ptr_offset + branch_bits;
			auto& child = allocator.get_object(child_ptr);
			child.set_as_empty_node();
			child.set_to(allocator.get_object(ptr), child_ptr);
			bv.add(branch_bits);
		}

		NodeT& init_new_child(uint8_t branch_bits, allocator_t& allocator) {
			auto child_ptr = base_ptr_offset + branch_bits;
			bv.add(branch_bits);
			auto& ref = allocator.get_object(child_ptr);
			ref.set_as_empty_node();
			return ref;
		}

		ptr_t extract(uint8_t branch_bits) {
			if (!bv.contains(branch_bits)) {
				std::printf("bad extraction of bb %u! bv was %x\n", branch_bits, bv.get());
				throw std::runtime_error("can't extract invalid node!");
			}
			bv.erase(branch_bits);
			return base_ptr_offset + branch_bits;
		}

		ptr_t at(uint8_t branch_bits) {
			if (!bv.contains(branch_bits)) {
				std::printf("bad access of bb %u! bv was %4x\n", branch_bits, bv.get());
			}
			return base_ptr_offset + branch_bits;
		}

		void log(std::string padding) {
			LOG("%schildren map: bv 0x%x base_ptr_offset 0x%x", padding.c_str(), bv.get(), base_ptr_offset);
		}
	};

	union {
		value_ptr_t value_;
		ChildrenPtrs children;
		ptr_t moved_to_location;
	};

	enum {VALUE, MAP, STOLEN, CLEARED} tag;

	void steal_ptr_map(AccountChildrenMap&& other) {
		children = std::move(other.children);
	}

	void stolen_guard(const char* caller) const {
		if (tag == STOLEN) {
			std::printf("%s\n", caller);
			std::fflush(stdout);
			throw std::runtime_error("can't do ops on stolen nodes!");
		}
	}

	void steal_value(value_ptr_t steal_ptr) {
		stolen_guard("steal_value");
		if (tag != VALUE) {
			tag = VALUE;
		}
		value_ = steal_ptr;
	}

	value_ptr_t get_value_ptr() {
		if (tag != VALUE) {
			return UINT32_MAX;
		}
		return value_;
	}

public:
	void print_offsets() {
		using this_t = AccountChildrenMap<ValueType, NodeT>;
		std::printf("children: start %lu end %lu\n", offsetof(this_t, children), offsetof(this_t, children) + sizeof(children));
		std::printf("tag: start %lu end %lu\n", offsetof(this_t, tag), offsetof(this_t, tag) + sizeof(tag));
	}

	void set_value(allocator_t& allocator, const ValueType& value_input) {
		stolen_guard("set_value");
		if (tag != VALUE) {
			value_ = allocator.allocate_value();
			auto& ref = allocator.get_value(value_);
			ref = value_input; // TODO std::move?
			tag = VALUE;
		}
	}

	void set_map_noalloc() {
		stolen_guard("set_map_noalloc");
		if (tag != MAP) {
			tag = MAP;
			new (&children) ChildrenPtrs();
		}
	}

	void log(std::string padding) {
		if (tag == VALUE) {
			//do nothing for now
		} else if (tag == MAP) {
			children.log(padding);
		} else if (tag == STOLEN) {
			LOG("%sSTOLEN to %x", padding.c_str(), moved_to_location);
		} else {
			//tag == CLEARED
			LOG("%sCLEARED NODE!!!", padding.c_str());
		}
	}

	void reset_map(allocator_t& allocator) {
		set_map(allocator);
	}

	void set_map(allocator_t& allocator) {
		set_map_noalloc();
		children.allocate(allocator);
	}

	void set_stolen(ptr_t new_address) {
		tag = STOLEN;
		moved_to_location = new_address;
	}

	std::optional<ptr_t> check_stolen() {
		if (tag == STOLEN) {
			return moved_to_location;
		}
		return std::nullopt;
	}

	AccountChildrenMap() : value_(UINT32_MAX), tag{CLEARED} {}

	AccountChildrenMap(AccountChildrenMap&& other) : value_(UINT32_MAX), tag{CLEARED} {
		other.stolen_guard("move_ctor");
		if (other.tag == VALUE) {
			steal_value(other.get_value_ptr());
			tag = VALUE;
		} else if (other.tag == MAP) {
			set_map_noalloc();
			steal_ptr_map(std::move(other));
			tag = MAP;
		} else {
			tag = CLEARED;
		}
		other.tag = CLEARED;
	}

	AccountChildrenMap& operator=(AccountChildrenMap&& other) {
		other.stolen_guard("operator=");
		if (other.tag == VALUE) {
			steal_value(other.get_value_ptr());
		} else if (other.tag == MAP) {
			set_map_noalloc();
			steal_ptr_map(std::move(other));
		} else {
			tag = CLEARED;
		}
		other.tag = CLEARED;
		return *this;
	}

	ptr_t operator[](uint8_t idx) {
		if (tag == MAP) {
			return children.at(idx);//.base_ptr_offset + idx;
		}
		return UINT32_MAX;
	}

	uint16_t get_bv() const {
		if (tag == MAP) {
			return children.bv.get();
		}
		return 0;
	}

	template<typename allocator_or_context_t>
	const ValueType& value(const allocator_or_context_t& allocator) const {
		if (tag == VALUE) {
			return allocator.get_value(value_);
		}
		throw std::runtime_error("can't get value from non leaf (const)");
	}

	template<typename allocator_or_context_t>
	ValueType& value(const allocator_or_context_t& allocator) {
		if (tag == VALUE) {
			return allocator.get_value(value_);
		}
		throw std::runtime_error("can't get value from non leaf");
	}

	//leaves nothing set as active union member
	void clear() {
//
		if (tag == VALUE) {
			//nothing to do
		} else if (tag == MAP) {
			children.bv.clear();
		}
		tag = CLEARED;
	}

	template<bool is_const>
	struct iterator_ {
		bv_t bv;

		ptr_t base_map_offset;

		bb_ptr_pair_t<ptr_t> operator*() {
			uint8_t branch = bv.lowest();
			return {branch, base_map_offset + branch};
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

	void emplace(uint8_t branch_bits, ptr_t ptr, allocator_t& allocator) {
		children.set_child(branch_bits, ptr, allocator);
	}

	NodeT& init_new_child(uint8_t branch_bits, allocator_t& allocator) {
		map_guard();
		return children.init_new_child(branch_bits, allocator);
	}

	void map_guard() {
		if (tag != MAP) {
			throw std::runtime_error("accessed MAP method when MAP not set!");
		}
	}

	ptr_t extract(uint8_t branch_bits) {
		map_guard();
		return children.extract(branch_bits);
	}

	iterator erase(iterator loc) {
		map_guard();
		auto bb = (*loc).first;
		children.extract(bb); // throws error on nonexistence
		loc++;
		return loc;
	}

	void erase(uint8_t loc) {
		map_guard();
		children.extract(loc);
	}

	iterator begin() {
		if (tag != MAP) {
			return end();
		}
		if (!children.bv.empty()) {
			return iterator{children.bv, children.base_ptr_offset};
		}
		return end();
	}

	const_iterator begin() const {
		if (tag != MAP) {
			return cend();
		}
		if (!children.bv.empty()) {
			return const_iterator{children.bv, children.base_ptr_offset};
		}
		return cend();
	}

	constexpr static iterator end() {
		return iterator{0, UINT32_MAX};
	}

	constexpr static const_iterator cend() {
		return const_iterator{0, UINT32_MAX};
	}

	iterator find(uint8_t bb) {
		if (tag != MAP) {
			return end();
		}
		if (children.bv.contains(bb)) {
			return iterator{children.bv.drop_lt(bb), children.base_ptr_offset};
		}
		return end();
	}

	const_iterator find(uint8_t bb) const {
		if (tag != MAP) {
			return cend();
		}
		if (children.bv.contains(bb)) {
			return const_iterator{children.bv.drop_lt(bb), children.base_ptr_offset};
		}
		return cend();
	}

	bool empty() const {
		stolen_guard("empty");
		if (tag != MAP) {
			return true;
		}
		return children.bv.empty();
	}

	size_t size() const {
		stolen_guard("size");
		if (tag != MAP) {
			return 0;
		}
		return children.bv.size();
	}
};


} /* speedex */