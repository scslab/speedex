#pragma once
/*! \file merkle_trie.h

Implementation of a Merkle Trie, a key value store.

Keys are all fixed length.  

Nodes can store metadata, such as number of leaves below said node
or number of nodes marked as deleted.  These metadata can be elements
of arbitrary commutative groups.

In this implementation, children pointers are standard 8-byte pointers (i.e.
virtual addresses).
*/


#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <cstring>
#include <execution>
#include <cstdint>
#include <type_traits>
#include <cstdio>
#include <iostream>
#include <atomic>
#include <tuple>
#include <thread>
#include <unordered_set>

#include "trie/children_map.h"
#include "trie/metadata.h"
#include "trie/prefix.h"
#include "trie/ranges.h"
#include "trie/utils.h"

#include "utils/debug_utils.h"
#include "utils/debug_macros.h"

#include "xdr/trie_proof.h"
#include "xdr/types.h"

#include <sodium.h>


namespace speedex {

/*! Node within a merkle trie. */
template<
	TriePrefix prefix_type,
	typename ValueType = EmptyValue, 
	typename MetadataType = EmptyMetadata,
	bool USE_LOCKS = true>
class TrieNode {
public:
	constexpr static uint8_t BRANCH_BITS = 4;
	constexpr static bool HAS_VALUE 
		= !std::is_same<EmptyValue, ValueType>::value;
	constexpr static bool HAS_METADATA 
		= !std::is_same<EmptyMetadata, MetadataType>::value;
	constexpr static bool METADATA_DELETABLE 
		= std::is_base_of<DeletableMixin, MetadataType>::value;
	constexpr static bool METADATA_ROLLBACK
		= std::is_base_of<RollbackMixin, MetadataType>::value;
	constexpr static bool HAS_SIZE 
		= std::is_base_of<SizeMixin, MetadataType>::value;

	constexpr static uint16_t KEY_LEN_BYTES = prefix_type::size_bytes();
	constexpr static unsigned int BRANCH_BITS_EXPORT = BRANCH_BITS;

	constexpr static bool LOCKABLE = USE_LOCKS;

	/*Metadata locking is slightly odd.
	  Adding/subtracting MetadataTypes to AtomicMetadataTypes can be 
	  interleaved.
	  This is because metadata is constructed to be commutative.
	  Thus, we allow metadata modification if you own a shared_lock on a node.
	  However, that means that reads not exclusively 
	  locked might be sheared (shorn?)
	*/
	using AtomicMetadataType = typename std::conditional<
		HAS_METADATA, 
			typename MetadataType::AtomicT,
			EmptyMetadata>::type;

	using trie_ptr_t = std::unique_ptr<TrieNode>;

	using exclusive_lock_t = std::lock_guard<std::shared_mutex>;
	using shared_lock_t = std::shared_lock<std::shared_mutex>;

	using prefix_t = prefix_type;

	using hash_t = Hash;
	using children_map_t = FixedChildrenMap<trie_ptr_t, ValueType>;
	using bv_t = typename children_map_t::bv_t;

	static_assert(
		!std::is_void<ValueType>::value, "can't have void valuetype");
	static_assert(
		!std::is_void<MetadataType>::value, "can't have void metadata");

private:
	static_assert(BRANCH_BITS == 2 || BRANCH_BITS == 4 || BRANCH_BITS == 8, 
		"invalid number of branch bits");
	static_assert(BRANCH_BITS == 4, "everything else is unimplemented");

	constexpr static uint8_t _MAX_CHAR_VALUE = 0xFFu;
	constexpr static unsigned char SHIFT_LEN = 8-BRANCH_BITS;
	constexpr static unsigned char BRANCH_MASK 
		= ((_MAX_CHAR_VALUE)<<SHIFT_LEN) & _MAX_CHAR_VALUE;
	constexpr static unsigned char MAX_BRANCH_VALUE 
		= ((_MAX_CHAR_VALUE)>>(8-BRANCH_BITS)); // 0 to this value, inclusive
public:
	//! Max key len in bits (KEY_LEN unit is BYTES)
	constexpr static PrefixLenBits MAX_KEY_LEN_BITS 
		= PrefixLenBits{8 * KEY_LEN_BYTES}; 
private:


	children_map_t children;

	// only 0 beyond the prefix
	prefix_t prefix;
	PrefixLenBits prefix_len;

	std::atomic_bool hash_valid = false;

	AtomicMetadataType metadata;
	
	OptionalLock<USE_LOCKS> locks;

	hash_t hash;

	//! Adds an input metadata object to the node's metadata.
	void update_metadata(
		const MetadataType& metadata_delta) {
		if (HAS_METADATA) {
			metadata += metadata_delta;
		}
	}

	//! Computes a nodes metadata by summing the metadata of the children.
	void compute_metadata_unsafe() {
		metadata.clear();
		if (prefix_len == MAX_KEY_LEN_BITS) {
			metadata += MetadataType{children.value()};
			return;
		}

		for (auto iter = children.begin(); iter != children.end(); iter++) {
			update_metadata(
				(*iter).second->metadata.unsafe_load());
		}
	}


	//! Get the branch bits of the input prefix.
	//! The branch bits are the BRANCH_BITS many bits
	//! of the input prefix that immediately follow
	//! the first prefix_len bits.
	unsigned char get_branch_bits(const prefix_t& data) const;

	//! Compute the length (in bits) of the longest matching prefix
	//! of this node's prefix with the input prefix.
	PrefixLenBits get_prefix_match_len(
		const prefix_t& other, 
		const PrefixLenBits other_len = MAX_KEY_LEN_BITS) const;

	// insert preserves metadata
	template<typename InsertFn, typename InsertedValueType>
	const MetadataType _insert(
		const prefix_t& key, const InsertedValueType& leaf_value);

	Hash& get_hash_ptr() {
		return hash;
	}

	//! Invalidate the cached hash.  Hash will be recomputed
	//! on next call to hash()
	void invalidate_hash() {
		hash_valid.store(false, std::memory_order_release);
	}

	//! Mark the cached hash as valid.  Called during
	//! calls to hash()
	void validate_hash() {
		hash_valid.store(true, std::memory_order_release);
	}

	//! Check whether the cached hash value is valid.
	bool get_hash_valid() const {
		return hash_valid.load(std::memory_order_acquire);
	}

	template<typename MergeFn>
	const MetadataType _merge_in(trie_ptr_t&& other);

	friend struct BatchMergeRange<TrieNode, MetadataType>;

	OptionalLock<USE_LOCKS>& get_lock_ref() {
		return locks;
	}

public:

	//! Return new node with current node's contents.
	//! Current node's children object is invalidated, but prefix and prefix_len
	//! are unchanged.
	trie_ptr_t move_contents_to_new_node() {
		return std::make_unique<TrieNode>(
			std::move(children), prefix, prefix_len);
	}

	//! Create new node using a new set of children but current prefix/len
	trie_ptr_t duplicate_node_with_new_children(children_map_t&& new_children) {
		return std::make_unique<TrieNode>(
			std::move(new_children), prefix, prefix_len);
	}

	//! Constructor for splitting a prefix into branches
	//! We transfer all of the root nodes stuff to the child
	//! Caller should set metadata of this node.
	TrieNode(
		children_map_t&& new_children,
		const prefix_t old_prefix,
		const PrefixLenBits prefix_len)
	 	: children(std::move(new_children))
	 	, prefix(old_prefix)
		, prefix_len(prefix_len)
		, metadata() // Take care of setting metadata in caller
	{}


	//! Construct a new value leaf with a given input value.
	template<typename InsertFn, typename InsertedValueType>
	static trie_ptr_t 
	make_value_leaf(
		const prefix_t key, 
		typename std::enable_if<
			std::is_same<ValueType, InsertedValueType>::value, 
			InsertedValueType>::type leaf_value) {
		return std::make_unique<TrieNode>(
			key, 
			leaf_value, 
			InsertFn::template new_metadata<MetadataType>(leaf_value));
	}

	//! Value leaves for instances where the input value is not the value stored
	//! at leaves.  Creates an instance of the leaf value, uses the
	//! InsertFn callback to insert the input value to the leaf value,
	//! then creates the trie node.
	template<typename InsertFn, typename InsertedValueType>
	static trie_ptr_t 
	make_value_leaf(
		const prefix_t key, 
		typename std::enable_if<
			!std::is_same<ValueType, InsertedValueType>::value,
			InsertedValueType>::type leaf_value) {

		ValueType value_out = InsertFn::new_value(key);
		InsertFn::value_insert(value_out, leaf_value);
		return std::make_unique<TrieNode>(
			key, 
			value_out, 
			InsertFn::template new_metadata<MetadataType>(leaf_value));
	}

	//! Constructor for value leaves.
	TrieNode(
		const prefix_t key, 
		ValueType leaf_value, 
		const MetadataType& base_metadata)
	: children(leaf_value)
	, prefix(key)
	, prefix_len(MAX_KEY_LEN_BITS)
	, metadata()
	{
		metadata.unsafe_store(base_metadata);
	}


	void prefetch_full_write() const {
		for (size_t i = 0; i < sizeof(TrieNode); i+=64) {
			__builtin_prefetch(static_cast<const void*>(this) + i, 1, 2);
		}
	}	

	void print_offsets() {
		LOG("children   %lu %lu", 
			offsetof(TrieNode, children), sizeof(children));
		LOG("locks      %lu %lu", 
			offsetof(TrieNode, locks), sizeof(locks));
		LOG("prefix     %lu %lu", 
			offsetof(TrieNode, prefix), sizeof(prefix));
		LOG("prefix_len %lu %lu", 
			offsetof(TrieNode, prefix_len), sizeof(prefix_len));	
		LOG("metadata   %lu %lu", 
			offsetof(TrieNode, metadata), sizeof(metadata));	
		LOG("hash       %lu %lu", 
			offsetof(TrieNode, hash), sizeof(hash));	
		LOG("hash_valid %lu %lu", 
			offsetof(TrieNode, hash_valid), sizeof(hash_valid));
	}

	//! Basic iterator implementation.  Not quite standard - check
	//! termination by calling at_end();
	struct iterator {

		using kv_t = std::pair<
			const prefix_t, std::reference_wrapper<const ValueType>>;

		const TrieNode& main_node;

		iterator(const TrieNode& main_node)
			: main_node(main_node)
			, local_iter(main_node.children.begin()) {
			if (main_node.prefix_len == MAX_KEY_LEN_BITS
				 || (main_node.prefix_len.len == 0 
				 		&& main_node.children.size() == 0)) {
				child_iter = nullptr;
			} else {
				child_iter = std::make_unique<iterator>(*((*local_iter).second));
			}
		}

		typename children_map_t::iterator local_iter;

		std::unique_ptr<iterator> child_iter;

		const kv_t operator*() {
			if (main_node . prefix_len == MAX_KEY_LEN_BITS) {
				return std::make_pair(
					main_node.prefix, std::cref(main_node.children.value()));
			}

			if (local_iter == main_node.children.end()) {
				throw std::runtime_error("deref iter end");
			}
			return **child_iter;
		}

		bool operator++() {
			if (local_iter == main_node.children.end()) {
				return true;
			} //if check passes, then child_ptr is not null

			if (!child_iter) {
				throw std::runtime_error("how on earth is child_iter null");
			}

			auto inc_local = ++(*child_iter);
			
			if (inc_local) {
				local_iter++;
				if (local_iter != main_node.children.end()) {
					child_iter 
						= std::make_unique<iterator>(*((*local_iter).second));
				} else {
					child_iter = nullptr;
				}
			}
			
			if (local_iter == main_node.children.end()) {
				return true;
			}

			return false;

		}

		bool at_end() {
			return local_iter == main_node.children.end();
		}
	};

	const PrefixLenBits get_prefix_len() const {
		return prefix_len;
	}

	const prefix_t get_prefix() const {
		return prefix;
	}

	//! Return true iff node is a leaf node.
	bool is_leaf() {
		return prefix_len == MAX_KEY_LEN_BITS;
	}

	//! Returns value at node.  Throws error if not a leaf.
	ValueType& get_value() {
		if (!is_leaf()) {
			throw std::runtime_error("can't get value from non leaf");
		}
		return children.value();
	}

	//! Construct an empty trie node (0 prefix, 0 prefix_len).
	//! A node should only be empty if it is the root of an
	//! empty trie.
	static trie_ptr_t make_empty_node() {
		return std::make_unique<TrieNode>();
	}

	/*! Constuctor for creating empty trie */
	TrieNode()
	  : children()
	  , prefix()
	  , prefix_len{0}
	  ,	metadata() {}

	//! Insert key, overwrites previous key, if it exists
	//! (InsertFn specifies an "overwrite" fn, which can do something else)
	template<bool x = HAS_VALUE, typename InsertFn, typename InsertedValueType>
	void insert(
		typename std::enable_if<x, const prefix_t&>::type key, 
		const InsertedValueType& leaf_value);

	//! Insert key, overwriting previous key (if it exists).
	//! "Overwrite" means InsertFn's overwrite callback
	template<bool x = HAS_VALUE, typename InsertFn>
	void insert(
		typename std::enable_if<!x, const prefix_t&>::type key);

	//! Computes hash for this node.
	//! Implicitly computes hashes for any children with invalied cached
	//! hashes.  Operates serially.
	template<typename... ApplyToValueBeforeHashFn>
	void compute_hash();

	//! Get the value associated with a given key.
	template<bool x = HAS_VALUE>
	std::optional<ValueType> 
	get_value(typename std::enable_if<x, const prefix_t&>::type query_key);

	void append_hash_to_vec(std::vector<unsigned char>& buf) {
		[[maybe_unused]]
		auto lock = locks.template lock<shared_lock_t>();
		buf.insert(buf.end(), hash.begin(), hash.end());
	}

	void copy_hash_to_buf(Hash& buffer) {
		[[maybe_unused]]
		auto lock = locks.template lock<shared_lock_t>();
		buffer = hash;
	}

	void copy_hash_to_buf(unsigned char* buffer) {
	
		[[maybe_unused]]
		auto lock = locks.template lock<shared_lock_t>();
		static_assert(sizeof(hash) == 32, "hash size nonsense");
		memcpy(buffer, hash.data(), 32);
	}

	template<bool x = HAS_SIZE>
	typename std::enable_if<x, size_t>::type size() const {
		static_assert(x == HAS_SIZE, "no funny business");

		int64_t sz = metadata.size.load(std::memory_order_acquire);
		TRIE_INFO("metadata.size:%d", sz);
		if (sz < 0) {
			throw std::runtime_error("invalid size!");
		}
		return sz;
	}

