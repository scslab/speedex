#pragma once

#include "config.h"

#include "hotstuff/block.h"

#include "lmdb/lmdb_types.h"
#include "lmdb/lmdb_wrapper.h"

#include "xdr/types.h"

namespace hotstuff {

class HotstuffLMDB : public speedex::LMDBInstance {

	constexpr static auto DB_NAME = "hotstuff";

	void open_env() {
		LMDBInstance::open_env(
			std::string(ROOT_DB_DIRECTORY) + std::string(HOTSTUFF_INDEX));
	}

	void add_decided_block_(block_ptr_t blk, std::vector<uint8_t> const& serialized_vm_blk_id);

	std::optional<std::pair<speedex::Hash, std::vector<uint8_t>>>
	get_decided_hash_id_pair_(uint64_t height) const;

public:

	void create_db() {
		LMDBInstance::create_db(DB_NAME);
	}

	void open_db() {
		LMDBInstance::open_db(DB_NAME);
	}

	using LMDBInstance::sync;

	HotstuffLMDB() : LMDBInstance() { open_env(); }

	template<typename vm_block_id>
	void add_decided_block(block_ptr_t blk, vm_block_id const& id);

	template<typename vm_block_id>
	std::optional<std::pair<speedex::Hash, vm_block_id>>
	get_decided_hash_id_pair(uint64_t height) const;


	class cursor {

		speedex::dbenv::txn rtx;
		speedex::dbenv::cursor c;

		friend class HotstuffLMDB;
		cursor(HotstuffLMDB const& lmdb)
			: rtx(lmdb.rbegin())
			, c(rtx.cursor_open(lmdb.get_data_dbi()))
			{}

	public:
	
		struct iterator {
			speedex::dbenv::cursor::iterator it;

			using kv_t = std::pair<uint64_t, speedex::Hash>;

			iterator(speedex::dbenv::cursor& c) : it(c) {}
			constexpr iterator() : it(nullptr) {}

			kv_t operator*();

			iterator& operator++() { ++it; return *this; }

			friend bool operator==(const iterator &a, const iterator &b) {
				return a.it == b.it;   // only works for forward iteration.
			}
			friend bool operator!=(const iterator &a, const iterator &b) = default;

	
		
		};

		iterator begin() {
			return iterator(c);
		}

		constexpr iterator end() {
			return iterator();
		}
	};

	cursor forward_cursor() {
		return cursor{*this};
	}
};

template<typename vm_block_id>
void 
HotstuffLMDB::add_decided_block(block_ptr_t blk, vm_block_id const& id)
{
	add_decided_block_(blk, id.serialize());
}

template<typename vm_block_id>
std::optional<std::pair<speedex::Hash, vm_block_id>>
HotstuffLMDB::get_decided_hash_id_pair(uint64_t height) const {
	auto out = get_decided_hash_id_pair_(height);
	if (!out) {
		return out;
	}
	return {out -> first, vm_block_id(out -> second)};
}

} /* hotstuff */
