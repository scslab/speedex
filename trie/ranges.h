#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

/*! \file ranges.h 
A collection of TBB Range objects, useful for 
iterating over tries for different purposes.
*/

namespace speedex {

//! Range for hashing a trie.
//! Does not actually hash entire trie, when used in TBB.  Hashes a disjoint
//! set of subtries which cover the set of values.
//! Call hash on root node after hashing with this range object.
template<typename TrieT>
class HashRange {
	
	uint64_t num_children;
public:
	//! Subtries for which this range is responsible
	std::vector<TrieT*> nodes;

	//! For TBB: Is this range worth executing?
	bool empty() const {
		return num_children == 0;
	}

	//! For TBB: Is this range splittable?
	bool is_divisible() const {
		return num_children > 1000;
	}

	size_t num_nodes() const {
		return nodes.size();
	}

	TrieT* operator[](size_t idx) const {
		return nodes.at(idx);
	}

	HashRange(std::unique_ptr<TrieT>& node) 
		: num_children(0)
		, nodes() {
			nodes.push_back(node.get());
			num_children = (node->size()) - (node -> num_deleted_subnodes());
		};

	//! For TBB: splitting constructor.
	//! Attempts to split range in half, but does not enforce exact split.
	//! Ignores deleted subnodes for this accounting.
	HashRange(HashRange& other, tbb::split) 
		: num_children(0)
		, nodes() {
			auto original_sz = other.num_children;
			while (num_children < original_sz/2) {
				if (other.nodes.size() == 1) {
					other.nodes = other.nodes.at(0)->children_list();
				}
				if (other.nodes.size() == 0) {
					std::printf("other.nodes.size() = 0!");
					return;
				}

				nodes.push_back(other.nodes.at(0));
				other.nodes.erase(other.nodes.begin());
				auto sz = nodes.back()->size() 
							- nodes.back()->num_deleted_subnodes();
				num_children += sz;
				other.num_children -= sz;
			}

	}
};


//! Range for applying a function to all values in a trie.
template<typename TrieT, unsigned int GRAIN_SIZE = 1000>
struct ApplyRange {
	std::vector<TrieT*> work_list;

	uint64_t work_size;

	//! For TBB: Is this range worth executing?
	bool empty() const {
		return work_size == 0;
	}

	//! For TBB: Is this range splittable?
	bool is_divisible() const {
		return work_size > GRAIN_SIZE;
	}

	ApplyRange(const std::unique_ptr<TrieT>& work_root)
		: work_list()
		, work_size(work_root -> size()) {
			work_list.push_back(work_root.get());
		}

	//! For TBB: splitting constructor.
	//! Attempts to split range in half, but does not enforce exact split.
	//! Main difference with HashRange is that this does not ignore
	//! deleted subnodes when doing the split accounting.
	ApplyRange(ApplyRange& other, tbb::split)
		: work_list()
		, work_size(0) {

			auto original_sz = other.work_size;
			if (original_sz == 0) {
				return;
			}
			while (work_size < original_sz/2) {
				if (other.work_list.size() == 0) {
					throw std::runtime_error(
						"other worklist should not be empty");
				}
				if (other.work_list.size() == 1) {

					if (other.work_list.at(0) == nullptr) {
						throw std::runtime_error(
							"found nullptr in ApplyRange!");
					}
					other.work_list = other.work_list.at(0)->children_list();
				} else {

					work_list.push_back(other.work_list.at(0));
					other.work_list.erase(other.work_list.begin());
				
					auto sz = work_list.back()->size();
					work_size += sz;
					other.work_size -= sz;
				}
			}
	}
};

//! Range for accumulating all the values stored in a trie.
template<typename TrieT, unsigned int GRAIN_SIZE = 1000>
struct AccumulateValuesRange {
	//! List of trie nodes for which this object is responsible.
	std::vector<TrieT*> work_list;

	//! Number of values for which this object is responsible.
	uint64_t work_size;

	//! Location at a thread that executes this range
	//! should begin placing accumulated values
	uint64_t vector_offset;

	//! For TBB: Is this range worth executing?
	bool empty() const {
		return work_size == 0;
	}

	//! For TBB: Can this range be divided further?
	bool is_divisible() const {
		return work_size > GRAIN_SIZE;
	}

	AccumulateValuesRange(const std::unique_ptr<TrieT>& work_root)
		: work_list()
		, work_size(work_root -> size())
		, vector_offset(0) {
			work_list.push_back(work_root.get());
		}