	template<bool x = HAS_SIZE>
	typename std::enable_if<!x, size_t>::type size() const {
		static_assert(x == HAS_SIZE, "no funny business");
		return uncached_size();
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, size_t>::type 
	num_deleted_subnodes() const {
		static_assert(x == METADATA_DELETABLE, "no funny business");
		int64_t out = metadata.num_deleted_subnodes.load(
			std::memory_order_acquire);
		if (out < 0) {
			throw std::runtime_error("invalid number of deleted subnodes");
		}
		return out;
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<!x, size_t>::type 
	num_deleted_subnodes() const {
		static_assert(x == METADATA_DELETABLE, "no funny business");
		return 0;
	}

	size_t uncached_size() const;

	//! Accumulate a list of all values in trie into \a values.
	//! Overwrites contents of \a values.
	template<typename VectorType>
	void accumulate_values(VectorType& values) const;

	template<typename VectorType>
	void accumulate_keys(VectorType& values);

	ProofNode create_proof_node();
	void create_proof(Proof& proof, prefix_t data);

	const MetadataType get_metadata_unsafe() {
		return metadata.unsafe_load();
	}

	void set_metadata_unsafe(
		const AtomicMetadataType& other) {
		metadata.unsafe_store(other.unsafe_load());
	}

	//! Returns a list of all children by branch_bits.
	//! Note that this is also default behavior for children_list,
	//! but wasn't guaranted until we switch children_map_t to fixed map
	//! from std::unordered_map.
	std::vector<TrieNode*> children_list_ordered() const {
		
		[[maybe_unused]]
		auto lock = locks.template lock<shared_lock_t>();

		std::vector<TrieNode*> output;
		for (unsigned int branch_bits = 0;
			branch_bits <= MAX_BRANCH_VALUE; 
			branch_bits ++)
		{
			auto iter = children.find(branch_bits);
			if (iter != children.end()) {
				output.push_back((*iter).second);
			}
		}
		return output;
	}

	std::vector<TrieNode*> children_list() const {

		[[maybe_unused]]
		auto lock = locks.template lock<shared_lock_t>();

		std::vector<TrieNode*> output;
		for (auto iter = children.begin(); iter != children.end(); iter++) {
			output.push_back((*iter).second);
		}
		return output;
	}

	//! Returns list of children paired with the branch bits of used to 
	//! get to each child.
	std::vector<std::pair<uint8_t, TrieNode*>> 
	children_list_with_branch_bits() {
		
		std::vector<std::pair<uint8_t, TrieNode*>> output;
		for (auto iter = children.begin(); iter != children.end(); iter++) {
			output.emplace_back((*iter).first, (*iter).second);
		}
		return output;
	}




	/*! Attempt to steal a subnode of the trie, if it exists.
	Input: prefix, prefix len.  We attempt to steal a subnode of this trie
	whose prefix is an extension of this input prefix.

	Output:
	bool: tell the caller to remove the entirety of this child from the node
	(i.e. a node X matches the prefix, so it returns true.  The parent of X
	removes X from its children).
	metadata: the change in metadata induced by doing the removal.  This must
	be propagated back up to the root.
	trie_ptr_t: the removed node.
	*/
	std::tuple<bool, MetadataType, trie_ptr_t>
	destructive_steal_child(
		const prefix_t stealing_prefix,
		const PrefixLenBits stealing_prefix_len);


	//! Propagate metadata from the current node to the target node.
	//! Used in conjunction (typically) with destructive_steal_child()
	void propagate_metadata(TrieNode* target, MetadataType metadata);

	//! Invalidates the hashes of nodes on the path from this to target.
	//! Used primarily with e.g. destructive_steal_child or other batch
	//! modify operations.
	void
	invalidate_hash_to_node_nolocks(TrieNode* target);



	//! unsafe, but don't be a fool this doesn't run in prod
	bool metadata_integrity_check() {
		if (prefix_len == MAX_KEY_LEN_BITS) {
			return metadata.unsafe_load().operator==(
				MetadataType(children.value()));
		}

		MetadataType sum;
		for (auto iter = children.begin(); iter != children.end(); iter++) {
			sum += (*iter).second->metadata.unsafe_load();
			if (!(*iter).second->metadata_integrity_check()) {
				return false;
			}
		}

		MetadataType local = metadata.unsafe_load();

		auto res = (sum.operator==(local));
		if (!res) {
			auto delta = metadata.unsafe_load();
			delta -= sum;
			std::printf("DISCREPANCY: %s\n", delta.to_string().c_str());
		}
		return res;
	}


	//! Merge in a trie to the current node.
	//! Destroys input (uses input as elts of trie later).
	template<typename MergeFn>
	void merge_in(trie_ptr_t&& other) {
		{
			[[maybe_unused]]
			auto lock = locks.template lock<TrieNode::exclusive_lock_t>();
			if (size() == 0) {
				throw std::runtime_error("can't merge into empty trie!");
			}
		}


		TRIE_INFO_F(other -> _log("merging in:"));

		_merge_in<MergeFn>(std::move(other));

		TRIE_INFO_F(_log("result after merge:"));
	}


	void _log(std::string padding) const;

	const MetadataType metadata_query(
		const prefix_t query_prefix,
		const PrefixLenBits query_len);

	template<
		typename MetadataOutputType, 
		typename KeyInterpretationType, 
		typename KeyMakerF>
	void metadata_traversal(
		std::vector<IndexedMetadata<
						MetadataOutputType, 
						KeyInterpretationType, 
						KeyMakerF>>& vec, 
		MetadataOutputType& acc_metadata,
		const PrefixLenBits query_len);

	//! Mark a particular node for deletion.  Returns the value that was deleted
	//! if said value existed.
	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<
		x, std::tuple<MetadataType, std::optional<ValueType>>>::type
	mark_for_deletion(const prefix_t key);

	//! Unmarks a particular node for deletion.
	//! Returns the undeleted value, if said value was present.
	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<
		x, std::tuple<MetadataType, std::optional<ValueType>>>::type
	unmark_for_deletion(const prefix_t key);
	
	//! Actually remove all the things we marked as deleted.
	template<bool x = METADATA_DELETABLE, typename DelSideEffectFn>
	typename std::enable_if<x, std::pair<bool, MetadataType>>::type
	perform_marked_deletions(DelSideEffectFn& side_effect_handler);

	//! Clear deletion markers.
	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, void>::type
	clear_marked_deletions();

	//! Perform deletion immediately.
	//! msg to parent: delete child, is anything deleted?, metadata change
	std::tuple<bool, std::optional<ValueType>, MetadataType> 
	perform_deletion(const prefix_t key);


//	template<bool x = METADATA_DELETABLE>
//	typename std::enable_if<x, MetadataType>::type
//	mark_subtree_lt_key_for_deletion(const prefix_t max_key);

//	template<bool x = METADATA_DELETABLE>
//	typename std::enable_if<x, MetadataType>::type
//	mark_subtree_for_deletion();

	bool contains_key(const prefix_t key);

	bool single_child() {
		return children.size() == 1;
	}

	//TODO use this throughout deletions
	trie_ptr_t get_single_child() {
		if (children.size() != 1) {
			throw std::runtime_error(
				"can't get single child from nonsingle children map");
		}
		auto out = children.extract((*children.begin()).first);
		children.clear();
		return out;
	}

	//! Repairs trie canonicality by removing nodes with only one child.
	//! These nodes appear when splitting a trie, only in the portion of the
	//! trie that is split off (i.e. the trie of fully executing offers).
	//! Only called when doing a rollback, and we have to merge this split
	//! off trie back in to restore state.
	void 
	clean_singlechild_nodes(const prefix_t explore_path);

	//! Split off up to and including endow_threshold units of endowment.
	trie_ptr_t 
	endow_split(int64_t endow_threshold);

	int64_t 
	endow_lt_key(const prefix_t max_key) const;

	//! Apply some function to every value in the trie.
	template<typename ApplyFn>
	void apply(ApplyFn& func);

	template<typename ApplyFn>
	void apply(ApplyFn& func) const;

	template<typename ApplyFn>
	void apply_geq_key(ApplyFn& func, const prefix_t min_apply_key);

	template<typename ApplyFn>
	void apply_lt_key(ApplyFn& func, const prefix_t min_apply_key);

	std::optional<prefix_t> get_lowest_key();

	//! Apply some function (which can modify trie values) to the value
	//! stored with the queried key.
	template<typename ModifyFn>
	void
	modify_value_nolocks(const prefix_t query_prefix, ModifyFn& fn);

	//concurrent modification will cause problems here
	TrieNode*
	get_subnode_ref_nolocks(
		const prefix_t query_prefix, const PrefixLenBits query_len);

	template<typename VectorType>
	void 
	accumulate_values_parallel_worker(VectorType& vec, size_t offset) const;

	/*template<typename VectorType>
	NoOpTask
	coroutine_accumulate_values_parallel_worker(
		VectorType& vec, CoroutineThrottler& throttler, size_t offset);

	template<typename ApplyFn>
	NoOpTask
	coroutine_apply(ApplyFn& func, CoroutineThrottler& throttler);
	*/

	template<bool x = METADATA_ROLLBACK>
	typename std::enable_if<x, void>::type
	clear_rollback();

	template<bool x = METADATA_ROLLBACK>
	typename std::enable_if<x, std::pair<bool, MetadataType>>::type
	do_rollback();
};


/*! Main merkle trie class.

All external code should use this class and not use TrieNode directly.


All methods should be threadsafe, although inserts concurrent with 
hashing or value accumulation will produce garbage.

Deletions aren't threadsafe with parallel accumulate_value.

Merge, insert, delete are threadsafe with each other

*/
template
<
	TriePrefix prefix_type, 
	typename ValueType = EmptyValue, 
	typename MetadataType = EmptyMetadata,
	bool USE_LOCKS = true
>
class MerkleTrie {
	constexpr static uint8_t BRANCH_BITS = 4;
public:
	using TrieT = TrieNode<prefix_type, ValueType, MetadataType, USE_LOCKS>;
	using prefix_t = prefix_type;
	constexpr static PrefixLenBits MAX_KEY_LEN_BITS = TrieT::MAX_KEY_LEN_BITS;
protected:

	using hash_t = typename TrieT::hash_t;

	typename TrieT::trie_ptr_t root;
	std::unique_ptr<std::shared_mutex> hash_modify_mtx;

	std::atomic<bool> hash_valid = false;
	hash_t root_hash;

	constexpr static bool HAS_VALUE = TrieT::HAS_VALUE;
	constexpr static bool METADATA_DELETABLE = TrieT::METADATA_DELETABLE;
	constexpr static bool HAS_METADATA = TrieT::HAS_METADATA;
	constexpr static int HAS_SIZE = TrieT::HAS_SIZE;
	constexpr static bool METADATA_ROLLBACK = TrieT::METADATA_ROLLBACK;


	void invalidate_hash() {
		hash_valid.store(false, std::memory_order_release);
	}

	void validate_hash() {
		hash_valid.store(true, std::memory_order_release);
	}

	bool get_hash_valid() {
		return hash_valid.load(std::memory_order_acquire);
	}


	void get_root_hash(Hash& out);

	void check_libsodium() {
		if (sodium_init() == -1) {
			throw std::runtime_error("Sodium init failed!!!");
		}
	}

public:

	//! Construct MT from a root pointer.
	//! Checks init of libsodium
	MerkleTrie(typename TrieT::trie_ptr_t&& root) 
		: root(std::move(root)),
		hash_modify_mtx(std::make_unique<std::shared_mutex>()) {
			check_libsodium();
		}

	//! Construct empty trie
	//! Checks libsodium init
	MerkleTrie() :
		root(TrieT::make_empty_node()),
		hash_modify_mtx(std::make_unique<std::shared_mutex>()) {
			check_libsodium();
		}


	//! Steal another trie.  Invalidates cached hash.
	//! No libsodium check because construction of other trie necessitates
	//! a libsodium check.
	MerkleTrie(MerkleTrie&& other) 
		: root(std::move(other.root))
		, hash_modify_mtx(std::move(other.hash_modify_mtx)) {}

	//! Set self equal to other.  Invalidates cached hash.
	MerkleTrie& operator=(MerkleTrie&& other) {		
		std::lock_guard lock(*hash_modify_mtx);
		invalidate_hash();
		root = std::move(other.extract_root());
		return *this;
	}


	//! Hash every node in trie.
	//! Relies on a TBB invocation.
	//! Applies ApplyFn... to every value before hashing.
	//! Writes output to \a buf.
	template<typename... ApplyFn>
	void hash(Hash& buf);

	//! Hash every node in trie.
	//! Operates in one thread - no TBB.
	//! Applies ApplyFn... to every value before hashing.
	//! Writes output to \a buf.
	template<typename... ApplyFn>
	void serial_hash(Hash& buf);


	void print_offsets() {
		root -> print_offsets();
	}

	//! Iterator for merkle trie.
	//! Interface isn't quite the same as a regular iterator interface.
	//! In particular, this checks termination by calling at_end()
	//! instead of operator== end()
	struct iterator {
		typename TrieT::iterator iter;
		
		using kv_t = typename TrieT::iterator::kv_t;

		const kv_t operator*() { return *iter;}

		iterator& operator++() { ++iter; return *this;}
		
		bool at_end() {
			return iter.at_end();
		}

		iterator(MerkleTrie& trie) : iter(*trie.root) {}
	};

	iterator begin() {
		return iterator{*this};
	}

	bool metadata_integrity_check() {
		return root -> metadata_integrity_check();
	}

	template<bool x = HAS_SIZE>
	typename std::enable_if<x, size_t>::type size() const {
		std::shared_lock<std::shared_mutex> lock(*hash_modify_mtx);
		if (root) {
			auto sz = root->size();
			if (sz < 0) {
				throw std::runtime_error("invalid trie size");
			}
			return sz;
		}
		return 0;
	}
	template<bool x = HAS_SIZE>
	typename std::enable_if<!x, size_t>::type size() const {
		std::shared_lock<std::shared_mutex> lock(*hash_modify_mtx);
		if (root) {
			return root->uncached_size();
		}
		return 0;
	}


	size_t uncached_size() const {
		std::shared_lock lock(*hash_modify_mtx);
		if (root) {
			return root -> uncached_size();
		}
		return 0;
	}
	void _log(std::string padding) {
		if (get_hash_valid()) {
			Hash buf;
			hash(buf);

			auto str = debug::array_to_str(buf.data(), buf.size());

			LOG("%s root hash: %s", padding.c_str(), str.c_str());
		}
		root->_log(padding);
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, unsigned int>::type 
	num_deleted_subnodes() const {
		std::shared_lock<std::shared_mutex> lock(*hash_modify_mtx);
		if (root) {
			return root->num_deleted_subnodes();
		}
		return 0;
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<!x, unsigned int>::type 
	num_deleted_subnodes() const {
		return 0;
	}


	//returns the metadata sum of all entries with keys <= prefix
	//TODO consider killing this method if we don't use it.
	//(it's been replaced by metadata_traversal)
	MetadataType metadata_query(
		const prefix_t query_prefix, 
		const uint16_t query_len) {
		std::shared_lock<std::shared_mutex> lock(*hash_modify_mtx);
		//requires query_len <= MAX_KEY_LEN
		return root->metadata_query(query_prefix, PrefixLenBits{query_len});
	}

	template<
		typename MetadataOutputType, 
		typename KeyInterpretationType, 
		typename KeyMakerF, 
		bool x = TrieT::HAS_METADATA>
	std::vector
	<
		IndexedMetadata<MetadataOutputType, KeyInterpretationType, KeyMakerF>
	> metadata_traversal(
		typename std::enable_if<x, const uint16_t>::type query_len_bits) {

		static_assert(x == TrieT::HAS_METADATA, "no funny business");
		if (query_len_bits > MAX_KEY_LEN_BITS.len){
			throw std::runtime_error("query too long");
		}
		using index_t
			= IndexedMetadata<
				MetadataOutputType, KeyInterpretationType, KeyMakerF>;

		std::vector<index_t> vec;

		vec.reserve(size());
		
		//otherwise size would double lock hash_modify_mtx
		std::lock_guard lock(*hash_modify_mtx);

		prefix_t zero_prefix;

		MetadataOutputType acc{};

		vec.push_back(index_t(
				KeyMakerF::eval(zero_prefix), acc)); // start with 0 at bot

		if (!root) {
			return vec;
		}

		root->metadata_traversal(vec, acc, PrefixLenBits{query_len_bits});
		return vec;
	}

	bool contains_key(const prefix_t key) {
		std::shared_lock lock(*hash_modify_mtx);
		return root -> contains_key(key);
	}

	template<typename ApplyFn>
	void apply(ApplyFn& func) {
		std::lock_guard lock(*hash_modify_mtx);

		if (!root) {
			throw std::runtime_error("root is null!");
		}
		root -> apply(func);
	}

	template<typename ApplyFn>
	void apply(ApplyFn& func) const {
		std::shared_lock lock(*hash_modify_mtx);

		if (!root) {
			throw std::runtime_error("root is null!");
		}
		root -> apply(func);
	}

	/*
	template<typename ApplyFn>
	void coroutine_apply(ApplyFn& func) {
		std::lock_guard lock(*hash_modify_mtx);

		CoroutineThrottler throttler(5);

		auto root_task = spawn_coroutine_apply(func, root.get(), throttler);

		throttler.spawn(root_task);

		throttler.join();
	} */

	template<typename ValueModifyFn>
	void
	parallel_batch_value_modify(ValueModifyFn& fn) const;

	template<typename ApplyFn>
	void
	parallel_apply(ApplyFn& fn) const;

	template<typename ApplyFn>
	void apply_geq_key(ApplyFn& func, const prefix_t min_apply_key) {
		std::shared_lock lock(*hash_modify_mtx);
		if (!root) {
			throw std::runtime_error("root is null!!!");
		}
		root -> apply_geq_key(func, min_apply_key);
	}

	template<typename ApplyFn>
	void apply_lt_key(ApplyFn& func, const prefix_t threshold_key) {
		std::shared_lock lock(*hash_modify_mtx);
		if (!root) {
			throw std::runtime_error("root is null!!!");
		}
		root -> apply_lt_key(func, threshold_key);
	}


	std::optional<prefix_t>
	get_lowest_key() {
		std::shared_lock lock(*hash_modify_mtx);

		if (!root) {
			throw std::runtime_error("ROOT IS NULL");
		}
		return root -> get_lowest_key();
	}

	MetadataType get_root_metadata() {
		std::lock_guard lock(*hash_modify_mtx);
		return root -> get_metadata_unsafe(); // ok bc exclusive lock on trie
	}

	template<bool x = HAS_VALUE>
	std::optional<ValueType> get_value(
		typename std::enable_if<x, const prefix_t&>::type query_key) {
		static_assert(x == HAS_VALUE, "no funny business");

		std::shared_lock lock(*hash_modify_mtx);

		return root -> get_value(query_key);
	}

	template<typename VectorType>
	VectorType accumulate_values() const {
		VectorType output;
		root -> accumulate_values(output);
		return output;
	}

	template<typename VectorType>
	void accumulate_values(VectorType& vec) const {
		std::lock_guard lock(*hash_modify_mtx);

		if (!root) {
			return;
		}

		vec.reserve(root -> size());

		root -> accumulate_values(vec);
	}

	template<typename VectorType>
	VectorType accumulate_values_parallel() const;

	template<typename VectorType>
	void accumulate_values_parallel(VectorType& output) const;

	//template<typename VectorType>
	//void coroutine_accumulate_values_parallel(VectorType& output) const;



	template<typename VectorType>
	VectorType accumulate_keys() {
		VectorType output;
		if (!root) {
			return output;
		}
		output.reserve(root -> size());
		root -> accumulate_keys(output);
		return output;
	}

 	TrieT*	
	get_subnode_ref_nolocks(
		const prefix_t query_prefix, const PrefixLenBits query_len_bits) {
		auto res = root 
			-> get_subnode_ref_nolocks(query_prefix, query_len_bits);
		if (res == nullptr) {
			if (!root) {
				std::printf("root shouldn't be null!!!\n");
				throw std::runtime_error("root can't be null!");
			}
			return root.get();
		}
		return res;
	}

	Proof generate_proof(prefix_t data) {
		Proof output;
		
		root -> create_proof(output, data);

		auto bytes = data.get_bytes_array();

		output.prefix.insert(output.prefix.end(), bytes.begin(), bytes.end());

		output.trie_size = size();
		root -> copy_hash_to_buf(output.root_node_hash);
		return output;
	}

	void clear_and_reset() {
		hash_modify_mtx = std::make_unique<std::shared_mutex>();
		clear();
	}

	void clear() {
		std::lock_guard lock(*hash_modify_mtx);
		root = TrieT::make_empty_node();
		invalidate_hash();
	}

	TrieT* dump_contents_for_detached_deletion_and_clear() {
		std::lock_guard lock(*hash_modify_mtx);

		TrieT* out = root.release();
		root = TrieT::make_empty_node();
		invalidate_hash();
		return out;
	}

	template<
		typename InsertFn = OverwriteInsertFn<ValueType>, 
		typename InsertedValueType = ValueType, 
		bool x = HAS_VALUE>
	void insert(
		typename std::enable_if<x, const prefix_t>::type data, 
		InsertedValueType leaf_value)
	{
		std::lock_guard lock(*hash_modify_mtx);

		invalidate_hash();
		root->template insert<x, InsertFn, InsertedValueType>(data, leaf_value);
	}

	template<typename InsertFn = OverwriteInsertFn<ValueType>, bool x = HAS_VALUE>
	void insert(typename std::enable_if<!x, const prefix_t>::type data) {
		std::lock_guard lock(*hash_modify_mtx);

		invalidate_hash();
		root->template insert<x, InsertFn>(data);
	}


	template<typename MergeFn = OverwriteMergeFn>
	void merge_in(MerkleTrie&& other) {
		std::lock_guard<std::shared_mutex> lock(*hash_modify_mtx);


		if (!other.root) {
			throw std::runtime_error("BaseTrie: merge_in other.root is null");
		}
		invalidate_hash();

		if (root -> size() == 0) {
			root = other.extract_root();;
			return;
		}

		if (other.size() == 0) {
			// Other was empty, no op
			return;
		}

		root->template merge_in<MergeFn>(std::move(other.root));
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, std::optional<ValueType>>::type
	mark_for_deletion(const prefix_t key) {
		static_assert(x == METADATA_DELETABLE, "no funny business");
		std::shared_lock<std::shared_mutex> lock(*hash_modify_mtx);
		auto [ _, value_out] = root->mark_for_deletion(key);

		if (value_out) {
			invalidate_hash();
		}

		return value_out;
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, std::optional<ValueType>>::type
	unmark_for_deletion(const prefix_t key) {
		static_assert(x == METADATA_DELETABLE, "no funny business");
		std::shared_lock<std::shared_mutex> lock(*hash_modify_mtx);
		auto [_ , value_out] = root->unmark_for_deletion(key);

		if (value_out) {
			invalidate_hash();
		}
		
		return value_out;
	}
	
	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, void>::type
	perform_marked_deletions() {
		//no locks bc invokes perform_marked_deletions to get lock
		auto null_side_effects = NullOpDelSideEffectFn{};
		perform_marked_deletions(null_side_effects);
	}

	template<bool x = METADATA_DELETABLE, typename DelSideEffectFn>
	typename std::enable_if<x, void>::type
	perform_marked_deletions(DelSideEffectFn& side_effect_handler) {
		static_assert(x == METADATA_DELETABLE, "no funny business");
		std::lock_guard lock(*hash_modify_mtx);
		if (root->get_metadata_unsafe().num_deleted_subnodes == 0) { //ok bc exclusive lock on root
			return;
		}

		invalidate_hash();
		auto res = root->perform_marked_deletions(side_effect_handler);

		if (res.first) {
			root = TrieT::make_empty_node();
		}
		if (root->single_child()) {
			root = root -> get_single_child();
		}
	}

	void clean_singlechild_nodes(const prefix_t explore_path) {
		std::lock_guard lock(*hash_modify_mtx);

		invalidate_hash();

		root -> clean_singlechild_nodes(explore_path);

		while(root->single_child()) {
			root = root -> get_single_child();
		}
	}

	template<bool x = METADATA_DELETABLE>
	typename std::enable_if<x, void>::type
	clear_marked_deletions() {
		std::shared_lock lock(*hash_modify_mtx);
		root -> clear_marked_deletions();
	}

//	template<bool x = METADATA_DELETABLE>
//	typename std::enable_if<x, void>::type
//	mark_subtree_lt_key_for_deletion(const prefix_t max_key) {
//		std::shared_lock lock(*hash_modify_mtx);
//		root -> mark_subtree_lt_key_for_deletion(max_key);
//	}

//	template<bool x = METADATA_DELETABLE>
//	typename std::enable_if<x, void>::type
//	mark_entire_tree_for_deletion() {
//		std::shared_lock lock(*hash_modify_mtx);
//		root -> mark_subtree_for_deletion();
//	}

	template<bool x = HAS_VALUE>
	typename std::conditional<x, std::optional<ValueType>, bool>::type
	perform_deletion(const prefix_t key) {
		std::lock_guard lock(*hash_modify_mtx);

		TRIE_INFO("starting new delete");
		auto [ delete_child, anything_deleted, _] = root->perform_deletion(key);

		if (anything_deleted) {
			 invalidate_hash();
		}

		if (delete_child) {
			root = TrieT::make_empty_node();
		}
		if (root->single_child()) {
			root = root -> get_single_child();
		}
		if (root -> size() == 0) {
			root = TrieT::make_empty_node();
		}

		using return_t 
			= typename std::conditional<
				x, std::optional<ValueType>, bool>::type;

		return (return_t) anything_deleted;
	}


	/*! Split the trie according to the endowment threshold.

	Specifically, peels off the lowest endow_threshold worth of offers
	from the trie.

	Rounds down.  I.e. it does not remove a 10 endow offer if
	endow_threshold is 8.
	*/
	MerkleTrie endow_split(int64_t endow_threshold);

	//concurrent modification might cause shorn reads
	int64_t 
	endow_lt_key(const prefix_t max_key) const {
		std::shared_lock lock(*hash_modify_mtx);
		return root->endow_lt_key(max_key);
	}

	std::unique_ptr<TrieT> extract_root() {
		std::lock_guard lock(*hash_modify_mtx);
		auto out = std::unique_ptr<TrieT>(root.release());
		root = TrieT::make_empty_node();
		invalidate_hash();
		return out;
	}

	template<typename MergeFn = OverwriteMergeFn>
	void batch_merge_in(std::vector<std::unique_ptr<TrieT>>&& tries);

	//! Invalidates hash from root to target node.
	void
	invalidate_hash_to_node_nolocks(TrieT* target) {
		invalidate_hash();
		root -> invalidate_hash_to_node_nolocks(target);
	}


	template<bool x = METADATA_ROLLBACK>
	typename std::enable_if<x, void>::type
	do_rollback() {
		std::lock_guard lock(*hash_modify_mtx);
		root -> do_rollback();
		if (root -> single_child()) {
			root = root -> get_single_child();
		}
	}

	template<bool x = METADATA_ROLLBACK>
	typename std::enable_if<x, void>::type
	clear_rollback() {
		std::lock_guard lock(*hash_modify_mtx);
		root -> clear_rollback();
	}
};



#define TEMPLATE_SIGNATURE template<TriePrefix prefix_type, typename ValueType, typename MetadataType, bool USE_LOCKS>
#define TEMPLATE_PARAMS prefix_type, ValueType, MetadataType, USE_LOCKS

//template instantiations:

TEMPLATE_SIGNATURE
void TrieNode<TEMPLATE_PARAMS>::_log(std::string padding) const {
	LOG("%sprefix %s (len %d bits)",
		padding.c_str(), 
		prefix.to_string(prefix_len).c_str(),
		prefix_len.len);
	if (get_hash_valid()) {
		LOG("%snode hash is: %s", 
			padding.c_str(), debug::array_to_str(hash.data(), 32).c_str());
	}
	if (prefix_len == MAX_KEY_LEN_BITS) {
		std::vector<unsigned char> buf;
		auto value = children.value();
		//value.serialize();
		value.copy_data(buf);
		auto str = debug::array_to_str(buf.data(), buf.size());
		LOG("%svalue serialization is %s", padding.c_str(), str.c_str());
		buf.clear();
	}
	LOG("%saddress: %p", padding.c_str(), this);
	LOG("%smetadata: %s", padding.c_str(), metadata.to_string().c_str());
	LOG("%snum children: %d, is_frozen: %d, bv: %x",
		padding.c_str(), children.size(), 0, children.get_bv());

	for (unsigned int bits = 0; bits <= MAX_BRANCH_VALUE; bits++) {
		auto iter = children.find(bits);
		if (iter != children.end()) {
			LOG("%schild: %x, parent_status:%s",
				padding.c_str(), 
				(*iter).first, (((*iter).second)?"true":"false"));
			(*iter).second->_log(padding+" |    ");
		}
	}
}

//! Get branch bits of \a data at offset equal to the current prefix len
TEMPLATE_SIGNATURE
unsigned char 
TrieNode<TEMPLATE_PARAMS>::get_branch_bits(const prefix_t& data) const {
	
	return data.get_branch_bits(prefix_len);
}

TEMPLATE_SIGNATURE
size_t 
TrieNode<TEMPLATE_PARAMS>::uncached_size() const {
	if (prefix_len == MAX_KEY_LEN_BITS) {
		return 1;
	} 
	unsigned int sz = 0;

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		sz += (*iter).second->uncached_size();
	}
	return sz;
}

//! Return metadata of all subnodes <= query_prefix (up to query_len)
//! query for 0x1234 with len 16 (bits) matches 0x1234FFFFF but not 0x1235
TEMPLATE_SIGNATURE
const MetadataType 
TrieNode<TEMPLATE_PARAMS>::metadata_query(
	const prefix_t query_prefix, 
	const PrefixLenBits query_len) {
	
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	auto prefix_match_len = get_prefix_match_len(
		query_prefix, query_len);

	if (prefix_match_len == query_len) {
		return metadata;
	}

	if (prefix_len > query_len) {

		// if prefix_len > query_len
		// (and thus prefix_match_len < query_len), we do not have a match.
		// hence return empty;
		return MetadataType{};
	}

	auto branch_bits = get_branch_bits(query_prefix);
	MetadataType metadata_out{};

	for (unsigned less_bb = 0; less_bb < branch_bits; less_bb++) {
		auto iter = children.find(less_bb);
		if (iter != children.end()) {
			metadata_out += children->second->get_metadata();
		}
	}
	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		return metadata_out;
	}
	metadata_out += iter->second->metadata_query(query_prefix, query_len);
	return metadata_out;
}

TEMPLATE_SIGNATURE
template<
	typename MetadataOutputType, 
	typename KeyInterpretationType, 
	typename KeyMakerF>
void TrieNode<TEMPLATE_PARAMS>::metadata_traversal(
	std::vector<IndexedMetadata<
					MetadataOutputType, 
					KeyInterpretationType, 
					KeyMakerF>>& vec, 
	MetadataOutputType& acc_metadata,
	const PrefixLenBits query_len) 
{

	using index_t
			= IndexedMetadata<
				MetadataOutputType, KeyInterpretationType, KeyMakerF>;

	//no lock needed if root gets exclusive lock

	if (prefix_len >= query_len) {
		auto interpreted_key = KeyMakerF::eval(prefix);

		 // safe bc BaseTrie::metadata_traversal gets exclusive lock on root
		acc_metadata += MetadataOutputType(	
			interpreted_key, metadata.unsafe_load());

		vec.push_back(index_t(interpreted_key, acc_metadata));
		return;
	}

	for (uint8_t branch_bits = 0; 
		branch_bits <= MAX_BRANCH_VALUE; 
		branch_bits++)
	{
		auto iter = children.find(branch_bits);
		if (iter != children.end()) {

			(*iter).second->metadata_traversal(
				vec, acc_metadata, query_len);
		}
	}
}

/*

NODE:
prefix len in bits: (2bytes)
prefix
bitvector representing which children present
for each child (sorted)
	hash of child

ROOT:
number of children [4bytes]
hash of root node

*/
namespace {

template <typename ValueType, typename prefix_t>
static void 
compute_hash_value_node(
	Hash& hash_buf, 
	const prefix_t prefix, 
	const PrefixLenBits prefix_len, 
	ValueType& value) {

	std::vector<unsigned char> digest_bytes;

	write_node_header(digest_bytes, prefix, prefix_len);

	value.copy_data(digest_bytes);
	
	if (crypto_generichash(
		hash_buf.data(), hash_buf.size(), 
		digest_bytes.data(), digest_bytes.size(), 
		NULL, 0) != 0) {
		throw std::runtime_error("error in crypto_generichash");
	}
}

template<
	typename Map, 
	typename bv_t, 
	bool IGNORE_DELETED_SUBNODES, 
	typename prefix_t, 
	typename... ApplyToValueBeforeHashFn>
static 
typename std::enable_if<!IGNORE_DELETED_SUBNODES, void>::type 
compute_hash_branch_node(
	Hash& hash_buf, 
	const prefix_t prefix, 
	const PrefixLenBits prefix_len, 
	const Map& children) {
	
	bv_t bv;
	for (auto iter = children.begin(); iter != children.end(); iter++) {
		bv.add((*iter).first);
		if (!(*iter).second) {
			throw std::runtime_error("can't recurse hash down null ptr");
		}
		(*iter).second->template compute_hash<ApplyToValueBeforeHashFn...>();

	}
	uint8_t num_children = children.size();

	std::vector<unsigned char> digest_bytes;


	write_node_header(digest_bytes, prefix, prefix_len);

	bv.write(digest_bytes);

	for (uint8_t i = 0; i < num_children; i++) {
		auto iter = children.find(bv.pop());

		if (!(*iter).second) {
			throw std::runtime_error("bv gave invalid answer!");
		}
		(*iter).second->append_hash_to_vec(digest_bytes);

	}

	TRIE_INFO("hash input:%s", 
		debug::array_to_str(digest_bytes.data(), digest_bytes.size()).c_str());
	if (crypto_generichash(
		hash_buf.data(), hash_buf.size(), 
		digest_bytes.data(), digest_bytes.size(), 
		NULL, 0) != 0) {
		throw std::runtime_error("error in crypto_generichash");
	}
}

template<
	typename Map, 
	typename bv_t, 
	bool IGNORE_DELETED_SUBNODES, 
	typename prefix_t, 
	typename... ApplyToValueBeforeHashFn>
static 
typename std::enable_if<IGNORE_DELETED_SUBNODES, void>::type 
compute_hash_branch_node(
	Hash& hash_buf, 
	const prefix_t prefix, 
	const PrefixLenBits prefix_len, 
	const Map& children) {
	

	bv_t bv;
	for (auto iter = children.begin(); iter != children.end(); iter++) {

		// ok bc either root has exclusive lock 
		// or caller has exclusive lock on node
		auto child_meta = (*iter).second->get_metadata_unsafe(); 

		if (child_meta.size > child_meta.num_deleted_subnodes) {
			bv.add((*iter).first);
			if (!(*iter).second) {
				throw std::runtime_error("can't recurse hash down null ptr");
			}
			(*iter).second
				 ->template compute_hash<ApplyToValueBeforeHashFn...>();
		}
		if (child_meta.size < child_meta.num_deleted_subnodes) {
			std::printf(
				"child_meta size: %lu child num_deleted_subnodes: %d\n", 
				child_meta.size, 
				child_meta.num_deleted_subnodes);
			(*iter).second->_log("my subtree:");
			throw std::runtime_error("invalid num deleted subnodes > size");
		}

	}
	uint8_t num_children = bv.size();

	if (num_children == 1) {
		// single valid subnode, 
		// subsume hash of the child instead of hashing this

		auto iter = children.find(bv.pop());
		if (!(*iter).second) {
			throw std::runtime_error(
				"tried to look for nonexistent child node! (bv err)");
		}

		(*iter).second->copy_hash_to_buf(hash_buf);
		return;
	}

	std::vector<unsigned char> digest_bytes;

	write_node_header(digest_bytes, prefix, prefix_len);

	bv.write(digest_bytes);

	for (uint8_t i = 0; i < num_children; i++) {
		auto iter = children.find(bv.pop());
		if (!(*iter).second) {
			throw std::runtime_error("bv error?");
		}
		(*iter).second -> append_hash_to_vec(digest_bytes);
	}

	TRIE_INFO("hash input:%s", 
		debug::array_to_str(digest_bytes.data(), digest_bytes.size()).c_str());

	if (crypto_generichash(
		hash_buf.data(), hash_buf.size(), 
		digest_bytes.data(), digest_bytes.size(), 
		NULL, 0) != 0) {
		throw std::runtime_error("error in crypto_generichash");
	}
}

} /* anonymous namespace */

/*! Computes hash of current node.

Applies ApplyToValueBeofreHashFn... to each value before hashing.
*/
TEMPLATE_SIGNATURE
template<typename... ApplyToValueBeforeHashFn>
void 
TrieNode<TEMPLATE_PARAMS>::compute_hash() {

	TRIE_INFO("starting compute_hash on prefix %s (len %d bits)",
		prefix.to_string(prefix_len).c_str(),
		prefix_len.len);

	auto hash_buffer_is_valid = get_hash_valid();

	if (hash_buffer_is_valid) return;

	if (children.empty()) {

		auto& value = children.value();

		(ApplyToValueBeforeHashFn::apply_to_value(value),...);
		compute_hash_value_node(hash, prefix, prefix_len, value);
	} else {
		compute_hash_branch_node<
			children_map_t,
			bv_t,
			METADATA_DELETABLE, 
			prefix_t, 
			ApplyToValueBeforeHashFn...>(hash, prefix, prefix_len, children);
	}

	validate_hash();
}

/*! Computes hash of root of merkle trie.
Hash input is [trie size - 4 bytes][root node hash - 32 bytes]

Assumes no more than 2^32 nodes in trie.
*/
TEMPLATE_SIGNATURE
void 
MerkleTrie<TEMPLATE_PARAMS>::get_root_hash(Hash& out) {
	//caller must lock node, so no lock here

	uint32_t num_children = root->size() - root -> num_deleted_subnodes();
	TRIE_INFO("hash num children: %d", num_children);
	constexpr int buf_size = 4 + 32;
	std::array<unsigned char, buf_size> buf;
	buf.fill(0);

	write_unsigned_big_endian(buf, num_children);
	if (num_children > 0) {
		root->copy_hash_to_buf(buf.data() + 4);
	} else if (num_children == 0) {
		//taken care of by fill above
	} else {
		throw std::runtime_error("invalid number of children");
	}
	INFO("top level hash in num children: %lu", num_children);
	INFO("top level hash in: %s", 
		debug::array_to_str(buf, 36).c_str());
	
	crypto_generichash(out.data(), out.size(), buf.data(), buf.size(), NULL, 0);

	//SHA256(buf.data(), buf.size(), out.data());
	INFO("top level hash out: %s", 
		debug::array_to_str(out.data(), out.size()).c_str());
}

TEMPLATE_SIGNATURE
template <typename ...ApplyFn>
void MerkleTrie<TEMPLATE_PARAMS>::serial_hash(Hash& buffer) {

	std::lock_guard lock(*hash_modify_mtx);

	if (get_hash_valid()) {
		buffer = root_hash;
		return;
	}

	if (!root) {
		throw std::runtime_error("root should not be nullptr in hash");	
	}

	root -> compute_hash();

	get_root_hash(root_hash);

	buffer = root_hash;
	validate_hash();
}

TEMPLATE_SIGNATURE
template <typename ...ApplyFn>
void 
MerkleTrie<TEMPLATE_PARAMS>::hash(Hash& buffer) {

	std::lock_guard lock(*hash_modify_mtx);

	if (get_hash_valid()) {

		buffer = root_hash;
		return;
	}


	tbb::parallel_for(
		HashRange<TrieT>(root),
		[] (const auto& r) {
			for (size_t idx = 0; idx < r.num_nodes(); idx++) {
				r[idx] -> template compute_hash<ApplyFn...>();
			}
		});

	root -> template compute_hash<ApplyFn...>();

	get_root_hash(root_hash);
	buffer = root_hash;
	validate_hash();
}


/*! Parallelize the work of merging in a pre-specified batch of tries.

Main trie (to which everything is merged in) should not be empty.  This'll work
better if the main trie is reasonably spread out over the whole keyspace.
*/
TEMPLATE_SIGNATURE
template<typename MergeFn>
void
MerkleTrie<TEMPLATE_PARAMS>::batch_merge_in(
	std::vector<std::unique_ptr<TrieT>>&& tries) {
	
	BatchMergeRange<TrieT, MetadataType> range(root.get(), std::move(tries));

	static_assert(USE_LOCKS,
		"need locks on individual nodes to do parallel merging");

	BatchMergeReduction<MergeFn> reduction{};

	tbb::parallel_reduce(range, reduction);
}

/*!
Called "destructive" because it destroys the invariant that a node has children.
Only should be used in relation to batch_merge_in.


Input: prefix, len of node to steal.  Specifically, attempts to steal
any node that FULLY matches the input prefix.

Returns: bool (tell parent to remove this),
		 metadata (meta delta if node is removed)
		 pointer (pointer of node to be removed)
*/
TEMPLATE_SIGNATURE
std::tuple<bool, MetadataType, typename TrieNode<TEMPLATE_PARAMS>::trie_ptr_t>
TrieNode<TEMPLATE_PARAMS>::destructive_steal_child(
	const prefix_t stealing_prefix, 
	const PrefixLenBits stealing_prefix_len) {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::exclusive_lock_t>();

	auto prefix_match_len = get_prefix_match_len(
		stealing_prefix, stealing_prefix_len);

	if (prefix_match_len == stealing_prefix_len) { // prefix_match_len <= prefix_len
		//full match, steal entire subtree
		return std::make_tuple(true, get_metadata_unsafe(), nullptr);
	}

	if (prefix_match_len == prefix_len) {
		//Implies perfect match up until prefix_len < stealing_prefix_len, 
		//so we can do a recursion

		auto branch_bits = get_branch_bits(stealing_prefix);

		auto iter = children.find(branch_bits);
		if (iter == children.end()) {
			//nothing to do
			return std::make_tuple(false, MetadataType{}, nullptr);
		}

		if (!(*iter). second) {
			std::printf("nullptr errror!");
			throw std::runtime_error("destructive_steal_child found a nullptr");
		}

		auto [do_steal_entire_subtree, meta_delta, ptr] = (*iter).second 
			 -> destructive_steal_child(stealing_prefix, stealing_prefix_len);

		if (do_steal_entire_subtree) {
			update_metadata(-meta_delta);

			auto out = children.extract((*iter).first);

			return std::make_tuple(false, meta_delta, std::move(out));
		} else {
			if (ptr) {
				update_metadata(-meta_delta);
				return std::make_tuple(false, meta_delta, std::move(ptr));
			} else {
				return std::make_tuple(false, MetadataType{}, nullptr);
			}
		}
	}

	//prefix_len > prefix_match_len, so there's no valid subtree to steal
	return std::make_tuple(false, MetadataType{}, nullptr);
}

//! propagates a metadata down to target (DOES NOT add metadata to target)
TEMPLATE_SIGNATURE
void
TrieNode<TEMPLATE_PARAMS>::propagate_metadata(
	TrieNode* target, MetadataType metadata) {

	invalidate_hash();
	if (target == this) {
		return;
	}

	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	auto branch_bits = get_branch_bits(target -> prefix);

	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		throw std::runtime_error(
			"can't propagate metadata to nonexistent node");
	}


	update_metadata(branch_bits, metadata);
	(*iter).second -> propagate_metadata(target, metadata);
}

TEMPLATE_SIGNATURE
void
TrieNode<TEMPLATE_PARAMS>::invalidate_hash_to_node_nolocks(TrieNode* target) {
	invalidate_hash(); // invalidate hash is threadsafe
	if (target == this) {
		return;
	}

	auto branch_bits = get_branch_bits(target -> prefix);

	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		throw std::runtime_error(
			"can't propagate metadata to nonexistent node");
	}

	(*iter).second -> invalidate_hash_to_node_nolocks(target);
}

TEMPLATE_SIGNATURE
ProofNode 
TrieNode<TEMPLATE_PARAMS>::create_proof_node() {
	if (prefix_len == MAX_KEY_LEN_BITS) {
		ProofNode output;
		write_unsigned_big_endian(
			output.prefix_length_and_bv.data(), prefix_len.len);
		return output;
	}

	ProofNode output;

	bv_t bv;
	for (auto iter = children.begin(); iter != children.end(); iter++) {
		bv.add((*iter).first);
	}

	write_unsigned_big_endian(
		output.prefix_length_and_bv.data(), prefix_len.len);

	bv.write_to(output.prefix_length_and_bv.data() 
					+ prefix_len.bytes_to_write_len());

	PROOF_INFO("prefix_len = %u data=%s", 
		prefix_len, 
		debug::array_to_str(output.prefix_length_and_bv.data(), 4).c_str());


	while (!bv.empty()) {
		auto cur_child_bits = bv.pop();
		Hash h;
		children.at(cur_child_bits)->copy_hash_to_buf(h);
		output.hashes.push_back(h);
	}
	return output;
}

TEMPLATE_SIGNATURE
void TrieNode<TEMPLATE_PARAMS>::create_proof(Proof& proof, prefix_t data) {

	proof.nodes.push_back(create_proof_node());

	if (prefix_len == MAX_KEY_LEN_BITS) {
		proof.membership_flag = 1;
		children.value().copy_data(proof.value_bytes);
		return;
	}
	
	auto branch_bits = get_branch_bits(data);
	auto iter = children.find(branch_bits);

	if (iter != children.end()) {
		(*iter).second->create_proof(proof, data);
	}

}

TEMPLATE_SIGNATURE
template<bool x, typename InsertFn, typename InsertedValueType>
void TrieNode<TEMPLATE_PARAMS>::insert(
	typename std::enable_if<x, const prefix_t&>::type data,
	const InsertedValueType& leaf_value) {

	static_assert(x == HAS_VALUE, "no funny games");
	_insert<InsertFn, InsertedValueType>(data, leaf_value);
}

TEMPLATE_SIGNATURE
template<bool x, typename InsertFn>
void TrieNode<TEMPLATE_PARAMS>::insert(
	typename std::enable_if<!x, const prefix_t&>::type data) {
	static_assert(x == HAS_VALUE, "no funny games");

	TRIE_INFO("Starting insert of value %s",
		data.to_string(MAX_KEY_LEN_BITS).c_str());

	TRIE_INFO("current size:%d", size());

	_insert<InsertFn, EmptyValue>(data, EmptyValue());
}

TEMPLATE_SIGNATURE
template<typename InsertFn, typename InsertedValueType>
const MetadataType 
TrieNode<TEMPLATE_PARAMS>::_insert(
	const prefix_t& data, const InsertedValueType& leaf_value) {

	invalidate_hash();

	TRIE_INFO("Starting insert to prefix %s (len %u bits) %p",
		prefix.to_string(prefix_len).c_str(),
		prefix_len.len,
		this);
	TRIE_INFO("num children: %d", children.size());

	if (children.size() == 1) {
		std::printf("fucked\n");
		_log("what the fuck");
		std::fflush(stdout);
		throw std::runtime_error("children size should never be 1 (insert)");
	}

	if (children.empty() 
			&& !(prefix_len == MAX_KEY_LEN_BITS || prefix_len.len == 0)) {
		std::printf(
			"invalid init: prefix_len=%d, num children = %lu, max len = %u\n", 
			prefix_len.len, children.size(), MAX_KEY_LEN_BITS.len);
		throw std::runtime_error("invalid initialization somewhere");
	}


	auto prefix_match_len = get_prefix_match_len(data);

	if (prefix_match_len > MAX_KEY_LEN_BITS) {
		throw std::runtime_error("invalid prefix match len!");
	}

	TRIE_INFO("prefix match len is %d", prefix_match_len.len);
	if (prefix_len.len == 0 && children.empty()) {
		TRIE_INFO("node is empty, no children");
		//initial node ONLY
		prefix = data;
		prefix_len = MAX_KEY_LEN_BITS;
		
		//new value
		children.set_value(InsertFn::new_value(prefix));

		InsertFn::value_insert(children.value(), leaf_value);
		//value = leaf_value;
		if (HAS_METADATA) {
			metadata.clear();
			metadata += (InsertFn::template new_metadata<MetadataType>(
				children.value()));
		}
		//new leaf: metadata change is += leaf_metadata
		return metadata.unsafe_load();
	} else if (prefix_match_len > prefix_len) {
		throw std::runtime_error("invalid prefix match len!");
	}
	else if (prefix_match_len == prefix_len) {
		if (prefix_len == MAX_KEY_LEN_BITS) {
			TRIE_INFO("overwriting existing key");
			InsertFn::value_insert(children.value(), leaf_value);
			//value = leaf_value;
			if (HAS_METADATA) {
				//value = leaf_value already.  This gets around
				// the case where leaf_value is not ValueType.
				// Returns the difference between the metadata associated with
				// the new value - the old metadata, sets metadata to new
				// metadata.
				return InsertFn::metadata_insert(metadata, children.value());
			}
			return MetadataType();
		}
		TRIE_INFO("full prefix match, recursing");
		auto branch_bits = get_branch_bits(data);
		auto iter = children.find(branch_bits);
		if (iter != children.end()) {
			TRIE_INFO("found previous child");
			auto& child = (*iter).second;
			
			if (HAS_METADATA) {
				auto metadata_delta 
					= child->template _insert<InsertFn>(data, leaf_value);
				update_metadata(metadata_delta);
				return metadata_delta;
			} else {
				child->template _insert<InsertFn>(data, leaf_value);
				return MetadataType{};
			}
		} else {
			TRIE_INFO("make new leaf");
			auto new_child = make_value_leaf<InsertFn, InsertedValueType>(
					data, leaf_value);
			MetadataType new_child_meta;
			if (HAS_METADATA) {
				// safe b/c of exclusive lock on *this
				new_child_meta = new_child->metadata.unsafe_load();
				update_metadata(new_child_meta);
			}
			children.emplace(branch_bits, std::move(new_child));
			return new_child_meta;
		}

	} else {
		TRIE_INFO("i don't extend current prefix, doing a break after %d", 
			prefix_match_len);
		auto original_child_branch = move_contents_to_new_node();
		if (HAS_METADATA) {
			// ok bc exclusive lock on *this,
			// and original_child_branch is local var
			original_child_branch->set_metadata_unsafe(metadata); 
		}

		auto new_child 
			= make_value_leaf<InsertFn, InsertedValueType>(data, leaf_value);
		
		children.clear();

		//this becomes the join of original_child_branch and new_child
		prefix_len = prefix_match_len;
		auto branch_bits = get_branch_bits(data);
		
		auto new_child_metadata 
			= new_child->metadata.unsafe_load(); //ok bc exclusive lock on *this
		auto original_child_metadata 
			= metadata.unsafe_load(); // ok bc exclusive lock on *this

		TRIE_INFO("new_child_metadata:%s", 
			new_child_metadata.to_string().c_str());
		TRIE_INFO("original_child_metadata:%s", 
			original_child_metadata.to_string().c_str());

		children.emplace(branch_bits, std::move(new_child));
		auto old_branch_bits = get_branch_bits(prefix);
		children.emplace(old_branch_bits, std::move(original_child_branch));

		if (branch_bits == old_branch_bits) {
			throw std::runtime_error("we split at the wrong index!");
		}

		if (children.size() != 2) {
			throw std::runtime_error("invalid children size!");
		}

		if(prefix_len.len / 8 >= KEY_LEN_BYTES) {
			throw std::runtime_error("invalid prefix_len");
		}

		prefix.truncate(prefix_len);

		if (HAS_METADATA) {
			metadata.clear();
			update_metadata(new_child_metadata);
			update_metadata(original_child_metadata);
			return new_child_metadata;
		}
		return MetadataType();
	}
}

TEMPLATE_SIGNATURE
template<typename MergeFn>
const MetadataType 
TrieNode<TEMPLATE_PARAMS>::_merge_in(trie_ptr_t&& other) {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::exclusive_lock_t>();
	invalidate_hash();

	if (!other) {
		throw std::runtime_error("other is null!!!");
	}

	// Note that other->size() could be 0 in an intermediate stage of 
	// batch_merge_in, due to subtrie stealing.
	// This could mean we'll insert an empty non-leaf node somewhere in the
	// trie.  This is ok -- this is guaranteed to get filled in at some point
	// during the batch merge.  An empty non-leaf gets added when all of the
	// node's children were stolen.  Those children will get merged in later.

	if (children.size() == 0 && prefix_len.len == 0) {
		throw std::runtime_error(
			"cannot _merge into empty trie!  Must call merge or somehow guard against this.");
	}

	TRIE_INFO("Starting merge_in to prefix %s (len %d bits)",
		prefix.to_string(prefix_len).c_str(),
		prefix_len.len);
	TRIE_INFO("num children: %d", children.size());
	for (auto iter = children.begin(); iter != children.end(); iter++) {
		TRIE_INFO("child: %x, parent_status:%s", 
			(*iter).first, 
			(((*iter).second)?"true":"false"));
		if ((*iter). first > MAX_BRANCH_VALUE) {
			throw std::runtime_error("invalid iter->first!");
		}
	}
	TRIE_INFO_F(_log("current state:    "));

	TRIE_INFO("other is not null?:%s", (other?"true":"false"));
	TRIE_INFO("other prefix len %d", other->prefix_len);
	
	auto prefix_match_len
		= get_prefix_match_len(other -> prefix, other -> prefix_len);

	if (prefix_match_len > MAX_KEY_LEN_BITS) {
		throw std::runtime_error("invalid too long prefix_match_len");
	}

	// Merge Case 0: two nodes are both leaves

	if (prefix_match_len == MAX_KEY_LEN_BITS) { //merge case 0
		TRIE_INFO("Full match, nothing to do but copy value");

		if (HAS_VALUE) {
			MergeFn::value_merge(children.value(), other->children.value());
			//value = other->value;
		}
		if (HAS_METADATA) {
			TRIE_INFO("\toriginal_metadata: %s", 
				metadata.unsafe_load().to_string().c_str());

			auto metadata_delta = MergeFn::metadata_merge(
				metadata, other->metadata);

			TRIE_INFO("\tnew metadata:%s", 
				metadata.unsafe_load().to_string().c_str());
			TRIE_INFO("\tmetadata delta:%s", 
				metadata_delta.to_string().c_str());
			return metadata_delta;
			//overwrite leaf: metadata change is += (new - old)
		}
		return MetadataType();
	}

	//Merge Case 1:
	// both nodes have identical prefixes
	// we just take union of children maps, merge duplicate children

	if (prefix_len == other->prefix_len && prefix_len == prefix_match_len) {
		TRIE_INFO("Merging nodes with prefix %s (len %d bits)",
			prefix.to_string(prefix_len).c_str(),
			prefix_len.len);

		MetadataType metadata_delta = MetadataType();

		for (auto other_iter = other->children.begin(); 
			other_iter != other->children.end(); 
			other_iter++) {

			if ((*other_iter).first > MAX_BRANCH_VALUE) {
				throw std::runtime_error("invalid other->first");
			}

			auto main_iter = children.find((*other_iter).first);
			TRIE_INFO("Processing BRANCH_BITS = %x", (*other_iter).first);
			if (main_iter == children.end()) {
				TRIE_INFO("didn't preexist");
				if (HAS_METADATA) {
					//safe bc exclusive lock on this 
					// implies exclusive lock on children
					auto child_metadata 
						= (*other_iter).second->metadata.unsafe_load(); 
					metadata_delta += child_metadata;
					update_metadata(child_metadata);
				}
				children.emplace(
					(*other_iter).first, 
					other->children.extract((*other_iter).first));
			} else { 
				/*
				main_iter->first == other_iter->first, 
				i.e. duplicate child node
				*/
				TRIE_INFO("preexisting");
				auto& merge_child = (*main_iter).second;

				if (!merge_child) {
					throw std::runtime_error("merging into nullptr!");
				}

				if (HAS_METADATA) {
					auto child_metadata = (merge_child
						->template _merge_in<MergeFn>(
							other->children.extract((*other_iter).first)));
					metadata_delta += child_metadata;
					update_metadata(child_metadata);
				} else {
					merge_child->template _merge_in<MergeFn>(
						other->children.extract((*other_iter).first));
				}
			}
			TRIE_INFO("current meta_delta:%s",
				metadata_delta.to_string().c_str());
		}
		TRIE_INFO("done merge");
		return metadata_delta;
	}

	// Merge Case 2
	// complete match on this prefix up to main_iter's prefix len, 
	// but other's prefix extends
	// thus other must become a child of this

	if (prefix_len == prefix_match_len /* and thus other->prefix_len > prefix_match_len*/) {
		//merge in with a child
		TRIE_INFO("recursing down subtree");

		if (other -> prefix_len < prefix_len) {
			std::printf("prefix_len %u other->prefix_len %u\n", 
				prefix_len.len, other->prefix_len.len);
			throw std::runtime_error("cannot happen!");
		}
		auto branch_bits = get_branch_bits(other->prefix);
		auto iter = children.find(branch_bits);
		if (iter == children.end()) {
			TRIE_INFO("making new subtree");
			// safe bc moving other into this 
			// fn gives us implicit exclusive ownership
			auto other_metadata = other->metadata.unsafe_load(); 
			children.emplace(branch_bits, std::move(other));
			if (HAS_METADATA) {
				update_metadata(other_metadata);
				return other_metadata;
			} else {
				return MetadataType();
			}
		}
		TRIE_INFO("using existing subtree");

		auto& merge_child = (*iter).second;

		if (HAS_METADATA) {
			auto delta = merge_child 
				-> template _merge_in<MergeFn>((std::move(other)));
			update_metadata(delta);
			//metadata += delta;
			return delta;
		} else {
			merge_child-> template _merge_in<MergeFn>((std::move(other)));
			return MetadataType();
		}
	}

	// Merge Case 3 (Converse of case 2)
	// this prefix is an extension of other's prefix.  
	// Hence, this must become child of other

	if (other->prefix_len == prefix_match_len /* and thus prefix_len > prefix_match_len*/) {
		TRIE_INFO("merge case 3");

		auto original_child_branch = move_contents_to_new_node();
		if (HAS_METADATA) {
			// safe bc obj is threadlocal rn and exc lock on metadata
			original_child_branch->set_metadata_unsafe(metadata); 
		}

		children.clear();
		children = std::move((other->children));
		
		other -> children.clear();

		prefix_len = other -> prefix_len;

		auto original_child_branch_bits = get_branch_bits(prefix);
		
		prefix = other->prefix;

		// safe bc move gives us implicit exclusive
		// ownership of other and lock on this
		set_metadata_unsafe(other->metadata);
		
		TRIE_INFO ("original_child_branch_bits: %d", original_child_branch_bits);

		auto iter = children.find(original_child_branch_bits);
		if (iter==children.end()) {
			TRIE_INFO("no recursion case 3");
			auto original_child_metadata = original_child_branch->metadata.unsafe_load(); // safe bc obj is threadlocal rn
			children.emplace(original_child_branch_bits, std::move(original_child_branch));
			if (HAS_METADATA) {
				update_metadata(original_child_metadata);
				return other->metadata.unsafe_load(); //safe bc move gives exclusive access to thread
			} else {
				return MetadataType{};
			}
		} else {
			TRIE_INFO("case 3 recursing");
			auto original_metadata = original_child_branch->metadata.unsafe_load(); // safe bc original_child_branch is threadlocal

			 // children was replaced by other's children
			auto matching_subtree_of_other = children.extract((*iter).first);

			
			
			if (!matching_subtree_of_other) {
				throw std::runtime_error("matching_subtree_of_other is null???");
			}



			//We do the swap here so that the input to _merge is always destructible, as per invariant.
			children.emplace(original_child_branch_bits, std::move(original_child_branch));


			//metadata adjustment corresponding to swapping pre-merge the matching subtrees
			// safe bc obj is threadlocal after the std::move call
			auto metadata_reduction 
				= matching_subtree_of_other->metadata.unsafe_load(); 
			metadata_reduction -= original_metadata;

			auto meta_delta = children.at(original_child_branch_bits)
				-> template _merge_in<MergeFn>(
					std::move(matching_subtree_of_other));

			meta_delta -= metadata_reduction;


			
			//auto meta_delta = iter->second->_merge_in(std::move(original_child_branch));
			//TRIE_INFO_F(_log("post case 3 recursion:    "));
			if (HAS_METADATA) {
				update_metadata(meta_delta);
				//metadata += meta_delta;
				auto change_from_original = metadata.unsafe_load(); // safe bc exclusive lock on this
				change_from_original -= original_metadata;
				return change_from_original;
			}
			else {
				return MetadataType{};
			}
		}

	}

	// Merge case 4:
	// We must create a common ancestor of both this and other.

	/* other->prefix_len > prefix_match_len && prefix_len > prefix_match_len */ //merge case 4

	auto original_child_branch = move_contents_to_new_node();
	if (HAS_METADATA) {
		original_child_branch->set_metadata_unsafe(metadata);// safe bc obj is threadlocal and exclusive lock on metadata
	}
	children.clear();
	prefix_len = prefix_match_len;

	auto original_branch_bits = get_branch_bits(prefix);
	auto other_branch_bits = get_branch_bits(other->prefix);
	auto other_metadata = other->metadata.unsafe_load(); // safe bc other is xvalue etc

	children.emplace(original_branch_bits, std::move(original_child_branch));
	children.emplace(other_branch_bits, std::move(other));

	//>= instead of > because we don't want equality here - prefix_len has been reduced to match len from it's potentially maximal length.
	if (prefix_len.num_fully_covered_bytes() >= KEY_LEN_BYTES) {
		throw std::runtime_error("invalid prefix_len");
	}

	prefix.truncate(prefix_len);

	if (HAS_METADATA) {

		auto original_child_metadata = metadata.unsafe_load(); // safe bc exclusve lock on self
		metadata.clear();

		update_metadata(original_child_metadata);
		update_metadata(other_metadata);
		return other_metadata;
	} else {
		return MetadataType();
	}
}

TEMPLATE_SIGNATURE
PrefixLenBits TrieNode<TEMPLATE_PARAMS>::get_prefix_match_len(
	const prefix_t& other, const PrefixLenBits other_len) const {

	return prefix.get_prefix_match_len(prefix_len, other, other_len);
}

TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, std::tuple<MetadataType, std::optional<ValueType>>>::type
TrieNode<TEMPLATE_PARAMS>::unmark_for_deletion(const prefix_t key) {
	static_assert(x == METADATA_DELETABLE, "no funny business");

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	auto prefix_match_len = get_prefix_match_len(key);

	if (prefix_match_len < prefix_len) {
		//incomplete match, which means that key doesn't exist.
		return std::make_tuple(MetadataType(), std::nullopt);
	}

	if (prefix_len == MAX_KEY_LEN_BITS) {
		auto swap_lambda = [] (AtomicDeletableMixin& object) {
			AtomicDeletableMixin::BaseT::value_t expect = 1, desired = 0;
			return AtomicDeletableMixin::compare_exchange(
				object, expect, desired);
		};
		bool res = swap_lambda(metadata);

		//bool res = metadata.num_deleted_subnodes
		//	.compare_exchange_strong(1, 0);
		//swaps 1 to 0 if it's 1, otherwise it's 0 already.
		auto meta_out = MetadataType();
		meta_out.num_deleted_subnodes = res?-1:0;
		if (!res) {
			return std::make_tuple(meta_out, std::nullopt);
		} else {
			invalidate_hash();
			return std::make_tuple(meta_out, children.value());
		}
	}

	auto branch_bits = get_branch_bits(key);

	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		return std::make_tuple(MetadataType(), std::nullopt);
	}

	auto res = (*iter).second->unmark_for_deletion(key);
	auto [metadata_change, deleted_obj] = res;

	if (deleted_obj) {
		invalidate_hash();
	}
	update_metadata(metadata_change);
	return res;

}

TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, std::tuple<MetadataType, std::optional<ValueType>>>::type
TrieNode<TEMPLATE_PARAMS>::mark_for_deletion(const prefix_t key) {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	auto prefix_match_len = get_prefix_match_len(key);

	if (prefix_match_len < prefix_len) {
		//incomplete match, which means that key doesn't exist.
		return std::make_tuple(MetadataType(), std::nullopt);
	}

	if (prefix_match_len == MAX_KEY_LEN_BITS) {

		auto swap_lambda = [] (AtomicDeletableMixin& object) {
			AtomicDeletableMixin::BaseT::value_t expect = 0, desired = 1;
			return AtomicDeletableMixin::compare_exchange(
				object, expect, desired);
		};
		bool res = swap_lambda(metadata);
		//swaps 0 to 1 if it's 0, otherwise it's 1 already.
		auto meta_out = MetadataType();
		meta_out.num_deleted_subnodes = res?1:0;
		if (!res) {
			return std::make_tuple(meta_out, std::nullopt);

			//hash not invalidated because nothing deleted
		} else {
			invalidate_hash();
			return std::make_tuple(meta_out, children.value());
		}

	}

	auto branch_bits = get_branch_bits(key);

	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		return std::make_tuple(MetadataType(), std::nullopt);

		//hash not invalidated because nothing marked as deleted
	}

