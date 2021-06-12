#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace speedex {

//! Stores a unique thread_local identifier (uint32_t).
//! Call get() to access id.
class ThreadlocalIdentifier {
	inline static std::atomic<uint32_t> _tl_initializer = 0;
	inline static thread_local uint32_t tid 
		= _tl_initializer.fetch_add(1, std::memory_order_relaxed); 
public:
	static uint32_t get() {
		return tid;
	}
};


/*! Threadlocal object cache.

Using ThreadlocalIdentifier is safe in any config, since each thread's id
is unique.  Downside is that more threads running could consume identifiers,
and so at some point, we'll need to boost CACHE_SIZE.

An alternative is TBB's tbb::this_task_arena::current_thread_index, 
(available in <tbb/task_arena.h>)
which keeps ids in a fixed (small) range.

The downside is that migrating this cache across arenas would break
any exported references or cause concurrent access to non-threadsafe objects.
This would be dangerous (easy to forget this restriction). YMMV.
*/

template<typename ValueType, int CACHE_SIZE = 128>
class ThreadlocalCache {

	std::array<std::optional<ValueType>, CACHE_SIZE> objects;

public:

	ThreadlocalCache() : objects() {}

	//! Get current thread's cached object.
	//! Input constructor arguments for cached object, in the case
	//! that this is the first access (and so thread's cache is empty).
	template<typename... ctor_args>
	ValueType& get(ctor_args&... args) {
		auto idx = ThreadlocalIdentifier::get();
		if (idx >= CACHE_SIZE) {
			throw std::runtime_error("invalid tlcache access!");
		}
		if (!objects[idx]) {
			objects[idx].emplace(args...);
		}
		return *objects[idx];
	}

	template<typename... ctor_args>
	ValueType& get_index(int idx, ctor_args&... args) {
		if (idx >= CACHE_SIZE || idx < 0) {
			throw std::runtime_error("invalid tlcache access!");
		}
		if (!objects[idx]) {
			objects[idx].emplace(args...);
		}
		return *objects[idx];
	}

	//! At times it is useful to export the list of cached objects
	//! all at once.
	std::array<std::optional<ValueType>, CACHE_SIZE>& get_objects() {
		return objects;
	}

	void clear() {
		for (int i = 0; i < CACHE_SIZE; i++) {
			objects[i] = std::nullopt;
		}
	}

};

}