	//! Split range into two pieces
	AccumulateValuesRange(AccumulateValuesRange& other, tbb::split)
		: work_list()
		, work_size(0)
		, vector_offset(other.vector_offset) {


			auto original_sz = other.work_size;
			while (work_size < original_sz/2) {
				if (other.work_list.size() == 1) {
					other.work_list = other.work_list.at(0)
						->children_list_ordered();
				}
				if (other.work_list.size() == 1) {
					throw std::runtime_error(
						"should not still have other.work_list.size() == 1");
				}

				if (other.work_list.size() == 0) {
					throw std::runtime_error(
						"shouldn't get to other.work_list.size() == 0");
				}

				work_list.push_back(other.work_list[0]);

				other.work_list.erase(other.work_list.begin());
				
				auto sz = work_list.back()->size();
				work_size += sz;

				other.work_size -= sz;
				other.vector_offset += sz;
			}
	}
};


/*! TBB Range for merging in a batch of tries.

When merging in a trie, no trie node has its prefix length increased.

Suppose a node X is merged into node Y.
We constructed merge_in so that X's prefix is always an extension of
Y's parent's prefix.

This method relies on "jumping ahead" and merging in subtries to non-root
nodes of a main trie, so we need to maintain certain prefix invariants in order
to ensure the trie is built as though it had been built by sequential
merge_in calls from the root.

This invariant is that X's prefix strictly extends Y's  parent's prefix,
and the extension agrees with Y.  In other words, merging X and Y cannot 
result in a root tree whose prefix is the same length as Y's parent's prefix.

The splitting is the tricky part of this class.
A split "steals responsibility" for a certain set of nodes on the main trie
(to which everything is merged into).  All subtries of the batch of tries
that are being merged in are then stolen by this new range.  When the range
is executed, the stolen tries are merged in starting at the nodes of the main
trie that this range took responsibility for.
*/
template<typename TrieT, typename MetadataType>
struct BatchMergeRange {
	using map_value_t = std::vector<TrieT*>;

	static_assert(TrieT::LOCKABLE, 
		"can't batch merge tries that don't have per-node locks");

	//! Maps from nodes of main trie to lists of new subtries that will get
	//! merged in at that node.
	//! The new subtries are owned by this object.
	std::unordered_map<TrieT*, map_value_t> entry_points;
	//Whichever thread executes the contents of a map_value_t own the memory.
	//Tbb needs a copy constructor for unclear reasons, 
	//but seems to only execute a range once.

	//! Branches of the main trie that some other node has taken
	//! responsibility for.
	std::unordered_set<TrieT*> banned_branches;

	using exclusive_lock_t = typename TrieT::exclusive_lock_t;
	using prefix_t = typename TrieT::prefix_t;


	TrieT * const root;

	//! Number of main trie children for which this range has responsibility.
	uint64_t num_children;

	//! For TBB: Is this range worth executing
	bool empty() const {
		return entry_points.size() == 0;
	}

	//! For TBB: Can this range be split?
	bool is_divisible() const {
		return (num_children >= 100) && (entry_points.size() != 0);
	}

	BatchMergeRange (TrieT* root, std::vector<std::unique_ptr<TrieT>>&& list)
		: entry_points()
		, banned_branches()
		, root(root)
		, num_children(root -> size())
		{

			map_value_t value;
			for (auto& ptr : list) {
				value.push_back(ptr.release());
			}

			entry_points.emplace(root, value);
		}

	//! TBB Splitting constructor
	BatchMergeRange(BatchMergeRange& other, tbb::split)
		: entry_points()
		, banned_branches(other.banned_branches)
		, root (other.root)
		, num_children(0)