	invalidate_hash();
	auto res = (*iter).second->mark_for_deletion(key);
	auto [ metadata_change, deleted_obj] = res;

	if (deleted_obj) {
		invalidate_hash();
	}
	update_metadata(metadata_change);
	return res;
}

TEMPLATE_SIGNATURE
template<bool x, typename DelSideEffectFn>
typename std::enable_if<x, std::pair<bool, MetadataType>>::type
TrieNode<TEMPLATE_PARAMS>::perform_marked_deletions(
	DelSideEffectFn& side_effect_handler)
{
	static_assert(x == METADATA_DELETABLE, "no funny business");

	//no lock needed because MerkleTrie wrapper gets exclusive lock


	TRIE_INFO("Starting perform_marked_deletions to prefix %s (len %d bits)",
		prefix.to_string(prefix_len).c_str(),
		prefix_len.len);
	TRIE_INFO("num children: %d", children.size());
	TRIE_INFO("metadata:%s", metadata.to_string().c_str());
	TRIE_INFO_F(_log("current subtree:    "));

	if (metadata.num_deleted_subnodes == 0) {
		TRIE_INFO("no subnodes, returning");
		return std::make_pair(false, MetadataType());
	}

	invalidate_hash();

	if (prefix_len == MAX_KEY_LEN_BITS && metadata.num_deleted_subnodes == 1) {
		side_effect_handler(prefix, children.value());
		return std::make_pair(true, -metadata.unsafe_load()); // safe bc exclusive lock
	}

	if (prefix_len == MAX_KEY_LEN_BITS && metadata.num_deleted_subnodes != 0) {
		throw std::runtime_error(
			"can't have num deleted subnodes not 0 or 1 at leaf");
	}


	// We could do the following: when an entire subtree is deleted, just 
	// wholesale delete the trie.  We do not, so that we can pass 
	// side_effect_handler to each value.

	//if (metadata.num_deleted_subnodes == size()) {
	//	TRIE_INFO("deleting entire subtree");
	//	return std::make_pair(true, -metadata.duplicate());
	//}

	auto metadata_delta = MetadataType();


	for (unsigned int branch_bits = 0; 
		branch_bits <= MAX_BRANCH_VALUE; 
		branch_bits++) {
		TRIE_INFO("scanning branch bits %d", branch_bits);

		auto iter = children.find(branch_bits);
		if (iter == children.end()) {
			continue;
		}

		auto& child_ptr = (*iter).second;

		if (!child_ptr) {
			_log("bad node");
			throw std::runtime_error(
				"tried to perform marked deletions on null child");
		}


		auto result = child_ptr->perform_marked_deletions(side_effect_handler);
		update_metadata(result.second);
		if (result.first) {
			TRIE_INFO("deleting subtree");
			children.erase(branch_bits);
		} else {
			if (child_ptr->single_child()) {

				auto single_child = child_ptr->get_single_child();
				TRIE_INFO("contracting size 1 subtree, prefix len %d",
					prefix_len);

				children.emplace((*iter).first, std::move(single_child));
			} 
		}
		metadata_delta += result.second;
	}
	TRIE_INFO("done scanning");
	return std::make_pair(children.empty(), metadata_delta);
}

TEMPLATE_SIGNATURE
void 
TrieNode<TEMPLATE_PARAMS>::clean_singlechild_nodes(const prefix_t explore_path) {
	[[maybe_unused]]
	auto lock = locks.template lock<exclusive_lock_t>();
	invalidate_hash();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		return;
	}

