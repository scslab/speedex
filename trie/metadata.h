#pragma once

#include <atomic>
#include <cstdint>
#include <sstream>

/*! \file metadata.h

Mixable metadata classes for use within merkle tries.
Metadata must be elements in a commutative group.

All metadata objects should implement the methods used in EmptyMetadata.

unsafe_load/store is not threadsafe with other modification.

The Atomic* metadata versions are what are stored within tries.
The non-atomic versions are passed around (as i.e. metadata deltas).

*/

namespace speedex {

namespace {
constexpr static auto load_order = std::memory_order_relaxed;//acquire;
constexpr static auto store_order = std::memory_order_relaxed;//release;
constexpr static auto load_store_order = std::memory_order_relaxed;//acq_rel;
}

//! Empty metadata no-op class
struct EmptyMetadata {

	using BaseT = EmptyMetadata;
	using AtomicT = EmptyMetadata;

	template<typename ValueType>
	EmptyMetadata(ValueType ptr) {}

	EmptyMetadata () {}

	EmptyMetadata& operator+=(const EmptyMetadata& other) { return *this; }

	EmptyMetadata& operator-=(const EmptyMetadata& other) { return *this; }

	EmptyMetadata operator-() { return EmptyMetadata(); }

	bool operator==(const EmptyMetadata& other) { return true; };

	EmptyMetadata unsafe_substitute(EmptyMetadata other) { 
		return EmptyMetadata(); 
	}
	void clear() {}

	EmptyMetadata unsafe_load() const { return EmptyMetadata{}; }
	void unsafe_store(const EmptyMetadata& other) {}

	std::string to_string() const {
		return std::string();
	}
};

struct AtomicDeletableMixin;

struct DeletableMixin {

	using AtomicT = AtomicDeletableMixin;
	using value_t = int32_t;
	value_t num_deleted_subnodes;

	template<typename ValueType>
	DeletableMixin(const ValueType& ptr) 
		: num_deleted_subnodes(0) {}
	DeletableMixin() : num_deleted_subnodes(0) {}

	DeletableMixin& operator+=(const DeletableMixin& other) {
		num_deleted_subnodes += other.num_deleted_subnodes;
		return *this;
	}

	DeletableMixin& operator-=(const DeletableMixin& other) {
		num_deleted_subnodes -= other.num_deleted_subnodes;
		return *this;
	}

	bool operator==(const DeletableMixin& other) {
		return num_deleted_subnodes == other.num_deleted_subnodes;
	}

	std::string to_string() const {
		std::stringstream s;
		s << "num_deleted_subnodes:"<<num_deleted_subnodes<<" ";
		return s.str();
	}

	template<typename AtomicType>
	void unsafe_load_from(const AtomicType& s) {
		num_deleted_subnodes = s.num_deleted_subnodes.load(load_order);
	}
};

struct AtomicDeletableMixin {

	using BaseT = DeletableMixin;

	std::atomic<BaseT::value_t> num_deleted_subnodes;

	template<typename ValueType>
	AtomicDeletableMixin(const ValueType& val) : num_deleted_subnodes(0) {}

	AtomicDeletableMixin() : num_deleted_subnodes(0) {}

	void operator+= (const DeletableMixin& other) {
		num_deleted_subnodes.fetch_add(other.num_deleted_subnodes, store_order);
	}

	void operator-= (const DeletableMixin& other) {
		num_deleted_subnodes.fetch_sub(other.num_deleted_subnodes, store_order);
	}

	bool operator== (const DeletableMixin& other) {
		return num_deleted_subnodes.load(load_order) 
			== other.num_deleted_subnodes;
	}

	bool compare_exchange(BaseT::value_t expect, BaseT::value_t desired) {
		return num_deleted_subnodes.compare_exchange_strong(
			expect, desired, load_store_order);
	}

	static bool compare_exchange(
		AtomicDeletableMixin& object, 
		BaseT::value_t& expect, 
		BaseT::value_t& desired) {
		return object.compare_exchange(expect, desired);
	}

	void clear() {
		num_deleted_subnodes = 0;
	}

	void unsafe_store(const DeletableMixin& other) {
		num_deleted_subnodes.store(other.num_deleted_subnodes, store_order);
	}

	std::string to_string() const {
		std::stringstream s;
		s << "num_deleted_subnodes:"<<num_deleted_subnodes<<" ";
		return s.str();
	}
};

struct AtomicSizeMixin;

//! Non-threadsafe size metadata.
struct SizeMixin {

	using AtomicT = AtomicSizeMixin;
	int64_t size;

	template<typename ValueType>
	SizeMixin(ValueType v) : size(1) {}

	SizeMixin() : size(0) {}

	SizeMixin& operator+=(const SizeMixin& other) {
		size += other.size;
		return *this;
	}
	SizeMixin& operator-=(const SizeMixin& other) {
		size -= other.size;
		return *this;
	}

	bool operator==(const SizeMixin& other) {
		return size == other.size;
	}

