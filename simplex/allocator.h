#pragma once

#include <memory>

namespace speedex {

class AllocatorRow {
	//low 16 bits are value, upper 48 for ptr (32 for buffer, 16 for idx)

	constexpr static uint16_t ROW_SIZE = UINT16_MAX;
	constexpr static uint64_t ROW_SIZE_SHIFTED = static_cast<uint64_t>(ROW_SIZE) << 16;
	mutable std::array<uint64_t, ROW_SIZE> data;
	uint64_t next_free_slot;
	uint16_t usage_count;

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
		next_free_slot+=0x1'0000;
		usage_count++;
		return out;
	}

	bool full() {
		return next_free_slot == ROW_SIZE_SHIFTED;
	}
	void free() {
		usage_count--;
	}
	bool empty() const {
		return usage_count == 0;
	}

	uint64_t* get(uint16_t idx) const {
		return &(data[idx]);
	}

	void clear() {
		usage_count = 0;
		next_free_slot = 0;
	}
};

class Allocator {
	std::vector<std::unique_ptr<AllocatorRow>> buffers;

	std::vector<uint32_t> nonfull_buffers;

public:

	Allocator()
		: buffers()
		, nonfull_buffers()
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
		uint64_t out = buffers[nonfull_buffers.back()]->allocate();

		if (buffers[nonfull_buffers.back()]->full()) {
			nonfull_buffers.pop_back();

			if (nonfull_buffers.empty()) {
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
		if (buffers[buf_idx]->empty()) {
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
	}
};

class buffered_forward_list {

	Allocator& alloc;

	uint64_t before_list_elt_addr;

public:

	buffered_forward_list(Allocator& alloc)
		: alloc(alloc)
		, before_list_elt_addr(alloc.allocate())
		{
			*(alloc.get(before_list_elt_addr)) = 0;
		}

	~buffered_forward_list() {
		alloc.free(before_list_elt_addr);
	}

	class iterator {
		Allocator* alloc;

		uint64_t* cur_object;

	public:

		constexpr iterator(Allocator* alloc, uint64_t* cur_object)
			: alloc(alloc)
			, cur_object(cur_object)
			{}

		iterator& operator++() {
		//	std::printf("deref cur_obj: %llx\n", *cur_object);
			cur_object = alloc->get(*cur_object);
		//	if (cur_object != nullptr) {
		//		std::printf("post advance: %llx\n", *cur_object);
		//	}
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

		//insert right after *cur_obj
		iterator insert_after(uint16_t value) {

		//	std::printf("insert after original obj: %p %llx\n", cur_object, *cur_object);

			uint64_t new_obj_addr = alloc->allocate();
		//	std::printf("new_obj_addr %llx\n", new_obj_addr);
			uint64_t* new_obj = (alloc->get(new_obj_addr));

			if (new_obj == nullptr) {
				throw std::runtime_error("wtf");
			}

		//	std::printf("new_obj: %p, %llx\n", &new_obj, new_obj);

			Allocator::copy_next_obj_ptr(*new_obj, *cur_object);
			Allocator::set_value(*new_obj, value);

			Allocator::copy_next_obj_ptr(*cur_object, new_obj_addr);

		//	std::printf("post cur_obj %llx\n", *cur_object);
		//	std::printf("new obj: %llx\n", new_obj);

			return iterator(alloc, new_obj);
		}

		iterator erase_after() {
			if (cur_object == nullptr) {
				throw std::runtime_error("can't erase after list end");
			}
			//assumption is that next obj actually exists
			uint64_t& next_obj = *(alloc->get(*cur_object));

			//frees the location to which cur_obj points
			alloc->free(*cur_object);

			Allocator::copy_next_obj_ptr(*cur_object, next_obj);

			return iterator(alloc, alloc->get(*cur_object));
		}
	};

	constexpr static iterator end() {
		return iterator(nullptr, nullptr);
	}

	iterator begin() const {
		//safe to deref first bc guaranteed to exist by ctor
		uint64_t first_elt_addr = *alloc.get(before_list_elt_addr);

		if (first_elt_addr == 0) {
			return end();
		}

		//std::printf("first_elt_addr: %llx\n", first_elt_addr);
		return iterator(&alloc, alloc.get(first_elt_addr));
	}

	iterator before_begin() const {
	//	std::printf("before_list_elt_addr: %llx\n", before_list_elt_addr);
		return iterator(&alloc, alloc.get(before_list_elt_addr));
	}

	void clear() {
		auto back_it = before_begin();
		auto it = begin();
		while (it != end()) {
			it = back_it.erase_after();
		}
	}

	void print_list() const {
		std::printf("printing list, start addr = %llx\n", before_list_elt_addr);
		for (auto p : *this) {
		}
	}
};

class buffered_forward_list_iter {
	buffered_forward_list& list;
	buffered_forward_list::iterator iter, back_iter;
public:

	buffered_forward_list_iter(buffered_forward_list& list)
		: list(list)
		, iter(list.begin())
		, back_iter(list.before_begin()) {}

	uint16_t operator*() {
		return *iter;
	}

	buffered_forward_list_iter& operator++(int) {
		iter++;
		back_iter++;
		return *this;
	}

	void insert(uint16_t const& val) {
		iter = back_iter.insert_after(val);//list.insert_after(back_iter, val);
	}

	void erase() {
		//back_iter.print("back_iter before erase");
		//iter.print("iter before erase");
		iter = back_iter.erase_after();//list.erase_after(back_iter);
		//back_iter.print("back_iter after erase");
		//iter.print("iter after erase");

	}

	bool at_end() {
		return iter == list.end();
	}
};

} /* speedex */