	for (uint8_t bb = 0; bb <= MAX_BRANCH_VALUE;) {
		auto iter = children.find(bb);
		if (iter == children.end()) {
			bb++;
		} else {
			auto& ptr = (*iter).second;
			if (ptr->single_child()) {
				children.emplace(
					(*iter).first, std::move(ptr->get_single_child()));
			} else {
				bb++;
			}
		}
	}

	auto bb = get_branch_bits(explore_path);

	auto next_iter = children.find(bb);
	if (next_iter != children.end()) {
		(*next_iter).second -> clean_singlechild_nodes(explore_path);
	}
}

TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, void>::type
TrieNode<TEMPLATE_PARAMS>::clear_marked_deletions() {
	static_assert(x == METADATA_DELETABLE, "no funny business");

	if (metadata.unsafe_load().num_deleted_subnodes == 0) { // ok bc root gets exclusive lock
		return;
	}
	metadata.num_deleted_subnodes = 0;

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		(*iter).second -> clear_marked_deletions();
	}
}


TEMPLATE_SIGNATURE
//First value is TRUE IFF parent should delete child
//next is "has anything been deleted"
std::tuple<bool, std::optional<ValueType>, MetadataType>
TrieNode<TEMPLATE_PARAMS>::perform_deletion(const prefix_t key) {
	
	//no lock needed bc root gets exclusive lock
	//std::lock_guard lock(*mtx);
	
	TRIE_INFO("deleting key %s",
		key.to_string(MAX_KEY_LEN_BITS).c_str());

	TRIE_INFO("deleting from current prefix %s (len %d bits)",
		prefix.to_string(prefix_len).c_str(),
		prefix_len.len);
	TRIE_INFO("num children: %d", children.size());

	auto prefix_match_len = get_prefix_match_len(key);

	if (prefix_match_len < prefix_len) {
		//incomplete match, which means that key doesn't exist.
		TRIE_INFO("key doesn't exist");
		return std::make_tuple(
			false, std::nullopt, MetadataType());
	}

	if (prefix_match_len == MAX_KEY_LEN_BITS) {
		TRIE_INFO("key deleted, removing");
		TRIE_INFO("metadata out:%s", 
			(-metadata.unsafe_load()).to_string().c_str());
 		
 		// safe bc exclusive lock on *this
		return std::make_tuple(true, children.value(), -metadata.unsafe_load());
	}

	auto branch_bits = get_branch_bits(key);
	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		//key isn't present, only 
		TRIE_INFO("no partial match, key must not exist");
		return std::make_tuple(false, std::nullopt, MetadataType());
	}

	auto& child_ptr = (*iter).second;

	if (!child_ptr) {
		throw std::runtime_error("perform_deletion found nullptr");
	}

	auto [delete_child, deleted_obj, metadata_delta] 
		= child_ptr->perform_deletion(key);

	if (deleted_obj) {
		invalidate_hash();
	}

	TRIE_INFO("key deleted, delete_child=%d", delete_child);
	if (delete_child) {
		children.erase(branch_bits);
	} else if (child_ptr->children.size() == 1) {
		TRIE_INFO("only one child, subsuming");

		auto replacement_child_ptr = child_ptr -> get_single_child();
		children.emplace(branch_bits, std::move(replacement_child_ptr));
	}
	update_metadata(metadata_delta);
	return std::make_tuple(false, deleted_obj, metadata_delta);
}