	std::string to_string() const {
		std::stringstream s;
		s << "size:"<<size<<" ";
		return s.str();
	}

	template<typename OtherSizeType>
	void unsafe_load_from(const OtherSizeType& s) {
		size = s.size.load(load_order);
	}
};


//! Threadsafe metadata storing the number of leaves below this node.
struct AtomicSizeMixin {
	std::atomic_int64_t size;

	template<typename ValueType>
	AtomicSizeMixin(const ValueType& v) : size(1) {}

	AtomicSizeMixin() : size(0) {}

	void operator+= (const SizeMixin& other) {
		size.fetch_add(other.size, store_order);
	}

	void operator-= (const SizeMixin& other) {
		size.fetch_sub(other.size, store_order);
	}

	bool operator== (const SizeMixin& other) {
		return size.load(load_order) == other.size;
	}
	void clear() {
		size = 0;
	}

	void unsafe_store(const SizeMixin& other) {
		size.store(other.size, store_order);
	}

	std::string to_string() const {
		std::stringstream s;
		s << "size:"<<size<<" ";
		return s.str();
	}

};

template<typename ...MetadataComponents>
struct AtomicCombinedMetadata;

//! Merge multiple metadata mixins into one object.
template <typename ...MetadataComponents>
struct CombinedMetadata : public MetadataComponents... {
	using AtomicT = AtomicCombinedMetadata<MetadataComponents...>;

	template<typename ValueType>
	CombinedMetadata(ValueType v) : MetadataComponents(v)... {}

	CombinedMetadata() : MetadataComponents()... {}

	using MetadataComponents::operator+=...;
	using MetadataComponents::operator-=...;
	using MetadataComponents::operator==...;
	using MetadataComponents::unsafe_load_from...;

	CombinedMetadata& operator+=(const CombinedMetadata& other) {
		(MetadataComponents::operator+=(other), ...);
		return *this;
	}
	CombinedMetadata& operator-=(const CombinedMetadata& other) {
		(MetadataComponents::operator-=(other),...);
		return *this;
	}
	CombinedMetadata operator-() {
		CombinedMetadata out = CombinedMetadata();
		out -=(*this);
		return out;
	}

	bool operator==(const CombinedMetadata& other) {
		bool result = true;
		((result = result && MetadataComponents::operator==(other)),...);
		return result;
	}

	bool operator!=(const CombinedMetadata& other) {
		return !(*this == other);
	}

	void clear() {
		*this = CombinedMetadata();
	}
	CombinedMetadata clone() const {
		CombinedMetadata output = *this;
		return output;
	}

	CombinedMetadata substitute(const CombinedMetadata& other) {
		CombinedMetadata output = *this;
		*this = other;
		return output;
	}

	std::string to_string() const {
		std::string output("");
		((output += MetadataComponents::to_string()),...);
		return output;
	}

	template<typename AtomicType>
	void unsafe_load_from(const AtomicType& other) {
		(MetadataComponents::unsafe_load_from(other),...);
	}

	void unsafe_store(const CombinedMetadata& other) {
		(MetadataComponents::unsafe_store(other),...);
	}
};

//! Threadsafe mixed metadata wrapper
template<typename ...MetadataComponents>
struct AtomicCombinedMetadata : public MetadataComponents::AtomicT... {

	using BaseT = CombinedMetadata<MetadataComponents...>;

	using MetadataComponents::AtomicT::operator+=...;
	using MetadataComponents::AtomicT::operator-=...;
	using MetadataComponents::AtomicT::operator==...;
	using MetadataComponents::AtomicT::clear...;
	using MetadataComponents::AtomicT::unsafe_store...;

	template<typename ValueType>
	AtomicCombinedMetadata(ValueType value)
		: MetadataComponents::AtomicT(value)... {}

	AtomicCombinedMetadata() : MetadataComponents::AtomicT()...{}

	void operator+=(const BaseT& other) {
		(MetadataComponents::AtomicT::operator+=(other),...);
	}
	void operator-=(const BaseT& other) {
		(MetadataComponents::AtomicT::operator-=(other),...);
	}
	bool operator==(const BaseT& other) {
		bool comparison = true;
		((comparison &= MetadataComponents::AtomicT::operator+=(other)),...);
		return comparison;
	}
	void clear() {
		(MetadataComponents::AtomicT::clear(),...);
	}

	//only safe to load when you have an exclusive lock on node.
	//Otherwise, might get shorn reads
	BaseT unsafe_load() const{
		BaseT output;
		output.unsafe_load_from(*this);
		return output;
	}

	void unsafe_store(const BaseT& other) {
		(MetadataComponents::AtomicT::unsafe_store(other),...);
	}

	BaseT unsafe_substitute(const BaseT& new_metadata) {
		auto output = unsafe_load();
		unsafe_store(new_metadata);
		return output;
	}

	std::string to_string() const {
		std::string output("");
		((output += MetadataComponents::AtomicT::to_string()),...);
		return output;
	}

};

} /* speedex */