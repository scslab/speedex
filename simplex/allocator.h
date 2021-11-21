#pragma once

#include <memory>
#include <set>
#include <vector>
#include <stdexcept>
#include <cstdint>

namespace speedex {

class AllocatorRow {
	//low 16 bits are value, upper 48 for ptr (32 for buffer, 16 for idx)

	constexpr static bool small_allocs = false;

	constexpr static uint32_t NUM_ELTS_TO_ALLOC = small_allocs ? 0x4 : 0x1'0000;
	constexpr static uint32_t MAX_INDEX = small_allocs ? 0x3 : UINT16_MAX;
	constexpr static uint64_t MAX_INDEX_SHIFTED = static_cast<uint64_t>(MAX_INDEX) << 16;
	mutable std::array<uint64_t, NUM_ELTS_TO_ALLOC> data;
	uint64_t next_free_slot;
	uint32_t usage_count;

	const uint64_t upper_bits;


public:

	AllocatorRow(uint64_t buffer_idx)
		: data()
		, next_free_slot(0)
		, usage_count(0)
		, upper_bits(buffer_idx << 32) {}

	// returns ptr to next obj
	uint64_t allocate() {
		uint64_t out = upper_bits + next_free_slot;
		//std::printf("allocation of %llx (from this=%p, ub=%llx, next_slot=%llx)\n", out, this, upper_bits, next_free_slot);
		next_free_slot+=0x1'0000;
		usage_count++;
		return out;
	}

	bool full() const {
		return next_free_slot == MAX_INDEX_SHIFTED;
	}
	void free() {
		usage_count--;
	}
	bool ready_for_refresh() const {
		return (usage_count == 0) && (full());
	}

	uint64_t* get(uint16_t idx) const {
		return &(data[idx]);
	}

	void clear() {
		usage_count = 0;
		next_free_slot = 0;
	}
};
/*
union AddressPair {
	struct {
		uint16_t offset;
		uint16_t buffer_idx;
	} a;
	uint32_t raw;
};

union ValuesPair {
	struct {
		uint16_t v1;
		uint16_t v2;
	} v;
	uint32_t raw;
}; */

//UINT16_MAX is considered invalid
/*union Entry 
{
	struct {
		AddressPair next;
		ValuesPair values;

		bool has_v2() const {
			return values.v.v2 != UINT16_MAX;
		}
		bool is_before_begin_ptr() const {
			return values.v.v1 == UINT16_MAX;
		}
	} e;
	uint64_t raw;
}; */

using AddressPair = uint32_t;

// [buf_idx_bits] [offset] [v1] [v2]

/* struct Entry {
	uint64_t raw;

	bool has_v2() const {
		return (raw & 0xFFFF) != 0xFFFF;
	}

	bool is_before_begin() const {
		return (raw & 0xFFFF'0000) == 0xFFFF'0000;
	}

	uint16_t buf_idx() const {
		return raw >> 48;
	}

	uint16_t offset() const {
		return (raw >> 32) & 0xFFFF;
	}

	void set_address(AddressPair addr) {
		raw = (raw & 0xFFFF'FFFF) + (static_cast<uint64_t>(addr) << 32);
	}

	uint16_t v1() const {
		return (raw >> 16) & 0xFFFF;
	}
	uint16_t v2() const {
		return raw & 0xFFFF;
	}

	void set_v1(uint16_t v) {
		raw = (raw & 0xFFFF'FFFF'0000'FFFF) | (static_cast<uint64_t>(v) << 16);
	}
	void set_v2(uint16_t v) {
		raw = (raw & 0xFFFF'FFFF'FFFF'0000) | (static_cast<uint64_t>(v));
	}

	void copy_addr(Entry const& other) {
		raw = (raw & 0xFFFF'FFFF) | (other.raw & 0xFFFF'FFFF'0000'0000);
	}

	void set_addr(AddressPair addr) {
		raw = (raw & 0xFFFF'FFFF) | (static_cast<uint64_t>(addr) << 32);
	}
}; */

struct Entry {
	AddressPair addr;
	uint16_t _v1, _v2;

	bool has_v2() const {
		return _v2 != UINT16_MAX;
	}

	bool is_before_begin() const {
		return _v1 == UINT16_MAX;
	}

	uint16_t buf_idx() const {
		return addr >> 16;
	}

	uint16_t offset() const {
		return (addr) & 0xFFFF;
	}