TEMPLATE_SIGNATURE
bool TrieNode<TEMPLATE_PARAMS>::contains_key(const prefix_t key) {
	
	[[maybe_unused]]
	auto lock = locks.template lock<std::shared_lock<std::shared_mutex>>();

	auto prefix_match_len = get_prefix_match_len(key);
	if (prefix_match_len < prefix_len) {
		return false;
	}
	if (prefix_match_len == MAX_KEY_LEN_BITS) {
		return true;
	}
	auto branch_bits = get_branch_bits(key);
	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		return false;
	}
	return (*iter).second->contains_key(key);
}


TEMPLATE_SIGNATURE
typename TrieNode<TEMPLATE_PARAMS>::trie_ptr_t
TrieNode<TEMPLATE_PARAMS>::endow_split(
	int64_t endow_threshold) {

	//no lock needed because root gets exclusive lock
	[[maybe_unused]]
	auto lock = locks.template lock<exclusive_lock_t>();

	if (children.empty()) {
		TRIE_INFO(
			"leaf -- returning null (endow_threshold < endowment, so no split");
		return nullptr;
	}

	if (endow_threshold >= metadata.endow) {
		throw std::runtime_error(
			"shouldn't have reached this far down - entire node is consumed");
	}

	if (endow_threshold < 0) {
		throw std::runtime_error("endow threshold can't be negative");
	}

	int64_t acc_endow = 0;

	invalidate_hash();

	children_map_t new_node_children;
	new_node_children.clear(); // set map as active union member

	for(unsigned char branch_bits = 0; 
		branch_bits <= MAX_BRANCH_VALUE; 
		branch_bits ++) {


		auto iter = children.find(branch_bits);
		if (iter != children.end()) {
			auto fully_consumed_subtree 
				= acc_endow + (*iter).second->metadata.endow;

			if (fully_consumed_subtree <= endow_threshold) {
				//fully consume subnode
				
				// safe bc exclusive lock on *this, which owns the children
				update_metadata(-((*iter).second->metadata.unsafe_load())); 
				auto new_child = children.extract((*iter).first);
				new_node_children.emplace((*iter).first, std::move(new_child));
				//children.erase(iter);

			} else {

				if (!(*iter).second) {
					throw std::runtime_error("invalid iter in endow_split!");
				}
				auto split_obj = (*iter).second->endow_split(
					endow_threshold - acc_endow);
				if (split_obj) {
					update_metadata(-(split_obj->metadata.unsafe_load()));
					new_node_children.emplace(
						(*iter).first, std::move(split_obj));
				}

				if ((*iter).second -> single_child()) {
					auto single_child = (*iter).second -> get_single_child();
					children.emplace((*iter).first, std::move(single_child));
					//children[(*iter).first] = std::move(single_child);
					//children.emplace(iter->first, std::move(single_child));
					if (!children.at((*iter).first)) {
						throw std::runtime_error("what the fuck children map");
					}
				}
			}
			acc_endow = fully_consumed_subtree;
		}

		if (acc_endow >= endow_threshold) {
			break;
		}
	}
	if (new_node_children.size()) {

		auto output = duplicate_node_with_new_children(
			std::move(new_node_children));

		// ok because output is threadlocal right now
		output->compute_metadata_unsafe(); 
		TRIE_INFO("current metadata:%s", output->metadata.to_string().c_str());
		//TRIE_INFO("returning exceeds %d split", split_idx);
		TRIE_INFO_F(output->_log("returned value:"));

		return output;
	}

	return nullptr;
}