	{

		if (other.entry_points.size() == 0) {
			throw std::runtime_error("other entry pts is nonzero!");
		}

		auto original_sz = other.num_children;

		// First things stolen are whole entry points.
		while (num_children < original_sz /2) {
			if (other.entry_points.size() > 1) {
				auto iter = other.entry_points.begin();
				num_children += (*iter).first -> size();
				other.num_children -= (*iter).first -> size();

				entry_points.emplace((*iter).first, (*iter). second);
				other.entry_points.erase(iter);
			} else {

				break;
			}
		}

		// If stealing whole entry points was not enough, start to steal
		// new entry points
		// At this point, the other range is only responsible for one 
		// entry point.
		if (num_children < original_sz /2) {

			if (other.entry_points.size() == 0) {
				std::printf("invalid other.entry_points!\n");
				throw std::runtime_error("invalid other.entry_points!");
			}

			// A node from which we will derive candidate entry points.
			TrieT* theft_root = other.entry_points.begin()->first;

			// Pairs of (branch bits, child nodes) which can be stolen.
			// Always the immediate children of theft_root.
			std::vector<std::pair<uint8_t, TrieT*>> stealable_subnodes;

			/*
			We will iterate through the children of this node, 
			stealing until the size is high enough.
			
			A stolen child of theft_root (theft_candidate) becomes an 
			entry point for us, and a banned subnode for them.
			 The TrieT*s in the map corresponding to theft_candidate 
			 are the children of the merge_in tries that 
			 correspond to theft_candidate.
			Anything that matches theft_root + candidate branch bits
			(i.e. anything that extends candidate's )
			 can get merged in to theft_candidate.
			 I read through the TBB source, at least the points where 
			 deleting the copy ctor causes problems, and
			  this seemed to be not an issue.
			 In fact, in two of the three spots, there's "TODO: std::move?"
			
			*/
			map_value_t merge_in_tries 
				= other.entry_points.begin() -> second;


			prefix_t steal_prefix;

				// + 10 for rounding error.  Accuracy is not very important here
			while (num_children + 10 < other.num_children) {
				

				if (stealable_subnodes.size() > 1) {

					// Lock theft_root so that theft_root is not modified
					// while we work
					auto lock = theft_root
						->get_lock_ref().template lock<exclusive_lock_t>();

					auto [stolen_bb, theft_candidate] 
						= stealable_subnodes.back();
					stealable_subnodes.pop_back();
					//do steal
					other.banned_branches.insert(theft_candidate);
					std::vector<TrieT*> stolen_merge_in_tries;
					steal_prefix = theft_root->get_prefix();

					for (auto iter = merge_in_tries.begin(); 
						iter != merge_in_tries.end(); 
						iter++) {

						if (theft_root -> is_leaf()) {
							break;
						}
						/* Any trie node that matches theft_root's prefix
						   PLUS the branch bits used to start a child's 
						   prefix can be merged into said child.
						   Said child's prefix won't get shorter than 
						   this length while we have a lock on it's parent.
						*/
						steal_prefix.set_next_branch_bits(
							theft_root -> get_prefix_len(), stolen_bb);
						
						/* Attempt to steal steal_prefix from *iter
						*/
						auto [do_steal_entire_subtree
							, metadata_delta
							, theft_result] 
							= ((*iter)->destructive_steal_child(
								steal_prefix, 
								theft_root -> get_prefix_len() 
									+ TrieT::BRANCH_BITS_EXPORT));

						if (theft_result) {
							stolen_merge_in_tries.emplace_back(
								theft_result.release());
						} else if (do_steal_entire_subtree) {
							//We could steal the entire subtree here
							//TODO verify that stealing entire subtree is
							// safe
							// TODO need to duplicate some code here, if 
							// we do the stealing.  Particularly the 
							// metadata updates.
						}
					}
					if (stolen_merge_in_tries.size() != 0) {

						entry_points.emplace(
							theft_candidate, 
							std::move(stolen_merge_in_tries));
						num_children += theft_candidate-> size();
					} else {
					}



					other.num_children -= theft_candidate->size();
				} else {
					if (stealable_subnodes.size() != 0) {
						// stealable_subnodes.size() == 1
						// Take this remaining stealable subnode as the
						// next theft_root.
						theft_root = stealable_subnodes.back().second;
						stealable_subnodes.pop_back();
					}

					auto lock = theft_root
						->get_lock_ref().template lock<exclusive_lock_t>();

					if (theft_root -> is_leaf()) {
						return;
					}

					stealable_subnodes = theft_root 
						-> children_list_with_branch_bits();
					
					// Filter out subnodes that were banned
					for (size_t i = 0; i < stealable_subnodes.size();) {
						auto iter = other.banned_branches.find(
							stealable_subnodes[i].second);
						if (iter != other.banned_branches.end()) {
							stealable_subnodes.erase(
								stealable_subnodes.begin() + i);
						} else {
							i++;
						}
					}

					if (stealable_subnodes.size() == 0) {
						// Could just return here
						throw std::runtime_error(
							"tried to steal from completely banned node");
					}
				}
			}
		}
	}

	//! Does all the work of merging in tries for which this range
	//! is responsible.
	template<typename MergeFn>
	void execute() {

		for (auto iter = entry_points.begin(); 
			iter != entry_points.end(); 
			iter++) {

			auto metadata = MetadataType{};

			TrieT* entry_pt = iter -> first;
			for (TrieT* node : iter -> second) {

				/* No race conditions here because ranges cannot
				   be split and executed at the same time.
				   Furthermore, ranges are only executed once by TBB.
				   TBB's documentation is not terribly clear.
				   */
				std::unique_ptr<TrieT> ptr = std::unique_ptr<TrieT>(node);
				if (ptr->size() > 0) {
					metadata += entry_pt
						->template _merge_in<MergeFn>(std::move(ptr));
				}

			}

			root -> propagate_metadata(entry_pt, metadata);
			
		}
	}
};

//! Runs a BatchMerge as a parallel_reduce
//! Could also run as a parallel_for, not a particular advantage either way.
template<typename MergeFn>
struct BatchMergeReduction {

	template<typename TrieT, typename MetadataType>
	void operator()(BatchMergeRange<TrieT, MetadataType>& range) {
		range.template execute<MergeFn>();
	}

	BatchMergeReduction() {}

	BatchMergeReduction(BatchMergeReduction& other, tbb::split) {}

	void join(BatchMergeReduction& other) {}
};


} /* speedex */