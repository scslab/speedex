#include "memory_database/user_account.h"

#include <xdrpp/marshal.h>
#include <cinttypes>


namespace speedex {

UserAccount::UserAccount(AccountID owner, PublicKey public_key)
	: uncommitted_assets_mtx()
	, owned_assets()
	, uncommitted_assets()
	, sequence_number_vec(0)
	, last_committed_id(0)
	, owner(owner)
	, pk(public_key)
{}

UserAccount::UserAccount()
	: uncommitted_assets_mtx()
	, owned_assets()
	, uncommitted_assets()
	, sequence_number_vec(0)
	, last_committed_id(UINT64_MAX)
	, owner()
	, pk()
{}


inline uint8_t 
get_seq_num_offset(uint64_t sequence_number, uint64_t last_committed_id) {

	uint64_t offset = (((sequence_number - last_committed_id) / MAX_OPS_PER_TX) - 1);
	return (offset >= 64 ? 255 : offset);
}

inline uint64_t 
get_seq_num_increment(uint64_t bv) {
	if (bv == 0) return 0;
	return (64 - __builtin_clzll(bv)) * MAX_OPS_PER_TX;
}

TransactionProcessingStatus 
UserAccount::reserve_sequence_number(
	uint64_t sequence_number) {

	if (sequence_number <= last_committed_id) {
		return TransactionProcessingStatus::SEQ_NUM_TOO_LOW;
	}

	uint8_t offset = get_seq_num_offset(sequence_number, last_committed_id);

	if (offset >= 64) {
		return TransactionProcessingStatus::SEQ_NUM_TOO_HIGH;
	}


	uint64_t bit_mask = ((uint64_t) 1) << offset;

	uint64_t prev = sequence_number_vec.fetch_or(bit_mask, std::memory_order_relaxed);

	if ((prev & bit_mask) != 0) {
		//some other tx has already reserved the sequence number
		return TransactionProcessingStatus::SEQ_NUM_TEMP_IN_USE;
	}

	return TransactionProcessingStatus::SUCCESS;
}

void UserAccount::release_sequence_number(
	uint64_t sequence_number) {

	if (sequence_number <= last_committed_id) {
		throw std::runtime_error("cannot release invalid seq num!");
	}

	uint8_t offset = get_seq_num_offset(sequence_number, last_committed_id);

	if (offset >= 64) {
		throw std::runtime_error("cannot release too far forward seq num!");
	}

	uint64_t bit_mask = ~(((uint64_t) 1) << offset);

	sequence_number_vec.fetch_and(bit_mask, std::memory_order_relaxed);
}

void UserAccount::commit_sequence_number(
	uint64_t sequence_number) {
}


void UserAccount::commit() {
	std::lock_guard lock(uncommitted_assets_mtx);

	for (auto iter = owned_assets.begin(); iter != owned_assets.end(); iter++) {
		iter->commit();
	}
	int uncommitted_size = uncommitted_assets.size();
	for (int i = 0; i < uncommitted_size; i++) {
		owned_assets.emplace_back(uncommitted_assets[i].commit());
	}

	uncommitted_assets.clear();
	last_committed_id += get_seq_num_increment(
		sequence_number_vec.load(std::memory_order_relaxed));
	sequence_number_vec.store(0, std::memory_order_relaxed);
}

void UserAccount::rollback() {
	std::lock_guard lock(uncommitted_assets_mtx);

	for (auto iter = owned_assets.begin(); iter != owned_assets.end(); iter++) {
		iter->rollback();
	}
	uncommitted_assets.clear();

	sequence_number_vec.store(0, std::memory_order_relaxed);
}

bool UserAccount::in_valid_state() {
	std::lock_guard lock(uncommitted_assets_mtx);

	for (auto iter = owned_assets.begin(); iter != owned_assets.end(); iter++) {
		if (!iter->in_valid_state()) {
			return false;
		}
	}
	for (auto iter = uncommitted_assets.begin();
	 	iter != uncommitted_assets.end(); 
	 	iter++) 
	{
		if (!iter->in_valid_state()) {
			return false;
		}
	}
	return true;
}

AccountCommitment UserAccount::produce_commitment() const {
	std::lock_guard lock(uncommitted_assets_mtx);

	AccountCommitment output;
	output.owner = owner;
	for (uint8_t i = 0; i < owned_assets.size(); i++) {
		output.assets.push_back(owned_assets[i].produce_commitment(i));
	}
	output.last_committed_id = last_committed_id;
	output.pk = pk;
	return output;
}

AccountCommitment UserAccount::tentative_commitment() const {
	std::lock_guard lock(uncommitted_assets_mtx);

	AccountCommitment output;
	output.owner = owner;
	for (uint8_t i = 0; i < owned_assets.size(); i++) {
		output.assets.push_back(owned_assets[i].tentative_commitment(i));
	}
	for (uint8_t i = 0; i < uncommitted_assets.size(); i++) {
		output.assets.push_back(
			uncommitted_assets[i].tentative_commitment(i + owned_assets.size()));
	}
	output.last_committed_id = 
		last_committed_id 
		+ get_seq_num_increment(sequence_number_vec.load(std::memory_order_relaxed));
	output.pk = pk;

	return output;
}

AccountID UserAccount::read_lmdb_key(const dbval& key) {
	return key.uint64(); // don't copy the database across systems with diff endianness
}

void 
UserAccount::set_owner(AccountID _owner, PublicKey const& _pk, uint64_t _last_committed_id)
{
	owner = _owner;
	pk = _pk;
	last_committed_id = _last_committed_id;
}

void 
UserAccount::log() const
{
	for (uint32_t i = 0; i < owned_assets.size(); i++) {
		std::printf(
			"%" PRIu32 "=%" PRId64 " ", i, owned_assets[i].lookup_available_balance());
	}
	std::printf("\n");
}

} /* speedex */
