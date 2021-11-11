#include "block_header_hash_map.h"

#include "utils/big_endian.h"

namespace speedex {

using xdr::operator==;

void 
BlockHeaderHashMapLMDB::open_env() {
	LMDBInstance::open_env(
		std::string(ROOT_DB_DIRECTORY) + std::string(HEADER_HASH_DB),
		DEFAULT_LMDB_FLAGS | MDB_NOLOCK); // need NOLOCK for commit/reload tests
}

void BlockHeaderHashMap::insert(uint64_t block_number, const Hash& block_hash) {

	if (block_number == 0) {
		throw std::runtime_error("should never insert genesis hash!");
/*
		if (last_committed_block_number != 0 || block_map.size() != 0) {
			throw std::runtime_error(
				"can't insert prev block 0 if we already have elements in block hash map");
		}
		Hash zero_hash;
		zero_hash.fill(0);
		if (zero_hash != block_hash) {
		//if (memcmp(block_hash.data(), zero_hash.data(), zero_hash.size()) != 0) {
			throw std::runtime_error("can't have genesis block with nonzero hash");
		}
		//prev block is genesis block, do nothing
		return;*/
	}

	if (block_number != last_committed_block_number + 1) {
		throw std::runtime_error("inserting wrong block number");
	}

	prefix_t key_buf;

	write_unsigned_big_endian(key_buf, block_number);

	block_map.insert(key_buf, HashWrapper(block_hash));
	
	// Difference between production and validation is here.
	last_committed_block_number = block_number;
}

void BlockHeaderHashMap::persist_lmdb(uint64_t current_block_number) {
	BLOCK_INFO("persisting header hash map at round %lu", current_block_number);

	if (!lmdb_instance) {
		return;
	}
	uint64_t persisted_round_number 
		= lmdb_instance.get_persisted_round_number();

	auto wtx = lmdb_instance.wbegin();

	//changed: we commit current_block_number because we've already inserted it.
	for (uint64_t i = persisted_round_number; i <= current_block_number; i++) { 
		if (i == 0) continue;
		TrieT::prefix_t round_buf;

		write_unsigned_big_endian(round_buf, i);
		auto round_bytes = round_buf.get_bytes_array();
		dbval key{round_bytes};

		// querying for round i
		auto hash_opt = block_map.get_value(round_buf);
		if (!hash_opt) {
			throw std::runtime_error("did not find hash in hash_map!");
		}

		dbval hash_val{*hash_opt};
		wtx.put(lmdb_instance.get_data_dbi(), key, hash_val);
	}

	lmdb_instance.commit_wtxn(wtx, current_block_number);
}

// LMDB committed to round X contains entries 1 through X.
// To sync back with LMDB, we need to remove all entries X+1 and higher.

void 
BlockHeaderHashMap::rollback_to_committed_round(uint64_t committed_block_number)
{
	if (committed_block_number < lmdb_instance.get_persisted_round_number()) {
		throw std::runtime_error("can't rollback beyond lmdb persist");
	}
	for (uint64_t i = committed_block_number + 1; i <= last_committed_block_number; i++) {
		if (i == 0) continue;
		TrieT::prefix_t round_buf;
		write_unsigned_big_endian(round_buf, i);

		if (!block_map.perform_deletion(round_buf)) {
			throw std::runtime_error("error when deleting from header hash map");
		}
	}
	last_committed_block_number = committed_block_number;//(committed_block_number == 0) ? 0 : committed_block_number - 1;
}

/*

bool 
BlockHeaderHashMap::tentative_insert_for_validation(
	uint64_t block_number, const Hash& block_hash) 
{
	if (block_number == 0) {
		if (last_committed_block_number != 0 || block_map.size() != 0) {
			throw std::runtime_error(
				"can't insert prev block 0 if we already have elements in block hash map");
		}
		Hash zero_hash;
		zero_hash.fill(0);
		if (memcmp(
				block_hash.data(), 
				zero_hash.data(), 
				zero_hash.size()) != 0) {
			throw std::runtime_error(
				"can't have genesis block with nonzero hash");
		}
		//validation prev block is genesis block, do nothing
		return true;
	}



	//input block number corresponds to previous block.
	if (block_number != last_committed_block_number) {
		return false;
	}

	prefix_t key_buf;

	write_unsigned_big_endian(key_buf, block_number);

	block_map.insert(key_buf, HashWrapper(block_hash));

	return true;
}
void BlockHeaderHashMap::rollback_validation() {
	
	prefix_t key_buf;
	write_unsigned_big_endian(key_buf, last_committed_block_number + 1);

	block_map.perform_deletion(key_buf);
}
void BlockHeaderHashMap::finalize_validation(uint64_t finalized_block_number) {
	if (finalized_block_number < last_committed_block_number) {
		throw std::runtime_error("can't finalize prior block");
	}
	last_committed_block_number = finalized_block_number;
} */

void BlockHeaderHashMap::load_lmdb_contents_to_memory() {
	auto rtx = lmdb_instance.rbegin();

	auto cursor = rtx.cursor_open(lmdb_instance.get_data_dbi());

	for (auto kv : cursor) {
		auto bytes = kv.first.bytes();
		uint64_t round_number;

		read_unsigned_big_endian(bytes.data(), round_number);


		if (round_number > lmdb_instance.get_persisted_round_number()) {

			std::printf(
				"round number: %lu persisted_round_number: %lu\n", 
				round_number, 
				lmdb_instance.get_persisted_round_number());
			std::fflush(stdout);
			throw std::runtime_error(
				"lmdb contains round idx beyond committed max");
		}

		prefix_t round_buf;
		write_unsigned_big_endian(round_buf, round_number);

		auto value = HashWrapper();
		if (kv.second.mv_size != 32) {
			throw std::runtime_error("invalid value size");
		}
		memcpy(value.data(), kv.second.mv_data, 32);
		block_map.insert(round_buf, value);
	}
	last_committed_block_number = lmdb_instance.get_persisted_round_number();
	rtx.commit();
}

std::optional<Hash>
BlockHeaderHashMap::get_hash(uint64_t round_number) const {
	if (round_number > get_persisted_round_number()) {
		return std::nullopt;
	}

	prefix_t round_buf;
	write_unsigned_big_endian(round_buf, round_number);
	auto hash_opt = block_map.get_value(round_buf);

	if (!hash_opt) {
		throw std::runtime_error("failed to load hash that lmdb should have");
	}

	return hash_opt;
}

} /* speedex */
