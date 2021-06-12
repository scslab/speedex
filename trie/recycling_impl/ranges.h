#pragma once

#include "trie/prefix.h"
#include "trie/recycling_impl/allocator.h"
#include "utils/threadlocal_cache.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace speedex {


template<typename TrieT>
class AccountHashRange {
	
	uint32_t num_children;
	using allocator_t = AccountTrieNodeAllocator<TrieT>;
	using ptr_t = TrieT::ptr_t;

	allocator_t& allocator;

public:
	std::vector<ptr_t> nodes;

	bool empty() const {
		return num_children == 0;
	}

	bool is_divisible() const {
		return num_children > 1000;
	}

	size_t num_nodes() const {
		return nodes.size();
	}

	TrieT& operator[](size_t idx) const {
		return allocator.get_object(nodes[idx]);
	}

	AccountHashRange(ptr_t node, allocator_t& allocator) 
		: num_children(0)
		, allocator(allocator)
		, nodes() {
			nodes.push_back(node);
			num_children = (allocator.get_object(node).size());
		};

	AccountHashRange(AccountHashRange& other, tbb::split) 
		: num_children(0)
		, allocator(other.allocator)
		, nodes() {
			auto original_sz = other.num_children;
			while (num_children < original_sz/2) {
				if (other.nodes.size() == 1) {
					other.nodes = allocator.get_object(other.nodes[0]).children_list();
				}
				if (other.nodes.size() == 0) {
					std::printf("other.nodes.size() = 0!");
					return;
				}

				nodes.push_back(other.nodes[0]);
				other.nodes.erase(other.nodes.begin());
				auto sz = allocator.get_object(nodes.back()).size();
				num_children += sz;
				other.num_children -= sz;
			}
	}
};


template<typename TrieT, unsigned int GRAIN_SIZE = 1000>
struct AccountAccumulateValuesRange {
	using ptr_t =  TrieT::ptr_t;
	using allocator_t =  TrieT::allocator_t;
	std::vector<ptr_t> work_list;

	uint64_t work_size;

	uint64_t vector_offset;

	const allocator_t& allocator;

	bool empty() const {
		return work_size == 0;
	}

	bool is_divisible() const {
		return work_size > GRAIN_SIZE;
	}

	AccountAccumulateValuesRange(ptr_t work_root, const allocator_t& allocator)
		: work_list()
		, work_size(allocator.get_object(work_root).size())
		, vector_offset(0)
		, allocator(allocator) {
			work_list.push_back(work_root);
		}

	AccountAccumulateValuesRange(AccountAccumulateValuesRange& other, tbb::split)
		: work_list()
		, work_size(0)
		, vector_offset(other.vector_offset)
		, allocator(other.allocator) {

			auto original_sz = other.work_size;
			while (work_size < original_sz/2) {
				if (other.work_list.size() == 1) {
					other.work_list = allocator.get_object(other.work_list.at(0)).children_list();
				}
				if (other.work_list.size() == 1) {
					std::printf("other.work_size = 1?!\n");
					throw std::runtime_error("shouldn't still have other.work_list.size() == 1");
				}

				if (other.work_list.size() == 0) {
					std::printf("other.work_size = 0?!\n");
					throw std::runtime_error("shouldn't get to other.work_list.size() == 0");
				}

				work_list.push_back(other.work_list[0]);

				//work_list.push_back(other.work_list[0]);
				other.work_list.erase(other.work_list.begin());
				
				auto sz = allocator.get_object(work_list.back()).size();
				//std::printf("stole %lu of %lu\n", sz, original_sz);
				work_size += sz;
				other.work_size -= sz;

				other.vector_offset += sz;
			}
	}
};

//Main difference with hash range is accounting for subnodes marked deleted.
//No nodes in work_list overlap, even after splitting
template<typename TrieT, unsigned int GRAIN_SIZE = 1000>
struct AccountApplyRange {
	using ptr_t = TrieT::ptr_t;
	using allocator_t =  TrieT::allocator_t;

	std::vector<ptr_t> work_list;

	uint64_t work_size;

	const allocator_t& allocator;

	bool empty() const {
		return work_size == 0;
	}

	bool is_divisible() const {
		return work_size > GRAIN_SIZE;
	}

	AccountApplyRange(ptr_t work_root, const allocator_t& allocator)
		: work_list()
		, work_size(0)
		, allocator(allocator) {
			work_size = allocator.get_object(work_root).size();
			work_list.push_back(work_root);
		}