	void set_address(AddressPair _addr) {
		addr = _addr;
		//raw = (raw & 0xFFFF'FFFF) + (static_cast<uint64_t>(addr) << 32);
	}

	uint16_t v1() const {
		return _v1;
	}
	uint16_t v2() const {
		return _v2;
		//return raw & 0xFFFF;
	}

	void set_v1(uint16_t v) {
		_v1 = v;
		//raw = (raw & 0xFFFF'FFFF'0000'FFFF) | (static_cast<uint64_t>(v) << 16);
	}
	void set_v2(uint16_t v) {
		_v2 = v;
		//raw = (raw & 0xFFFF'FFFF'FFFF'0000) | (static_cast<uint64_t>(v));
	}

	void copy_addr(Entry const& other) {
		addr = other.addr;
		//raw = (raw & 0xFFFF'FFFF) | (other.raw & 0xFFFF'FFFF'0000'0000);
	}

	void set_addr(AddressPair _addr) {
		addr = _addr;
		//raw = (raw & 0xFFFF'FFFF) | (static_cast<uint64_t>(addr) << 32);
	}
};



//using ValuePair = std::pair<uint16_t, uint16_t>;
//using AddressPair = std::pair<uint16_t, uint16_t>;

// pointer format: 16 bits (buffer idx) 16 bits offset in buffer
//using Entry = std::pair<AddressPair, ValuePair>;

class CompressedAllocatorRow {



	static_assert(sizeof(Entry) == 8);

	constexpr static bool small_allocs = false;

	constexpr static uint32_t NUM_ENTRIES = small_allocs ? 0x4 : 0x1'0000;
	constexpr static uint32_t MAX_ENTRY_IDX = small_allocs ? 0x3 : UINT16_MAX;

	mutable std::array<Entry, NUM_ENTRIES> data;
	uint32_t next_free_slot;
	uint32_t usage_count;

	const uint32_t upper_bits;

public:

	CompressedAllocatorRow(uint32_t buffer_idx)
		: data()
		, next_free_slot(0)
		, usage_count(0)
		, upper_bits(buffer_idx << 16) {}

	// returns ptr to next obj
	AddressPair allocate() {


		AddressPair out = upper_bits + next_free_slot;
		//uint32_t out = upper_bits + next_free_slot;
		next_free_slot++;
		usage_count++;
		return out;//static_cast<AddressPair>(out);
	}

	bool full() const {
		return next_free_slot == MAX_ENTRY_IDX;
	}

	void free() {
		usage_count--;
	}

	bool ready_for_refresh() const {
		return (usage_count == 0) && (full());
	}

	Entry* get(uint16_t idx) const {
		return &(data[idx]);
	}

	void clear() {
		usage_count = 0;
		next_free_slot = 0;
	}
};

class CompressedAllocator {
	std::vector<std::unique_ptr<CompressedAllocatorRow>> buffers;

	std::vector<uint16_t> nonfull_buffers;

public:

	CompressedAllocator() 
		: buffers()
		, nonfull_buffers()
		{
			buffers.emplace_back(std::make_unique<CompressedAllocatorRow>(1));
			nonfull_buffers.push_back(0);
		}

	Entry* get(const Entry& addr) {
	//	std::printf("get %x (buffer_idx = %x offset = %x)\n", addr.raw, addr.a.buffer_idx, addr.a.offset);
		//uint32_t buffer_idx = addr >> 16;

		uint16_t buffer_idx = addr.buf_idx();
		uint16_t offset = addr.offset();
		if (buffer_idx == 0) {
			return nullptr;
		}
		return buffers[buffer_idx - 1]->get(offset);
	}

	Entry* get(AddressPair addr) {
		uint16_t buffer_idx = addr >> 16;
		uint16_t offset = addr & 0xFFFF;
		if (buffer_idx == 0) {
			return nullptr;
		}
		return buffers[buffer_idx - 1]->get(offset);
	}

	AddressPair allocate() {
		AddressPair out = buffers[nonfull_buffers.back()]->allocate();

		if (buffers[nonfull_buffers.back()]->full()) {
			nonfull_buffers.pop_back();

			if (nonfull_buffers.empty()) {
				uint16_t next_buffer_idx = buffers.size();
				buffers.emplace_back(std::make_unique<CompressedAllocatorRow>(next_buffer_idx + 1));
				nonfull_buffers.push_back(next_buffer_idx);
			}
		}
		return out;
	}