TEMPLATE_SIGNATURE
MerkleTrie<TEMPLATE_PARAMS>
MerkleTrie<TEMPLATE_PARAMS>::endow_split(int64_t endow_threshold) {
	std::lock_guard lock(*hash_modify_mtx);

	if (endow_threshold == 0) {
		return MerkleTrie();
	}

	invalidate_hash();

	if (!root) {
		throw std::runtime_error("can't consume from empty trie");
	}

	//ok bc exclusive lock on hash_modify_mtx
	auto root_endow = root -> get_metadata_unsafe().endow; 
	if (endow_threshold > root_endow) {
		throw std::runtime_error("not enough endow");
	}
	if (endow_threshold == root_endow) {
		//consuming entire trie
		auto out = MerkleTrie(std::move(root));
		root = TrieT::make_empty_node();
		return out;
	}

	auto ptr = root -> endow_split(endow_threshold);

	if (root -> single_child()) {
		root =root -> get_single_child();
	}
	if (ptr) {
		return MerkleTrie(std::move(ptr));
	} else {
		return MerkleTrie();
	}
}

TEMPLATE_SIGNATURE
template<typename VectorType>
VectorType 
MerkleTrie<TEMPLATE_PARAMS>::accumulate_values_parallel() const {
	VectorType output;
	accumulate_values_parallel(output);
	return output;
}

