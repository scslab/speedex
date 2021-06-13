#include "orderbook/lmdb.h"

#include "utils/price.h"

namespace speedex {


ThunkGarbage
__attribute__((warn_unused_result)) 
OrderbookLMDB::write_thunks(const uint64_t current_block_number, bool debug) {

	dbenv::wtxn wtx = wbegin();

	std::vector<thunk_t> relevant_thunks;
	{
		std::lock_guard lock(*mtx);
		for (size_t i = 0; i < thunks.size();) {
			auto& thunk = thunks.at(i);
			if (thunk.current_block_number <= current_block_number) {
				relevant_thunks.emplace_back(std::move(thunk));
				thunks.at(i).reset_trie();
				thunks.erase(thunks.begin() + i);
			} else {
				i++;
			}

		}
	}
	//In one round, everything below partial_exec_key is deleted (cleared).
	//So first, we find the maximum partial_exec_key, and delete everything BELOW that.

	//get maximum key, remove offers
	// iterate from top to bot, adding only successful offers and rolling key downwards

	prefix_t key_buf;


	bool key_set = false;

	if (relevant_thunks.size() == 0 && get_persisted_round_number() != current_block_number) {
		throw std::runtime_error("can't persist without thunks");
	}

	if (relevant_thunks.size() == 0) {
		return;
	}

	if (relevant_thunks[0].current_block_number != get_persisted_round_number() + 1) {
		throw std::runtime_error("invalid current_block_number");
	}


	//compute maximum key

	bool print = false;

	if (print)
		std::printf("phase 1\n");

	for (size_t i = 0; i < relevant_thunks.size(); i++) {
		auto& thunk = relevant_thunks[i];
		if (thunk.current_block_number > current_block_number) {
			throw std::runtime_error("impossible");
			//inflight thunk that's not done yet, or for some reason we're not committing that far out yet.
			continue;
		}
		if (print)
			std::printf("phase 1(max key) thunk i=%lu %lu\n", i,  thunk.current_block_number);

		//remove deleted keys
		for (auto& delete_kv : thunk.deleted_keys.deleted_keys) {
			auto& delete_key = delete_kv.first;
			auto bytes = delete_key.get_bytes_array();
			dbval key = dbval{bytes};
			wtx.del(dbi, key);
		}

		if (thunk.get_exists_partial_exec()) {
			key_set = true;
			if (key_buf < thunk.partial_exec_key) {
			//auto res = memcmp(key_buf, thunk.partial_exec_key.data(), MerkleWorkUnit::WORKUNIT_KEY_LEN);
			//if (res < 0) {
				key_buf = thunk.partial_exec_key;
//				memcpy(key_buf, thunk.partial_exec_key.data(), MerkleWorkUnit::WORKUNIT_KEY_LEN);
			}
			INTEGRITY_CHECK("thunk threshold key: %s", DebugUtils::__array_to_str(thunk.partial_exec_key, MerkleWorkUnit::WORKUNIT_KEY_LEN).c_str());

		}
	}

	INTEGRITY_CHECK("final max key: %s", DebugUtils::__array_to_str(key_buf, MerkleWorkUnit::WORKUNIT_KEY_LEN).c_str());

	//auto& wtx = wtxn();
	
	if (print)
		std::printf("phase 2\n");

	auto cursor = wtx.cursor_open(dbi);

	//auto begin_cursor = wtx.cursor_open(dbi).begin();

	auto key_buf_bytes = key_buf.get_bytes_array();

	dbval key = dbval{key_buf_bytes};


	//MerkleTrieT::prefix_t key_backup = key_buf;
	//unsigned char key_backup[MerkleWorkUnit::WORKUNIT_KEY_LEN];
	//memcpy(key_backup, key_buf, MerkleWorkUnit::WORKUNIT_KEY_LEN);

	cursor.get(MDB_SET_RANGE, key); 

	//get returns the least key geq  key_buf.
	//So after one --, we get the greatest key < key_buf.
	// find greatest key leq key_buf;
	
	int num_deleted = 0;

	if ((!key_set) || (!cursor)) {
		INTEGRITY_CHECK("setting cursor to last, key_set = %d", key_set);
		cursor.get(MDB_LAST);
		//If the get operation fails, then there isn't a least key greater than key_buf.
		//if key was not set by any thunk, then all thunks left db in fully cleared state.
		// so we have to delete everything on disk.

		//So we must delete the entire database.


	} else {
		--cursor;
	}

	while (cursor) {
		cursor.del();
		--cursor;
		num_deleted++;
	}

	//while (cursor != begin_cursor) {
	//	--cursor;
	//	cursor.del();

	//	num_deleted++;
	//}
	INTEGRITY_CHECK_F(
		if (num_deleted > 0) {
			INTEGRITY_CHECK("num deleted is %u", num_deleted);
		}
	);

	//std::printf("num deleted first pass = %d\n", num_deleted);

	key_buf.clear();
//	memset(key_buf, 0, MerkleWorkUnit::WORKUNIT_KEY_LEN);


	if (print) 
		std::printf("phase 3\n");
	for (int i = relevant_thunks.size() - 1; i>= 0; i--) {

		if (relevant_thunks.at(i).current_block_number > current_block_number) {
			// again ignore inflight thunks
			continue;
		}

		if (print)
			std::printf("phase 3 i %d %lu\n", i, relevant_thunks[i].current_block_number);
		//std::printf("processing thunk %u\n", i);
		//auto res = memcmp(key_buf, relevant_thunks[i].partial_exec_key.data(), MerkleWorkUnit::WORKUNIT_KEY_LEN);
		//if (res < 0) {
		if (key_buf < relevant_thunks[i].partial_exec_key) {
			key_buf = relevant_thunks[i].partial_exec_key;
		
		}

		Price min_exec_price = price::read_price_big_endian(key_buf);

		prefix_t offer_key_buf;

		if (print)
			std::printf("thunks[i].uncommitted_offers_vec.size() %lu , current_block_number %lu\n",
			relevant_thunks[i].uncommitted_offers_vec.size(), relevant_thunks[i].current_block_number);
		
		for (auto idx = relevant_thunks[i].uncommitted_offers_vec.size(); idx > 0; idx--) { // can't test an unsigned for >=0 meaningfully

			auto& cur_offer = relevant_thunks[i].uncommitted_offers_vec[idx-1]; //hence idx -1
			if (cur_offer.amount == 0) {
				std::printf("cur_offer.owner %lu id %lu cur_offer.amount %lu cur_offer.minPrice %lu\n", cur_offer.owner, cur_offer.offerId, cur_offer.amount, cur_offer.minPrice);
				throw std::runtime_error("tried to persist an amount 0 offer!");
			}
			
			if (cur_offer.minPrice >= min_exec_price) {
				generate_orderbook_trie_key(cur_offer, offer_key_buf);
				bool db_put = false;
				if (cur_offer.minPrice > min_exec_price) {
					db_put = true;
				} else {

					if (offer_key_buf >= key_buf) {
						db_put = true;
					}
				}
				if (db_put) {

					auto offer_key_buf_bytes = offer_key_buf.get_bytes_array();
					dbval db_key = dbval{offer_key_buf_bytes};

					auto value_buf = xdr::xdr_to_opaque(cur_offer);
					dbval value = dbval{value_buf.data(), value_buf.size()};
					wtx.put(dbi, db_key, value);
				} else {
					break;
				}
			} else {
				break;
			}
		}
		if (print)
			std::printf("done phase 3 loop\n");
	}

	//insert the partial exec offers

	if (print)
		std::printf("phase 4\n");

	for (size_t i = 0; i < relevant_thunks.size(); i++) {

		if (relevant_thunks.at(i).current_block_number > current_block_number) {
			// again ignore inflight thunks
			throw std::runtime_error("impossible");
			continue;
		}

		if (print)
			std::printf("phase 4 i %lu %lu\n", i, relevant_thunks[i].current_block_number);

		//thunks[i].uncommitted_offers.apply_geq_key(func, key_buf);

		INTEGRITY_CHECK("thunks[i].uncommitted_offers.size() = %d", relevant_thunks[i].uncommitted_offers.size());

		INTEGRITY_CHECK("num values put = %d", func.num_values_put);


		if (!relevant_thunks[i].get_exists_partial_exec()) {
			INTEGRITY_CHECK("no partial exec, continuing to next thunk");
			continue;
		}


		auto partial_exec_key_bytes = relevant_thunks[i].partial_exec_key.get_bytes_array();
		dbval partial_exec_key{partial_exec_key_bytes};//(relevant_thunks[i].partial_exec_key.data(), MerkleWorkUnit::WORKUNIT_KEY_LEN);

		auto get_res = wtx.get(dbi, partial_exec_key);

		if (!get_res) {
			INTEGRITY_CHECK("didn't find partial exec key because of preemptive clearing");
			continue;
			//throw std::runtime_error("did not find offer that should be in lmdb");
		}

		//offer in memory
		Offer partial_exec_offer;// = thunks[i].preexecute_partial_exec_offer;

		//if (get_res) {
			//std::printf("partial exec of preexisting offer\n");
			//use offer on disk instead, if it exists.
			//This lets us process partial executions in backwards order.
		dbval_to_xdr(*get_res, partial_exec_offer);
		//}

		//Offer partial_exec_offer = thunks[i].preexecute_partial_exec_offer;

		if ((uint64_t)relevant_thunks[i].partial_exec_amount > partial_exec_offer.amount) {
			throw std::runtime_error("can't have partial exec offer.amount < thunk[i].partial_exec_amount");
		}

		partial_exec_offer.amount -= relevant_thunks[i].partial_exec_amount;


		if (relevant_thunks[i].partial_exec_amount < 0) {
			//allowed to be 0 if no partial exec
			std::printf("thunks[i].partial_exec_amount = %ld\n", relevant_thunks[i].partial_exec_amount);
			throw std::runtime_error("invalid thunks[i].partial_exec_amount");
		}

		if (partial_exec_offer.amount < 0) {	
			std::printf("partial_exec_offer.amount = %ld]n", partial_exec_offer.amount);
			std::printf("relevant_thunks.partial_exec_amount was %ld\n", relevant_thunks[i].partial_exec_amount);
			throw std::runtime_error("invalid partial exec offer leftover amount");
		}

		//std::printf("producing dbval for modified offer\n");

		if (partial_exec_offer.amount > 0) {
			auto modified_offer_buf = xdr::xdr_to_opaque(partial_exec_offer);
			dbval modified_offer = dbval{modified_offer_buf.data(), modified_offer_buf.size()};//xdr_to_dbval(partial_exec_offer);
			wtx.put(dbi, partial_exec_key, modified_offer);
		} else {
			//partial_exec_offer.amount = 0
			wtx.del(dbi, partial_exec_key);
		}

	}
	
	if (print)
		std::printf("phase 5\n");
	//finally, clear the partial exec offers, if they clear
	for (size_t i = 0; i < relevant_thunks.size(); i++) {

		if (relevant_thunks[i].current_block_number > current_block_number) {
			throw std::runtime_error("impossible!");
			// again ignore inflight thunks
			continue;
		}

		if (print)
			std::printf("phase 5 i %lu %lu\n", i, relevant_thunks[i].current_block_number);

		if (!relevant_thunks[i].get_exists_partial_exec()) {
			continue;
		}

		for (size_t future = i + 1; future < relevant_thunks.size(); future++) {
			if (relevant_thunks[future].current_block_number > current_block_number) {
				throw std::runtime_error("impossible!!!");
				continue;
			}

			if (print)
				std::printf("continuing to future %lu %lu\n", future, relevant_thunks[future].current_block_number);
			if (relevant_thunks[i].partial_exec_key < relevant_thunks[future].partial_exec_key) {
		
				//strictly less than 0 - that is, round i's key is strictly less than round future's, so i's partial exec offer then fully clears in future.
				// We already took care of the 0 case in the preceding loop.
				auto partial_exec_key_bytes = relevant_thunks[i].partial_exec_key.get_bytes_array();
				dbval partial_exec_key{partial_exec_key_bytes};//(relevant_thunks[i].partial_exec_key.data(), MerkleWorkUnit::WORKUNIT_KEY_LEN);
				wtx.del(dbi, partial_exec_key);
			}
		}
	}
	if (print)
		std::printf("done saving workunit\n");


	ThunkGarbage output;

	for (auto& thunk : relevant_thunks) {
		output.add(
			thunk.cleared_offers
				.dump_contents_for_detached_deletion_and_clear());
	}

	commit_wtxn(wtx, current_block_number);

	return output;
}



} /* speedex */