	void free(Entry const& entry) {
		uint32_t buf_idx = entry.buf_idx() - 1;
		buffers[buf_idx]->free();
		if (buffers[buf_idx]->ready_for_refresh()) {
			buffers[buf_idx]->clear();
			nonfull_buffers.push_back(buf_idx);
		}
	}

	void free(AddressPair addr) {
		uint32_t buf_idx = (addr >> 16) - 1;
		buffers[buf_idx]->free();
		if (buffers[buf_idx]->ready_for_refresh()) {
			buffers[buf_idx]->clear();
			nonfull_buffers.push_back(buf_idx);
		}
	}

	void clear() {
		nonfull_buffers.clear();
		for (size_t i = 0; i < buffers.size(); i++) {
			buffers[i]->clear();
			nonfull_buffers.push_back(i);
		}
	}

};

class Allocator {
	std::vector<std::unique_ptr<AllocatorRow>> buffers;

	std::vector<uint32_t> nonfull_buffers;

	//std::set<uint64_t> active_buffers;

public:

	Allocator()
		: buffers()
		, nonfull_buffers()
		//, active_buffers()
		{
			buffers.emplace_back(std::make_unique<AllocatorRow>(1));
			nonfull_buffers.push_back(0);
		}

	uint64_t* get(uint64_t addr) const {
		uint32_t buffer_idx = addr >> 32;
		if (buffer_idx == 0) {
			return nullptr;
		}
		return buffers[buffer_idx - 1]->get((addr & (0xFFFF'0000)) >> 16);
	}

	uint64_t allocate() {

//		if (nonfull_buffers.size() == 0) {
//			throw std::runtime_error("impossible");
//		}

//		std::printf("alloc from buffer %llu (%llx) \n", nonfull_buffers.back(), nonfull_buffers.back());

		uint64_t out = buffers[nonfull_buffers.back()]->allocate();
		//active_buffers.insert(out);

		if (buffers[nonfull_buffers.back()]->full()) {
//			std::printf("buffer %lu was full\n", nonfull_buffers.back());
//			std::printf("fetching new buffer, cur size was %lu (nonfull was %lu, back %lu)\n", buffers.size(), nonfull_buffers.size(), nonfull_buffers.back());
			nonfull_buffers.pop_back();

			if (nonfull_buffers.empty()) {
		//		std::printf("alloc new buffer\n");
				uint32_t next_buffer_idx = buffers.size();
				buffers.emplace_back(std::make_unique<AllocatorRow>(next_buffer_idx + 1));
				nonfull_buffers.push_back(next_buffer_idx);
			}
		}
		return out;
	}

	void free(uint64_t addr) {
		uint32_t buf_idx = (addr >> 32) - 1;
		buffers[buf_idx]->free();


	//	uint64_t ptr = addr & (0xFFFF'FFFF'FFFF'0000);
	//	std::printf("free on %llx (idx %lu)\n", ptr, buf_idx);

	//	auto it = active_buffers.find(ptr);
	//	if (it == active_buffers.end()) {
	//		throw std::runtime_error("mismatch");
	//	}
	//	active_buffers.erase(it);

		//if (freed_buffers.find(addr) != freed_buffers.end()) {
		//	std::printf("double free on %llx\n", addr);
		//	for (auto val : freed_buffers) {
		//		std::printf("\tfreed_buffers=%llx\n", val);
		//	}
		//} else {
		//	std::printf("free on %llx\n", addr);
		//}

		//freed_buffers.insert(addr);
		if (buffers[buf_idx]->ready_for_refresh()) {
			buffers[buf_idx]->clear();
	//		std::printf("buffer %lu is now free, adding it to list\n", buf_idx);

	//		for (auto & idx : nonfull_buffers) {
//
//				if (idx == buf_idx) {
//					std::printf("double add on buffer %lu\n", buf_idx);
//					throw std::runtime_error("wtf");
//				}
//			}
			nonfull_buffers.push_back(buf_idx);
		}
	}

	static uint16_t 
	get_value(uint64_t addr) {
		return addr & 0xFFFF;
	}

	static void
	set_value(uint64_t& addr, uint16_t val) {
		addr = (addr & 0xFFFF'FFFF'FFFF'0000) | (static_cast<uint64_t>(val));
	}

	static void
	copy_next_obj_ptr(uint64_t& mod_addr, uint64_t const& next_addr) {
		mod_addr = (mod_addr & 0xFFFF) | (next_addr & 0xFFFF'FFFF'FFFF'0000);
	}

	void clear() {
		nonfull_buffers.clear();
		for (size_t i = 0; i < buffers.size(); i++) {
			buffers[i]->clear();
			nonfull_buffers.push_back(i);
		}
	//	active_buffers.clear();
		//std::printf("clearing free buffers, %lu\n", freed_buffers.size());
	}
};

extern Allocator alloc;
extern CompressedAllocator c_alloc;

class compressed_forward_list {

	AddressPair before_list_elt_addr;

	//constexpr static Entry empty_entry {
	//	.raw = 0x0000'0000'FFFF'FFFF
	//};

	constexpr static Entry empty_entry {
		.addr = 0,
		._v1 = UINT16_MAX,
		._v2 = UINT16_MAX
	};

	/*constexpr static Entry empty_entry {
		.e = {
			.next = {
				.a = {
					.offset = 0,
					.buffer_idx = 0
				}
			},
			.values = {
				.v = {
					.v1 = UINT16_MAX,
					.v2 = UINT16_MAX
				}
			}
		}
	}; */

public:

	compressed_forward_list()
		: before_list_elt_addr(c_alloc.allocate())
		{
			*c_alloc.get(before_list_elt_addr) = empty_entry;//-> raw = empty_entry.raw;
		}

	compressed_forward_list(compressed_forward_list&& other)
		: before_list_elt_addr(other.before_list_elt_addr)
		{
			other.before_list_elt_addr = 0;
		}

	~compressed_forward_list() {
		if (before_list_elt_addr != 0) {
			c_alloc.free(before_list_elt_addr);
		}
	}

	class iterator {
		Entry* cur_entry;
		bool skip_first;

	public:


		constexpr iterator(Entry* cur_entry, bool skip_first = false)
			: cur_entry(cur_entry)
			, skip_first(skip_first)
			{
			}

		iterator& operator++() {

			if (skip_first || (!cur_entry ->has_v2())) {
				cur_entry = c_alloc.get(*cur_entry);
				skip_first = false;
			} else {
				skip_first = true;
			}
			return *this;
		}

		iterator& operator++(int) {
			return ++(*this);
		}

		uint16_t operator*() const {

			//std::printf("deref on iter %p (skip=%d) (raw %llx)\n", cur_entry, skip_first, cur_entry -> raw);
			if (skip_first) {
				return cur_entry -> v2();// e.values.v.v2;
			} else {
				return cur_entry -> v1(); // e.values.v.v1;
			}
		}

		bool operator==(compressed_forward_list::iterator const& other) const {
			return (cur_entry == other.cur_entry) && (skip_first == other.skip_first);
		}
		bool operator!=(compressed_forward_list::iterator const& other) const = default;


		//insert right after *cur_obj
		iterator insert_after(uint16_t value) {
		//	std::printf("inserting %u to iterator %p skip=%d (%llx, v1=%u, v2=%u)\n", 
		//		value, cur_entry, skip_first, cur_entry->raw, cur_entry -> e.values.v.v1, cur_entry -> e.values.v.v2);


			if (cur_entry -> has_v2()) {
				AddressPair new_addr = c_alloc.allocate();

				Entry *new_entry = c_alloc.get(new_addr);

				new_entry -> set_v2(UINT16_MAX);
				//new_entry -> e.values.v.v2 = UINT16_MAX;

				new_entry -> copy_addr(*cur_entry);
				//new_entry -> e.next.raw = cur_entry -> e.next.raw;
				cur_entry -> set_addr(new_addr);
				//cur_entry -> e.next.raw = new_addr.raw;

				if (skip_first) {
					new_entry -> set_v1(value);
					//new_entry -> e.values.v.v1 = value;
					return iterator(new_entry);
				} else {
					new_entry -> set_v1(cur_entry -> v2());
					//new_entry -> e.values.v.v1 = cur_entry -> e.values.v.v2;
					cur_entry -> set_v2(value);
					//cur_entry -> e.values.v.v2 = value;
					return iterator(cur_entry, true);
				}
			} else {
				cur_entry -> set_v2(value);
				//cur_entry -> e.values.v.v2 = value;
				return iterator(cur_entry, true);
			}
		}

		void print_iter() {
			if (cur_entry == nullptr) {
				std::printf("iter is nullptr\n");
				return;
			}
			//std::printf("print iterator %p skip=%d (%llx, v1=%u, v2=%u)\n", 
			//	cur_entry, skip_first, cur_entry->raw, cur_entry -> e.values.v.v1, cur_entry -> e.values.v.v2);
		}


		iterator erase_after() {

			//std::printf("erasing from to iterator %p skip=%d (%llx, v1=%u, v2=%u)\n", 
			//	cur_entry, skip_first, cur_entry->raw, cur_entry -> e.values.v.v1, cur_entry -> e.values.v.v2);


			// if we're pointing at first elt in pair, and there's a second elt,
			// just delete the second elt;
			if ((!skip_first) && cur_entry -> has_v2()) {
				cur_entry -> set_v2(UINT16_MAX);
				//cur_entry -> e.values.v.v2 = UINT16_MAX;

				if (!cur_entry -> is_before_begin()) {
					return iterator(c_alloc.get(*cur_entry));
				} else {
					Entry* next_obj = c_alloc.get(*cur_entry);
					if (next_obj == nullptr) {
						return iterator(nullptr);
					}

					cur_entry -> set_v2(next_obj->v1());
					//cur_entry -> e.values.v.v2 = next_obj -> e.values.v.v1;

					if (next_obj -> has_v2()) {
						next_obj -> set_v1(next_obj -> v2());
						next_obj -> set_v2(UINT16_MAX);
						//next_obj -> e.values.v.v1 = next_obj->e.values.v.v2;
						//next_obj -> e.values.v.v2 = UINT16_MAX;
					} else {
						c_alloc.free(*cur_entry);
						cur_entry -> copy_addr(*next_obj);
						//cur_entry -> e.next.raw = next_obj -> e.next.raw;
						//return iterator(c_alloc.get(cur_entry -> e.next));
					}
					return iterator(cur_entry, true);

				}
			}	

			// Otherwise, there's either no second elt, or there's a second elt
			// and we're pointing to it already.
			// In either case delete first elt of next, and possibly delete all of next.

			Entry* next_obj = c_alloc.get(*cur_entry);

			if (next_obj -> has_v2()) {
				next_obj -> set_v1(next_obj->v2());
				next_obj -> set_v2(UINT16_MAX);
				//next_obj -> e.values.v.v1 = next_obj->e.values.v.v2;
				//next_obj -> e.values.v.v2 = UINT16_MAX;
				return iterator(next_obj);
			} else {

				c_alloc.free(*cur_entry);

				cur_entry -> copy_addr(*next_obj);
				//cur_entry -> e.next.raw = next_obj -> e.next.raw;

				return iterator(c_alloc.get(*cur_entry));
			}
		}
	};

	constexpr static iterator end() {
		return iterator(nullptr);
	}

	iterator begin() const {

		Entry* first_elt = c_alloc.get(before_list_elt_addr);
		if (first_elt -> has_v2()) {
			return iterator(first_elt, true);
		} else {
			return iterator(nullptr);
		}
		//safe to deref first bc guaranteed to exist by ctor
		/*Entry* first_elt = c_alloc.get(before_list_elt_addr);

		if (first_elt == nullptr) {
			return end();
		}

		return iterator(c_alloc.get(first_elt -> e.next));*/
	}

	iterator before_begin() const {
		return iterator(c_alloc.get(before_list_elt_addr));
	}

	void clear() {
		auto back_it = before_begin();
		auto it = begin();
		while (it != end()) {
			it = back_it.erase_after();
		}
	}
};

class buffered_forward_list {

	uint64_t before_list_elt_addr;

public:

	buffered_forward_list()//Allocator& alloc)
		//: alloc(alloc)
		: before_list_elt_addr(alloc.allocate())
		{
			*(alloc.get(before_list_elt_addr)) = 0;
		}

	buffered_forward_list(buffered_forward_list&& other)
		//: alloc(other.alloc)
		: before_list_elt_addr(other.before_list_elt_addr)
		{
			other.before_list_elt_addr = 0;
		}

	~buffered_forward_list() {
		if (before_list_elt_addr != 0) {
			alloc.free(before_list_elt_addr);
		}
	}

	class iterator {
	//	Allocator* alloc;

		uint64_t* cur_object;
		//uint16_t cached_value;

	public:

		constexpr iterator(uint64_t* cur_object)
		//	: alloc(alloc)
			: cur_object(cur_object)
			//, cached_value(0)
			{
				//if (cur_object != nullptr) {
				//	cached = *cur_object;//Allocator::get_value(*cur_object);
				//}
			}

		iterator& operator++() {
			cur_object = alloc.get( *cur_object);
			//if (cur_object != nullptr) {
			//	cached = *cur_object;//Allocator::get_value(*cur_object);
			//}

			return *this;
		}
		iterator& operator++(int) {
			return ++(*this);
		}

		uint16_t operator*() const {
		//	std::printf("deref obj %p (%llx)\n", cur_object, *cur_object);
			return Allocator::get_value(*cur_object);
		}

		bool operator==(buffered_forward_list::iterator const& other) const {
			return cur_object == other.cur_object;
		}
		bool operator!=(buffered_forward_list::iterator const& other) const = default;

		void print(std::string desc = "") {
			if (cur_object != nullptr) {
				std::printf("%s %p %llx\n", desc.c_str(), cur_object, *cur_object);
			} else {
				std::printf("%s %p (null)\n", desc.c_str(), cur_object);
			}
		}

		void print_iter() {
			print();
		}

		//insert right after *cur_obj
		iterator insert_after(uint16_t value) {

		//	std::printf("insert after original obj: %p %llx\n", cur_object, *cur_object);

			uint64_t new_obj_addr = alloc.allocate();
		//	std::printf("new_obj_addr %llx\n", new_obj_addr);
			uint64_t* new_obj = (alloc.get(new_obj_addr));

		//	if (new_obj == nullptr) {
		//		throw std::runtime_error("wtf");
		//	}

		//	std::printf("new_obj: %p, %llx\n", &new_obj, new_obj);

			Allocator::copy_next_obj_ptr(*new_obj,*cur_object);
			Allocator::set_value(*new_obj, value);

			Allocator::copy_next_obj_ptr(*cur_object, new_obj_addr);
			//cached = *cur_object;

		//	std::printf("post cur_obj %llx\n", *cur_object);
		//	std::printf("new obj: %llx\n", new_obj);

			return iterator(new_obj);
		}

		iterator erase_after() {
			if (cur_object == nullptr) {
				throw std::runtime_error("can't erase after list end");
			}
			//assumption is that next obj actually exists
			uint64_t& next_obj = *(alloc.get(*cur_object));

			//frees the location to which cur_obj points
			alloc.free(*cur_object);

			Allocator::copy_next_obj_ptr(*cur_object, next_obj);
			//cached = *cur_object;

			return iterator(alloc.get(*cur_object));
		}
	};

	constexpr static iterator end() {
		return iterator(nullptr);
	}

	iterator begin() const {
		//safe to deref first bc guaranteed to exist by ctor
		uint64_t first_elt_addr = *alloc.get(before_list_elt_addr);

		if (first_elt_addr == 0) {
			return end();
		}

		//std::printf("first_elt_addr: %llx\n", first_elt_addr);
		return iterator(alloc.get(first_elt_addr));
	}

	iterator before_begin() const {
	//	std::printf("before_list_elt_addr: %llx\n", before_list_elt_addr);
		return iterator(alloc.get(before_list_elt_addr));
	}

	void clear() {
		auto back_it = before_begin();
		auto it = begin();
		while (it != end()) {
			it = back_it.erase_after();
		}
	}

	//void print_list() const {
	//	std::printf("printing list, start addr = %llx\n", before_list_elt_addr);
	//	for (auto p : *this) {
	//	}
	//}
};

template<typename forward_list_t>
class buffered_forward_list_iter {
	forward_list_t& list;
	forward_list_t::iterator iter, back_iter;
public:

	buffered_forward_list_iter(forward_list_t& list)
		: list(list)
		, iter(list.begin())
		, back_iter(list.before_begin()) {}

	uint16_t operator*() {
		//std::printf("operator*\n");
		//back_iter.print_iter();
		//iter.print_iter();
		return *iter;
	}

	buffered_forward_list_iter& operator++(int) {
		back_iter = iter;
		iter++;
		//std::printf("post ++\n");
		//back_iter.print_iter();
		//iter.print_iter();
		//iter++;
		//back_iter++;
		return *this;
	}

	void insert(uint16_t const& val) {
		iter = back_iter.insert_after(val);//list.insert_after(back_iter, val);

		//std::printf("post insert\n");
		//back_iter.print_iter();
		//iter.print_iter();
	}

	void erase() {
		//back_iter.print("back_iter before erase");
		//iter.print("iter before erase");
		iter = back_iter.erase_after();//list.erase_after(back_iter);
		//std::printf("post erase\n");
		//back_iter.print_iter();
		//iter.print_iter();
		//back_iter.print("back_iter after erase");
		//iter.print("iter after erase");

	}

	bool at_end() {
		return iter == list.end();
	}
};

} /* speedex */