	AccountApplyRange(AccountApplyRange& other, tbb::split)
		: work_list()
		, work_size(0) 
		, allocator(other.allocator) {

			auto original_sz = other.work_size;
			if (original_sz == 0) {
				return;
			}
			while (work_size < original_sz/2) {
				if (other.work_list.size() == 0) {
					std::printf("other work list shouldn't be zero!\n");
					throw std::runtime_error("get fucked this won't print");
				}
				if (other.work_list.size() == 1) {

					if (other.work_list.at(0) == UINT32_MAX) {
						throw std::runtime_error("found nullptr in ApplyRange!");
					}
					other.work_list = allocator.get_object(other.work_list.at(0)).children_list_nolock();
				} else {
					work_list.push_back(other.work_list.at(0));
					other.work_list.erase(other.work_list.begin());
				
					auto sz = allocator.get_object(work_list.back()).size();
					work_size += sz;
					other.work_size -= sz;
				}
			}
	}
};

template<typename ValueType>
class SerialAccountTrie;

template<typename TrieT>
struct AccountBatchMergeRange {
	using ptr_t = TrieT::ptr_t;
	using map_value_t = std::vector<ptr_t>;
	using allocator_t = AccountTrieNodeAllocator<TrieT>;
	using context_cache_t = ThreadlocalCache<SerialAccountTrie<typename TrieT::value_t>>;
	using prefix_t = TrieT::prefix_t;
	
	allocator_t& allocator;
	context_cache_t& cache;

	//maps from nodes of main trie to lists of new subtries that will get merged in at that node.
	//the new subtries are owned by this object.
	std::unordered_map<ptr_t, map_value_t> entry_points; // maps to unpropagated metadata
	//Whichever thread executes the contents of a map_value_t own the memory.
	//Tbb needs a copy constructor for unclear reasons, but seems to only execute a range once.

	//brances of the main trie that some other node has taken responsibility for.
	std::unordered_set<ptr_t> banned_branches;

	const ptr_t root;

	uint64_t num_children;


	bool empty() const {
		return entry_points.size() == 0;
	}

	bool is_divisible() const {
		return (num_children >= 100) && (entry_points.size() != 0);
	}

	std::shared_ptr<std::mutex> mtx;

	AccountBatchMergeRange (ptr_t root, std::vector<ptr_t> merge_in_list, allocator_t& allocator, context_cache_t& cache)
		: allocator(allocator)
		, cache(cache)
		, entry_points()
		, banned_branches()
		, root(root)
		, num_children(allocator.get_object(root).size())
		, mtx(std::make_shared<std::mutex>())
		{

			map_value_t value;
			for (auto& ptr : merge_in_list) {
				value.push_back(ptr);
			}

			entry_points.emplace(root, value);
		}

