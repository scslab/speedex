#pragma once

#include <shared_mutex>
#include <variant>

#include <xdrpp/marshal.h>

/*! \file utils.h 

Miscellaneous classes used in trie management.

*/

namespace speedex {

struct EmptyValue {

	constexpr static uint16_t data_len() {
		return 0;
	}

	constexpr static void serialize() {}

	constexpr static void copy_data(std::vector<uint8_t>& buf) {}

	constexpr static bool modified_since_last_hash() {
		return false;
	}
	//void append_to_proof(std::vector<unsigned char>& buf) const {}
};



//F is function to map prefix to KeyInterpretationType
template<typename MetadataOutputType, typename KeyInterpretationType, typename KeyMakerF>
struct IndexedMetadata {
	KeyInterpretationType key;
	MetadataOutputType metadata;

	IndexedMetadata(KeyInterpretationType key, MetadataOutputType metadata) :
			key(key),
			metadata (metadata) {}
};

struct GenericInsertFn {
	template<typename MetadataType, typename ValueType>
	static MetadataType new_metadata(const ValueType& value) {
		return MetadataType(value);
	}

	template<typename ValueType, typename prefix_t>
	static ValueType new_value(const prefix_t& prefix) {
		return ValueType{};
	}
};

//can call unsafe methods bc exclusive locks on metadata inputs in caller
struct OverwriteMergeFn {
	template<typename ValueType>
	static void value_merge(ValueType& main_value, const ValueType& other_value) {
		main_value = other_value;
	}

	template<typename AtomicMetadataType>
	static typename AtomicMetadataType::BaseT 
	metadata_merge(AtomicMetadataType& main_metadata, const AtomicMetadataType& other_metadata) {

		//return other - main, set main <- delta
		auto other_metadata_loaded = other_metadata.unsafe_load();
		auto original_main_metadata = main_metadata.unsafe_load();
		main_metadata.unsafe_store(other_metadata_loaded);

		other_metadata_loaded -= original_main_metadata;
		return other_metadata_loaded;
	}
};

//can call unsafe methods bc excl locks on metadata inputs in caller
struct OverwriteInsertFn : public GenericInsertFn {

	template<typename ValueType>
	static void value_insert(ValueType& main_value, const ValueType& other_value) {
		main_value = other_value;
	}

	template<typename AtomicMetadataType, typename ValueType>
	static typename AtomicMetadataType::BaseT 
	metadata_insert(AtomicMetadataType& original_metadata, const ValueType& new_value) {

		//return other - main, set main <- delta
		auto new_metadata = typename AtomicMetadataType::BaseT(new_value);
		auto metadata_delta = new_metadata;
		metadata_delta -= original_metadata.unsafe_substitute(new_metadata);
		return metadata_delta;
	}
};

struct RollbackInsertFn : public OverwriteInsertFn {
	template<typename MetadataType, typename ValueType>
	static MetadataType new_metadata(const ValueType& value) {
		auto out = MetadataType(value);
		out.num_rollback_subnodes = 1;
		return out;
	}


	template<typename AtomicMetadataType, typename ValueType>
	static typename AtomicMetadataType::BaseT 
	metadata_insert(AtomicMetadataType& original_metadata, const ValueType& new_value) {

		//return other - main, set main <- delta
		auto new_metadata = typename AtomicMetadataType::BaseT(new_value);
		new_metadata.num_rollback_subnodes = 1;
		
		auto metadata_delta = new_metadata;
		metadata_delta -= original_metadata.unsafe_substitute(new_metadata);
		return metadata_delta;
	}
};

struct NullOpDelSideEffectFn {
	template<typename ...Args>
	void operator() (const Args&... args) {}
};


template<typename xdr_type>
struct XdrTypeWrapper : public xdr_type {

	XdrTypeWrapper() 
		: xdr_type()
		{}
	XdrTypeWrapper(const xdr_type& x) 
		: xdr_type(x)
		//, serialization()
       		{}

	XdrTypeWrapper& operator=(const XdrTypeWrapper& other) {
		xdr_type::operator=(other);
		return *this;
	}

	XdrTypeWrapper(const XdrTypeWrapper& other)
	: xdr_type()
	{
		xdr_type::operator=(other);
	}

	size_t data_len() const 
	{
		return xdr::xdr_size(static_cast<xdr_type>(*this));
	}

	void copy_data(std::vector<uint8_t>& buf) const {
		auto serialization = xdr::xdr_to_opaque(static_cast<xdr_type>(*this));
		buf.insert(buf.end(), serialization.begin(), serialization.end());
	}
};

/*! Template class that optionally wraps a mutex.
When SERIAL_MODE = true, class contains a rwlock.  
Methods that need shared locks
can be used (i.e. parallel_insert)
When SERIAL_MODE = false, class is empty.
*/
template<
	bool SERIAL_MODE>
class OptionalLock {
};

template<>
class OptionalLock<true> {

	mutable std::shared_mutex mtx;


public:
	OptionalLock() 
		: mtx() {}

	template<typename lock_type>
	lock_type lock() const {
		mtx.lock();
		return {mtx, std::adopt_lock};
	}

};

template<>
class OptionalLock<false> {

public:
	OptionalLock() {}

	template<typename lock_type>
	std::monostate lock() const {
		return std::monostate{};
	}

};

} /* speedex */