/*
TEMPLATE_SIGNATURE
template<typename VectorType>
void 
MerkleTrie<TEMPLATE_PARAMS>::coroutine_accumulate_values_parallel(
	VectorType& output) const {

	std::lock_guard lock(*hash_modify_mtx);
	if (!root) {
		return;
	}

	try {
		output.resize(root -> size());
	} catch (...) {
		std::printf("failed to resize with size %lu\n", root -> size());
		throw;
	}

	AccumulateValuesRange<TrieT> range(root);

	tbb::parallel_for(
		range,
		[&output] (const auto& range) {
			auto vector_offset = range.vector_offset;
			CoroutineThrottler throttler(2);
			//std::printf("starting range with work_list sz %lu\n", 
				range.work_list.size());

			for (size_t i = 0; i < range.work_list.size(); i++) {



				if (throttler.full()) {
					range.work_list[i]
						->coroutine_accumulate_values_parallel_worker(
							output, throttler, vector_offset);
				} else {
					throttler.spawn(
						spawn_coroutine_accumulate_values_parallel_worker(
							output, *range.work_list[i], throttler, vector_offset));
				}
				vector_offset += range.work_list[i]->size();
			}
			throttler.join();
		});
}
*/

TEMPLATE_SIGNATURE
template<typename VectorType>
void 
MerkleTrie<TEMPLATE_PARAMS>::accumulate_values_parallel(
	VectorType& output) const {

	std::lock_guard lock(*hash_modify_mtx);

	if (!root) {
		return;
	}

	output.resize(root -> size());

	AccumulateValuesRange<TrieT> range(root);

	tbb::parallel_for(
		range,
		[&output] (const auto& range) {

			auto vector_offset = range.vector_offset;
			for (size_t i = 0; i < range.work_list.size(); i++) {
				range.work_list[i]->accumulate_values_parallel_worker(output, vector_offset);
				vector_offset += range.work_list[i]->size();
			}
		});
}
/*
template<typename VectorType, typename TrieT>
auto 
spawn_coroutine_accumulate_values_parallel_worker(VectorType& vec, TrieT& target, CoroutineThrottler& throttler, size_t offset) -> RootTask {
	//auto& target_dereferenced = co_await PrefetchAwaiter{target, throttler.scheduler};
	target.coroutine_accumulate_values_parallel_worker(vec, throttler, offset);
	co_return;
}


TEMPLATE_SIGNATURE
template<typename VectorType>
NoOpTask
TrieNode<TEMPLATE_PARAMS>::coroutine_accumulate_values_parallel_worker(VectorType& vec, CoroutineThrottler& throttler, size_t offset) {

	//std::printf("starting accumulate_values_parallel_worker\n");
	if (prefix_len == MAX_KEY_LEN_BITS) {
		//will cause problems if this we use custom accs, instead of just vecs
		auto& set_loc = co_await WriteAwaiter(&(vec[offset]), throttler.scheduler);
		set_loc = children.value();
		//vec[offset] = value;
		co_return;
	}
	for (auto iter = children.begin(); iter != children.end(); iter++) {

		auto& child = co_await ReadAwaiter{(*iter).second.get(), throttler.scheduler};
		size_t child_sz = child.size();

		if (!throttler.scheduler.full()) {
			throttler.spawn(spawn_coroutine_accumulate_values_parallel_worker(vec, child, throttler, offset));
		} else {
			child.coroutine_accumulate_values_parallel_worker(vec, throttler, offset);
		}
		offset += child_sz;
	}
	co_return;
} */


TEMPLATE_SIGNATURE
template<typename VectorType>
void
TrieNode<TEMPLATE_PARAMS>::accumulate_values_parallel_worker(VectorType& vec, size_t offset) const {
	if (prefix_len == MAX_KEY_LEN_BITS) {
		if (offset > vec.size()) {
			throw std::runtime_error("invalid access");
		}
		vec[offset] = children.value();
		return;
	}
	for (unsigned int branch_bits = 0; branch_bits <= MAX_BRANCH_VALUE; branch_bits ++) {
		auto iter = children.find(branch_bits);
		if (iter != children.end()) {
			if (!(*iter).second) {
				_log("bad node");
				throw std::runtime_error("tried to accumulate value from empty node");
			}
			(*iter).second -> accumulate_values_parallel_worker(vec, offset);
			offset += (*iter).second -> size();
		}
	}
}

TEMPLATE_SIGNATURE
template<typename ApplyFn>
void
MerkleTrie<TEMPLATE_PARAMS>::parallel_apply(ApplyFn& fn) const {

	std::shared_lock lock(*hash_modify_mtx);

	ApplyRange<TrieT> range(root);

	tbb::parallel_for(
		range,
		[&fn] (const auto& r) {
			for (size_t i = 0; i < r.work_list.size(); i++) {
				r.work_list.at(i)->apply(fn);
			}
		});
}

TEMPLATE_SIGNATURE
template<typename ValueModifyFn>
void
MerkleTrie<TEMPLATE_PARAMS>::parallel_batch_value_modify(
	ValueModifyFn& fn) const {

	std::lock_guard lock(*hash_modify_mtx);

	ApplyRange<TrieT> range(root);
	//guaranteed that range.work_list contains no overlaps
	tbb::parallel_for(
		range,
		[&fn] (const auto& range) {
			for (unsigned int i = 0; i < range.work_list.size(); i++) {
				fn(range.work_list[i]);
			}
		});
}

//concurrent modification will cause problems here
TEMPLATE_SIGNATURE
TrieNode<TEMPLATE_PARAMS>*
TrieNode<TEMPLATE_PARAMS>::get_subnode_ref_nolocks(
	const prefix_t query_prefix, const PrefixLenBits query_len) {

	auto prefix_match_len = get_prefix_match_len(query_prefix, query_len);

	if (prefix_match_len == query_len) {
		return this;
	}

	if (prefix_match_len > prefix_len) {
		throw std::runtime_error("cannot happen!");
	}

	if (prefix_match_len < prefix_len) {
		return nullptr;
	}

	if (prefix_match_len == prefix_len) {
		auto bb = get_branch_bits(query_prefix);
		auto iter = children.find(bb);
		if (iter == children.end()) {
			throw std::runtime_error("can't recurse down nonexistent subtree!");
		}
		if (!(*iter).second) {
			_log("bad node: ");
			throw std::runtime_error("found a null iter in get_subnode_ref_nolocks");
		}

		auto child_candidate = (*iter).second 
				 -> get_subnode_ref_nolocks(query_prefix, query_len);
		if (child_candidate == nullptr) {
			return this;
		}
		return child_candidate;
	}
	throw std::runtime_error("invalid recursion");
}

TEMPLATE_SIGNATURE
template<typename ModifyFn>
void
TrieNode<TEMPLATE_PARAMS>::modify_value_nolocks(
	const prefix_t query_prefix, ModifyFn& fn) {
	
	invalidate_hash();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		fn(children.value());
		return;
	}

	auto prefix_match_len = get_prefix_match_len(query_prefix);
	if (prefix_match_len != prefix_len) {

		std::printf("my prefix: %s %d\n query: %s\n", 
			prefix.to_string(prefix_len).c_str(), 
			prefix_len.len,
			query_prefix.to_string(MAX_KEY_LEN_BITS).c_str());
		throw std::runtime_error("invalid recurison: value nonexistent");
	}

	auto bb = get_branch_bits(query_prefix);

	auto iter = children.find(bb);
	if (iter == children.end()) {
		throw std::runtime_error(
			"branch bits not found: can't modify nonexistent value");
	}

	if (!(*iter). second) {
		throw std::runtime_error(
			"modify_value_nolocks found a nullptr lying around");
	}

	(*iter).second -> modify_value_nolocks(query_prefix, fn);
}

TEMPLATE_SIGNATURE
template<typename ApplyFn>
void 
TrieNode<TEMPLATE_PARAMS>::apply(ApplyFn& func) {

	if (prefix_len == MAX_KEY_LEN_BITS) {
		if (children.size() != 0) {
			throw std::runtime_error("leaves have no children");
		}
		if (size() != 1) {
			_log("failed node: ");
			throw std::runtime_error("invalid size in apply");
		}
		func(children.value());
		return;
	}

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		if (!(*iter).second) {
			throw std::runtime_error(
				"null pointer len = " + std::to_string(prefix_len.len));
		}
		if ((*iter).first > MAX_BRANCH_VALUE) {
			throw std::runtime_error("invalid branch bits!");
		}
		(*iter).second->apply(func);
	}
}

/*
template<typename ApplyFn, typename TrieT>
auto 
spawn_coroutine_apply(ApplyFn& func, TrieT* target, CoroutineThrottler& throttler) -> RootTask {

	//std::printf("spawning new coroutine! %p\n", target);

	auto& target_dereferenced = co_await PrefetchAwaiter{target, throttler.scheduler};

	//std::printf("woke up! %p\n", target);
	target_dereferenced.coroutine_apply(func, throttler);//.h_.resume();
	//std::printf("done apply\n");
	co_return;
} */


/*
template<typename TrieT>
struct CoroutineTrieNodeWrapper {
	//TrieT* current_node;

	template<typename ApplyFn>
	auto coroutine_apply_task(
		ApplyFn& fn, TrieT* target, CoroutineThrottler& throttler) -> RootTask {


		auto coroutine_apply_lambda = [this] (
			ApplyFn& fn, TrieT* target, 
			CoroutineThrottler& throttler, 
			auto& recursion_callback) -> NoOpTask {

			auto& this_node = co_await PrefetchAwaiter{target, throttler.scheduler};

			if (this_node.prefix_len == TrieT::MAX_KEY_LEN_BITS) {
				fn(this_node.value);
				co_return;
			}




			for (auto iter = this_node.children.begin(); iter != this_node.children.end(); iter++) {
				if (!throttler.scheduler.full()) {

					auto new_task = coroutine_apply_task(
						fn, (*iter).second.get(), throttler);
					throttler.spawn(new_task);
				} else {
					recursion_callback(fn, (*iter).second.get(), throttler, recursion_callback);
				}
			}
			co_return;
		};

		coroutine_apply_lambda(fn, target, throttler, coroutine_apply_lambda);
		co_return;

	}


};*/