	AccountBatchMergeRange(AccountBatchMergeRange& other, tbb::split)
		: allocator(other.allocator)
		, cache(other.cache)
		, entry_points()
		, banned_branches(other.banned_branches)
		, root (other.root)
		, num_children(0)
		, mtx(other.mtx)
		 {
			if (other.entry_points.size() == 0) {
				std::printf("other entry pts is nonzero!\n");
				throw std::runtime_error("what the fuck");
			}

			auto original_sz = other.num_children;

			while (num_children < original_sz /2) {
				if (other.entry_points.size() > 1) {
					auto iter = other.entry_points.begin();
					auto& entry_pt = allocator.get_object((*iter).first);
					num_children += entry_pt.size();
					other.num_children -= entry_pt.size();

					entry_points.emplace((*iter).first, (*iter). second);
					other.entry_points.erase(iter);
				} else {
					break;
				}
			}


			//other.entry_points.size() == 1

			if (num_children < original_sz /2) {

				if (other.entry_points.size() == 0) {
					std::printf("invalid other.entry_points!\n");
					throw std::runtime_error("invalid other.entry_points!");
				}
				ptr_t theft_root = other.entry_points.begin()->first;

				std::vector<std::pair<uint8_t, ptr_t>> stealable_subnodes;

				//other entry_points is a single TrieT*.
				//We will iterate through the children of this node, stealing until we size is high enough.
				//A stolen child of theft_root (theft_candidate) becomes an entry point for us, and a banned subnode for them.
				// The TrieT*s in the map corresponding to theft_candidate are the children of the merge_in tries that correspond to theft_candidate.
				//Anything that matches theft_root + candidate branch bits can get merged in to theft_candidate.
				// I read through the TBB source, at least the points where deleting the copy ctor causes problems, and this seemed to be not an issue.
				// In fact, in two of the three spots, there's "TODO: std::move?"

				map_value_t merge_in_tries = other.entry_points.begin() -> second;
				//std::printf("tries at entry point in other: %lu\n", other.entry_points.begin()->second.size());


	//			prefix_t steal_prefix{0};

				while (num_children + 10 < other.num_children) { // + 10 for rounding error
					

					if (stealable_subnodes.size() > 1) {
						auto& theft_root_ref = allocator.get_object(theft_root);
						auto lk = theft_root_ref.lock();

						auto [stolen_bb, theft_candidate] = stealable_subnodes.back();
						stealable_subnodes.pop_back();
						//do steal
						other.banned_branches.insert(theft_candidate);
						std::vector<ptr_t> stolen_merge_in_tries;
						prefix_t steal_prefix = theft_root_ref.get_prefix();

						for (auto iter = merge_in_tries.begin(); iter != merge_in_tries.end(); iter++) {

							if (theft_root_ref.is_leaf()) {
								break;
							}
		
							steal_prefix.set_next_branch_bits(theft_root_ref.get_prefix_len(), stolen_bb);
	
							auto [do_steal_entire_subtree, metadata_delta, theft_result] 
								= (allocator.get_object(*iter).destructive_steal_child(
										steal_prefix, 
										theft_root_ref.get_prefix_len() + TrieT::BRANCH_BITS, 
										cache.get(allocator).get_allocation_context()));
							if (theft_result.non_null()) {
								
								stolen_merge_in_tries.emplace_back(theft_result.get());
							} else if (do_steal_entire_subtree) {
								//std::printf("asked to steal entire subtree!\n");
								//throw std::runtime_error("should not happen if stealing from theft_root");

							}
						}
						auto& theft_candidate_ref = allocator.get_object(theft_candidate);
						if (stolen_merge_in_tries.size() != 0) {
							//std::printf("stole %lu tries\n", stolen_merge_in_tries.size());
							entry_points.emplace(theft_candidate, std::move(stolen_merge_in_tries));
							num_children += theft_candidate_ref.size();
						} else {
							//std::printf("nothing stolen #sad so continuing, losing %lu size\n", theft_candidate -> size());
						}

						other.num_children -= theft_candidate_ref.size();
					} else {
						if (stealable_subnodes.size() != 0) {
							theft_root = stealable_subnodes.back().second;
							stealable_subnodes.pop_back();
						}

						auto& theft_root_ref = allocator.get_object(theft_root);

						auto lk = theft_root_ref.lock();

						stealable_subnodes = theft_root_ref.children_list_with_branch_bits_nolock();
						if (theft_root_ref.is_leaf()) {
							return;
						}
						//filter stealable
						for (std::size_t i = 0; i < stealable_subnodes.size();) {
							auto iter = other.banned_branches.find(stealable_subnodes[i].second);
							if (iter != other.banned_branches.end()) {
								stealable_subnodes.erase(stealable_subnodes.begin() + i);
							} else {
								i++;
							}
						}

						if (stealable_subnodes.size() == 0) {
							std::printf("NO STEALABLE NODES\n");
							throw std::runtime_error("we could just return, but throwing error for now to be safe");
						}
					}
				}
			}
		}

		template<typename MergeFn>
		void execute() {
			for (auto iter = entry_points.begin(); iter != entry_points.end(); iter++) {
				int32_t sz_delta = 0;
				ptr_t entry_pt = iter -> first;
				for (ptr_t node : iter -> second) {

					if (allocator.get_object(node).size() > 0) {
						
						sz_delta += allocator.get_object(entry_pt).template merge_in<MergeFn>(node, cache.get(allocator).get_allocation_context());
					}
				}

				auto* entry_pt_realptr = &(allocator.get_object(entry_pt));

				allocator.get_object(root).propagate_sz_delta(entry_pt_realptr, sz_delta, allocator);
			}
		}
};

template<typename MergeFn>
struct AccountBatchMergeReduction {

	template<typename TrieT>
	void operator()(AccountBatchMergeRange<TrieT>& range) {
		range.template execute<MergeFn>();
	}

	AccountBatchMergeReduction() {}

	AccountBatchMergeReduction(AccountBatchMergeReduction& other, tbb::split) {}

	void join(AccountBatchMergeReduction& other) {}
};

} /* speedex */