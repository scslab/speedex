#pragma once

#include <cstdint>

namespace speedex {


struct AccountTrieAllocatorConstants {
	constexpr static size_t BUF_SIZE = 500'000;

	constexpr static uint8_t BUFFER_ID_BITS = 8;
	constexpr static uint8_t OFFSET_BITS = 24;

	constexpr static uint32_t OFFSET_MASK 
		= (((uint32_t)1) << (OFFSET_BITS)) - 1;

	static_assert(BUFFER_ID_BITS + OFFSET_BITS == 32, "ptrs are size 32 bits");
};

template<typename ObjType>
struct AccountTrieNodeAllocator;

/*! Singlethreaded node/value allocator.
When it runs out, it asks the main node allocator for another working buffer.
Allocations are never recycled, until the main node allocator is cleared
(after which a context should not be used until reset)
*/
template<typename ObjType>
class AllocationContext {
	uint32_t cur_buffer_offset_and_index = UINT32_MAX;
	uint32_t value_buffer_offset_and_index = UINT32_MAX;

	AccountTrieNodeAllocator<ObjType>& allocator;

	constexpr static size_t BUF_SIZE 
		= AccountTrieAllocatorConstants::BUF_SIZE;
	constexpr static uint8_t OFFSET_BITS 
		= AccountTrieAllocatorConstants::OFFSET_BITS;
	constexpr static uint32_t OFFSET_MASK 
		= AccountTrieAllocatorConstants::OFFSET_MASK;
	using value_t = typename ObjType::value_t;

public:

	AllocationContext(
		uint32_t cur_buffer_offset_and_index, 
		uint32_t value_buffer_offset_and_index, 
		AccountTrieNodeAllocator<ObjType>& allocator) 
		: cur_buffer_offset_and_index(cur_buffer_offset_and_index)
		, value_buffer_offset_and_index(value_buffer_offset_and_index)
		, allocator(allocator) {}

	uint32_t allocate(uint8_t num_nodes) {
		if (((cur_buffer_offset_and_index + num_nodes) & OFFSET_MASK) > BUF_SIZE) {
			allocator.assign_new_buffer(*this);
		}

		uint32_t out = cur_buffer_offset_and_index;
		cur_buffer_offset_and_index += num_nodes;
		return out;
	}

	uint32_t allocate_value() {
		if (((value_buffer_offset_and_index + 1) & OFFSET_MASK) > BUF_SIZE) {
			allocator.assign_new_value_buffer(*this);
		}
		uint32_t out = value_buffer_offset_and_index;
		value_buffer_offset_and_index += 1;
		return out;
	}

	void set_cur_buffer_offset_and_index(uint32_t value) {
		cur_buffer_offset_and_index = value;
	}

	void set_cur_value_buffer_offset_and_index(uint32_t value) {
		value_buffer_offset_and_index = value;
	}

	uint32_t init_root_node() {
		auto ptr = allocate(1);
		auto& node = get_object(ptr);
		node.set_as_empty_node();
		return ptr;
	}

	ObjType& get_object(uint32_t ptr) const {
		return allocator.get_object(ptr);
	}

	value_t& get_value(uint32_t ptr) const {
		return allocator.get_value(ptr);
	}
};

/*! Manages a group of allocation contexts.
New contexts can be requested from this object,
and when those allocation contexts use up their buffers,
this node grants additional buffers.

Allocations are not freed until the whole allocator is reset.
After resetting, created contexts should be nullified.

This struct is threadsafe.
*/
template<typename ObjType>
struct AccountTrieNodeAllocator {
	constexpr static size_t BUF_SIZE 
		= AccountTrieAllocatorConstants::BUF_SIZE;
	constexpr static uint8_t OFFSET_BITS 
		= AccountTrieAllocatorConstants::OFFSET_BITS;
	constexpr static uint32_t OFFSET_MASK 
		= AccountTrieAllocatorConstants::OFFSET_MASK;

	using buffer_t = std::array<ObjType, BUF_SIZE>;

	using value_t = typename ObjType::value_t;

	using value_buffer_t = std::array<value_t, BUF_SIZE>;

private:
	std::atomic<uint16_t> next_available_buffer = 0;
	std::atomic<uint16_t> next_available_value_buffer = 0;

	using buffer_ptr_t = std::unique_ptr<buffer_t>;

	std::array<buffer_ptr_t, 256> buffers;

	using value_buffer_ptr_t = std::unique_ptr<value_buffer_t>;

	std::array<value_buffer_ptr_t, 256> value_buffers;

	using context_t = AllocationContext<ObjType>;

public:

	//! Get a new allocation context
	context_t get_new_allocator() {
		uint16_t idx = next_available_buffer.fetch_add(
			1, std::memory_order_relaxed);
		if (idx >= buffers.size()) {
			throw std::runtime_error("used up all allocation buffers!!!");
		}

		if (!buffers[idx]) {
			buffers[idx] = std::make_unique<buffer_t>();
		}

		uint16_t value_buffer_idx = next_available_value_buffer.fetch_add(
			1, std::memory_order_relaxed);
		if (value_buffer_idx >= value_buffers.size()) {
			throw std::runtime_error("used up all value buffers");
		}

		if (!value_buffers[value_buffer_idx]) {
			value_buffers[value_buffer_idx] 
				= std::make_unique<value_buffer_t>();
		}

		return context_t(
			((uint32_t) idx) << OFFSET_BITS,
			((uint32_t) value_buffer_idx) << OFFSET_BITS,
			*this);
	}

	//! Give a context a new trie node buffer
	void assign_new_buffer(context_t& context) {
		uint16_t idx = next_available_buffer.fetch_add(
			1, std::memory_order_relaxed);
		if (idx >= buffers.size()) {
			throw std::runtime_error("used up all allocation buffers!!!");
		}

		if (!buffers[idx]) {
			buffers[idx] = std::make_unique<buffer_t>();
		}

		context.set_cur_buffer_offset_and_index(
			((uint32_t) idx) << OFFSET_BITS);
	}

	//! Give a context a new trie value buffer
	void assign_new_value_buffer(context_t& context) {
		uint16_t value_buffer_idx = next_available_value_buffer.fetch_add(
			1, std::memory_order_relaxed);
		if (value_buffer_idx >= value_buffers.size()) {
			throw std::runtime_error("used up all value buffers");
		}

		if (!value_buffers[value_buffer_idx]) {
			value_buffers[value_buffer_idx] 
				= std::make_unique<value_buffer_t>();
		}

		context.set_cur_value_buffer_offset_and_index(
			((uint32_t) value_buffer_idx) << OFFSET_BITS);
	}

	//! Access a particular node, given a handle
	ObjType& get_object(uint32_t ptr) const {
		uint8_t idx = ptr >> OFFSET_BITS;
		uint32_t offset = ptr & OFFSET_MASK;
		return (*buffers[idx])[offset];
	}

	//! Access a particular trie value, given a handle
	value_t& get_value(uint32_t value_ptr) const {
		uint8_t idx = value_ptr >> OFFSET_BITS;
		uint32_t offset = value_ptr & OFFSET_MASK;
		return (*value_buffers[idx])[offset];
	}

	//! Reset the allocator.  All contexts should be cleared or deleted.
	void reset() {
		next_available_buffer = 0;
		next_available_value_buffer = 0;
	}
};

} /* speedex */