/*
TEMPLATE_SIGNATURE
template<typename ApplyFn>
NoOpTask
TrieNode<TEMPLATE_PARAMS>::coroutine_apply(ApplyFn& func, CoroutineThrottler& throttler) {

	if (prefix_len == MAX_KEY_LEN_BITS) {
		func(children.value());
		co_return;
	}
	for (auto iter = children.begin(); iter != children.end(); iter++) {

		if (!throttler.scheduler.full()) {
			throttler.spawn(spawn_coroutine_apply(func, (*iter).second, throttler));
		} else {
			auto& child = co_await PrefetchAwaiter{(*iter).second, throttler.scheduler};
			child.coroutine_apply(func, throttler);
		}
	}

	co_return;
} */


TEMPLATE_SIGNATURE
template<typename ApplyFn>
void 
TrieNode<TEMPLATE_PARAMS>::apply(ApplyFn& func) const {

	//[[maybe_unused]]
	//auto lock = locks.template lock<TrieNode::shared_lock_t>();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		func(children.value());
		return;
	}

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		(*iter).second->apply(func);
	}
}


TEMPLATE_SIGNATURE
template<typename ApplyFn>
void TrieNode<TEMPLATE_PARAMS>::apply_geq_key(
	ApplyFn& func, const prefix_t min_apply_key) {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	if (prefix_len == MAX_KEY_LEN_BITS) {

		if (prefix >= min_apply_key) {
			func(prefix, children.value());
			if (children.size() != 0) {
				throw std::runtime_error("what the fuck");
			}
		}
		return;	
	}

	prefix_t min_apply_key_truncated = min_apply_key;

	min_apply_key_truncated.truncate(prefix_len);
	//truncate_prefix(min_apply_key_truncated, prefix_len, MAX_KEY_LEN_BITS);


	if (prefix > min_apply_key_truncated) {
		if (USE_LOCKS) {
			//can't acquire lock twice
			for (auto iter = children.begin(); iter != children.end(); iter++) {
				(*iter).second->apply(func);
			}
		} else {
			apply(children.value());
		}
		return;
	}

	if (prefix < min_apply_key_truncated) {
//	if (prev_diff_res < 0) {
		
		//is possible, not a problem
		//throw std::runtime_error("should be impossible this time");
		//std::printf("%s too low, ignoring\n", DebugUtils::__array_to_str(prefix, __num_prefix_bytes(prefix_len)).c_str());
		return;
	}


	auto min_branch_bits = get_branch_bits(min_apply_key);

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		if ((*iter).first == min_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}

			(*iter).second->apply_geq_key(func, min_apply_key);
		} else if ((*iter).first > min_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}
			(*iter).second -> apply(func);
		}
	}
}


TEMPLATE_SIGNATURE
template<typename ApplyFn>
void TrieNode<TEMPLATE_PARAMS>::apply_lt_key(
	ApplyFn& func, const prefix_t threshold_key) {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		if (prefix < threshold_key) {
			func(children.value());
			if (children.size() != 0) {
				throw std::runtime_error("what the fuck");
			}
		}
		return;	
	}

	prefix_t threshold_key_truncated;
	threshold_key_truncated = threshold_key;

	threshold_key_truncated.truncate(prefix_len);
	//truncate_prefix(threshold_key_truncated, prefix_len, MAX_KEY_LEN_BITS);

	if (prefix < threshold_key_truncated) {
		if (USE_LOCKS) {
			//can't acquire lock twice
			for (auto iter = children.begin(); iter != children.end(); iter++) {
				(*iter).second->apply(func);
			}
		} else {
			apply(func);
		}
		return;
	}

	if (prefix > threshold_key_truncated) {
	//if (prev_diff_res > 0) {
		
		//is possible, not a problem
		//throw std::runtime_error("should be impossible this time");
		//std::printf("%s too low, ignoring\n", DebugUtils::__array_to_str(prefix, __num_prefix_bytes(prefix_len)).c_str());
		return;
	}


	auto min_branch_bits = get_branch_bits(threshold_key);

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		if ((*iter).first == min_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}

			(*iter).second->apply_lt_key(func, threshold_key);
		} else if ((*iter).first < min_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}
			(*iter).second -> apply(func);
		}
	}
}

/*
TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, MetadataType>::type
TrieNode<TEMPLATE_PARAMS>::mark_subtree_for_deletion() {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		if (children.size() != 0) {
			throw std::runtime_error("leaves have no children");
		}
		auto swap_lambda = [] (AtomicDeletableMixin& object) {
			AtomicDeletableMixin::BaseT::value_t expect = 0, desired = 1;
			return AtomicDeletableMixin::compare_exchange(
				object, expect, desired);
		};
		bool res = swap_lambda(metadata);
		//swaps 0 to 1 if it's 0, otherwise it's 1 already.
		auto meta_out = MetadataType{};
		meta_out.num_deleted_subnodes = res?1:0;
		if (!res) {
			return meta_out;
			//hash not invalidated because nothing deleted
		} else {
			invalidate_hash();
			return meta_out;

		}
	}

	MetadataType meta_delta{};

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		if (!(*iter).second) {
			throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
		}
		auto local_delta = (*iter).second->mark_subtree_for_deletion();
		update_metadata((*iter).first, local_delta);
		meta_delta += local_delta;
	}
	if (meta_delta.num_deleted_subnodes != 0) {
		invalidate_hash();
	}
	return meta_delta;
}



//strictly below max_key is deleted
TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, MetadataType>::type
TrieNode<TEMPLATE_PARAMS>::mark_subtree_lt_key_for_deletion(
	const prefix_t max_key) {

	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();


	if (prefix_len == MAX_KEY_LEN_BITS) {
		if (prefix < max_key) {
			if (children.size() != 0) {
				throw std::runtime_error("what the fuck");
			}

			auto swap_lambda = [] (AtomicDeletableMixin& object) {
				AtomicDeletableMixin::BaseT::value_t expect = 0, desired = 1;
				return AtomicDeletableMixin::compare_exchange(
					object, expect, desired);
			};
			bool res = swap_lambda(metadata);
			//swaps 0 to 1 if it's 0, otherwise it's 1 already.
			auto meta_out = MetadataType();
			meta_out.num_deleted_subnodes = res?1:0;
			if (!res) {
				return meta_out;
				//hash not invalidated because nothing deleted
			} else {
				invalidate_hash();
				return meta_out;

			}
		}
		return MetadataType{};
	}

	prefix_t max_key_truncated = max_key;

	//truncate_prefix(max_key_truncated, prefix_len, MAX_KEY_LEN_BITS);
	max_key_truncated.truncate(prefix_len);

	if (prefix < max_key_truncated) {
		if (USE_LOCKS) {
			auto metadata_out = MetadataType{};
			//can't acquire lock twice
			for (auto iter = children.begin(); iter != children.end(); iter++) {
				auto local_delta = (*iter).second->mark_subtree_for_deletion();
				update_metadata((*iter).first, local_delta);
				metadata_out += local_delta;
			}
			return metadata_out;
		} else {
			return mark_subtree_for_deletion();
		}
	} else if (prefix > max_key_truncated) {
		return MetadataType{};
		//is possible, not a problem
		//throw std::runtime_error("mark lt key should be impossible this time");
		//std::printf("%s too high, ignoring\n", DebugUtils::__array_to_str(prefix, __num_prefix_bytes(prefix_len)).c_str());
	}


	auto max_branch_bits = get_branch_bits(max_key);
	auto meta_delta = MetadataType{};
	for (auto iter = children.begin(); iter != children.end(); iter++) {
		if ((*iter).first == max_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error(
					"null pointer len = " + std::to_string(prefix_len.len));
			}

			auto local_delta = (*iter).second->mark_subtree_lt_key_for_deletion(max_key);
			update_metadata((*iter).first, local_delta);
			meta_delta += local_delta;
		} else  if ((*iter).first < max_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}
			auto local_delta = (*iter).second -> mark_subtree_for_deletion();
			update_metadata((*iter).first, local_delta);
			meta_delta += local_delta;

		}
	}
	if (meta_delta.num_deleted_subnodes != 0) {
		invalidate_hash();
	}
	return meta_delta;
}
*/

TEMPLATE_SIGNATURE
int64_t 
TrieNode<TEMPLATE_PARAMS>::endow_lt_key(const prefix_t max_key) const {
	
	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		if (prefix < max_key) {
			if (children.size() != 0) {
				throw std::runtime_error("what the fuck");
			}
			return metadata.unsafe_load().endow;
			
		}
		return 0;
	}

	prefix_t max_key_truncated = max_key;

	//truncate_prefix(max_key_truncated, prefix_len, MAX_KEY_LEN_BITS);
	max_key_truncated.truncate(prefix_len);

	if (prefix < max_key_truncated) {
		return metadata.unsafe_load().endow;
	}
	else if (prefix > max_key_truncated) {
		return 0;
	}
//	if (prev_diff_res > 0) {
//		
//		return 0;
//		
		//actually is possible, is not a problem.
		//throw std::runtime_error("should be impossible this time");
		//std::printf("%s too low, ignoring\n", DebugUtils::__array_to_str(prefix, __num_prefix_bytes(prefix_len)).c_str());
//	}


	auto max_branch_bits = get_branch_bits(max_key);
	int64_t valid_endow = 0;
	for (auto iter = children.begin(); iter != children.end(); iter++) {
		if ((*iter).first == max_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}

			valid_endow += (*iter).second->endow_lt_key(max_key);
		} else  if ((*iter).first < max_branch_bits) {
			if (!(*iter).second) {
				throw std::runtime_error("null pointer len = " + std::to_string(prefix_len.len));
			}
			valid_endow += (*iter).second -> get_metadata_unsafe().endow;
		}
	}
	return valid_endow;
}

TEMPLATE_SIGNATURE
std::optional<typename TrieNode<TEMPLATE_PARAMS>::prefix_t>
TrieNode<TEMPLATE_PARAMS>::get_lowest_key() {
	//std::shared_lock lock(*mtx);
	
	[[maybe_unused]]
	auto lock = locks.template lock<shared_lock_t>();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		return prefix;
	}
	for (unsigned char i = 0; i <= MAX_BRANCH_VALUE; i++) {
		auto iter = children.find(i);
		if (iter != children.end()) {
			return (*iter).second->get_lowest_key();
		}
	}
	return std::nullopt; // empty trie
}

TEMPLATE_SIGNATURE
template<bool x>
std::optional<ValueType> 
TrieNode<TEMPLATE_PARAMS>::get_value(
	typename std::enable_if<x, const prefix_t&>::type query_key) {
	
	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t> ();

	if (prefix_len == MAX_KEY_LEN_BITS) {
		return std::make_optional(children.value());
	}

	auto branch_bits = get_branch_bits(query_key);

	auto iter = children.find(branch_bits);
	if (iter == children.end()) {
		return std::nullopt;
	}

	return (*iter).second->get_value(query_key);
}


TEMPLATE_SIGNATURE
template<typename VectorType>
void
TrieNode<TEMPLATE_PARAMS>::accumulate_values(VectorType& output) const {	
	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();
	if (prefix_len == MAX_KEY_LEN_BITS) {
		output.push_back(children.value());
		return;
	}

	if (size() < 0 || size() > 1000000000) {
		//sanity check, TODO remove.
		std::printf("size=%lu\n", size());
		_log("trie: ");
		throw std::runtime_error("invalid size!!!");
	}



	for (unsigned bb = 0; bb <= MAX_BRANCH_VALUE; bb++) {
		auto iter = children.find(bb);
		if (iter != children.end()) {
			(*iter).second -> accumulate_values(output);
		}
	}
}

TEMPLATE_SIGNATURE
template<typename VectorType>
void
TrieNode<TEMPLATE_PARAMS>::accumulate_keys(VectorType& output) {
	[[maybe_unused]]
	auto lock = locks.template lock<TrieNode::shared_lock_t>();
	if (prefix_len == MAX_KEY_LEN_BITS) {
		output.add_key(prefix);
		return;
	}

	for (unsigned bb = 0; bb <= MAX_BRANCH_VALUE; bb++) {
		auto iter = children.find(bb);
		if (iter != children.end()) {
			(*iter).second -> accumulate_keys(output);
		}
	}
}


TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, void>::type
TrieNode<TEMPLATE_PARAMS>::clear_rollback() {
	static_assert(x == METADATA_ROLLBACK, "no funny business");

	if (metadata.unsafe_load().num_rollback_subnodes == 0) { // ok bc root gets exclusive lock
		return;
	}
	metadata.num_rollback_subnodes = 0;

	for (auto iter = children.begin(); iter != children.end(); iter++) {
		(*iter).second -> clear_rollback();
	}
}

TEMPLATE_SIGNATURE
template<bool x>
typename std::enable_if<x, std::pair<bool, MetadataType>>::type
TrieNode<TEMPLATE_PARAMS>::do_rollback() {
	static_assert(x == METADATA_ROLLBACK, "no funny business");

	//no lock needed because MerkleTrie gets exclusive lock

	if (metadata.num_rollback_subnodes == 0) {
		TRIE_INFO("no subnodes, returning");
		return std::make_pair(false, MetadataType());
	}

	invalidate_hash();

	if (prefix_len == MAX_KEY_LEN_BITS && metadata.num_rollback_subnodes == 1) {
		return std::make_pair(true, -metadata.unsafe_load()); // safe bc exclusive lock
	}

	if (prefix_len == MAX_KEY_LEN_BITS && metadata.num_rollback_subnodes > 1) {
		throw std::runtime_error(
			"can't have num rollback subnodes > 1 at leaf");
	}

	auto metadata_delta = MetadataType();

	for (unsigned int branch_bits = 0; 
		branch_bits <= MAX_BRANCH_VALUE; 
		branch_bits++) 
	{
		TRIE_INFO("scanning branch bits %d", branch_bits);

		auto iter = children.find(branch_bits);
		if (iter == children.end()) {
			continue;
		}

		auto& child_ptr = (*iter).second;
		auto result = child_ptr->do_rollback();
		update_metadata(result.second);
		if (result.first) {
			TRIE_INFO("deleting subtree");
			children.erase(branch_bits);
		} else {
			if (child_ptr->single_child()) {
				TRIE_INFO(
					"contracting size 1 subtree, prefix len %d", prefix_len);
				auto replacement_child_ptr = child_ptr->get_single_child();
				children.emplace(
					(*iter).first, std::move(replacement_child_ptr));
			} 
		}
		metadata_delta += result.second;
	}
	TRIE_INFO("done scanning");
	return std::make_pair(children.empty(), metadata_delta);
}


#undef TEMPLATE_PARAMS
#undef TEMPLATE_SIGNATURE
}
