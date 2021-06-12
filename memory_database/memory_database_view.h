#pragma once

/*! \file memory_database_view.h

Used in front of a memory_database.  Manages
positive/negative side effects in transaction processing.
We can't increase the balance of an account until we 
know that a transaction can commit - this view class
takes care of this restriction.
*/

#include "lmdb/lmdb_wrapper.h"

#include "memory_database/user_account.h"
#include "memory_database/memory_database.h"

#include "xdr/types.h"
#include "xdr/transaction.h"

#include <unordered_map>
#include <vector>
#include <cstdint>



namespace speedex {

/*! View of a single user's account.

Manages the distinction between negative and positive
side effects.

Call commit to persist positive side effects to db.
*/
class UserAccountView {
	UserAccount& main;
	//should be always positive.
	//How much additional asset to add to escrow/available accounts
	//if view is successfully committed.
	std::unordered_map<AssetID, int64_t> available_buffer;

	//should be always negative.
	//How much asset was taken out of accounts during view lifetime,
	// should be returned to owner if view is unwound.
	std::unordered_map<AssetID, int64_t> available_side_effects;

public:

	UserAccountView(UserAccount& main)
		: main(main) {}

	/*! Escrow some amount of an asset.
	Returns SUCCESS on successful escrow,
	and an error code otherwise.
	*/
	TransactionProcessingStatus conditional_escrow(
		AssetID asset, int64_t amount);

	/*! Transfer some amount of an asset to an account.

	Positive means increasing an account's balance (so 
	will get buffered), negative will get conditionally
	withdrawn from main db.

	Returns SUCCESS on successful escrow,
	and an error code otherwise.
	*/
	TransactionProcessingStatus transfer_available(
		AssetID asset, int64_t amount);

	int64_t lookup_available_balance(AssetID asset);

	//! View should not be used after commit/unwind.
	void commit();
	void unwind();
};

/*! Database view that also manages creation of new accounts.
New accounts can be used within a transaction, but not outside of that
transaction until the block commits.
*/
class AccountCreationView {
protected:
	MemoryDatabase& main_db;
	const account_db_idx db_size;
	using new_account_pair_t = std::pair<AccountID, MemoryDatabase::DBEntryT>;
	std::unordered_map<account_db_idx, new_account_pair_t> new_accounts;

	std::unordered_map<AccountID, account_db_idx> temporary_idxs;

	void commit();

	AccountCreationView(MemoryDatabase& db)
		: main_db(db),
		db_size(db.size()),
		new_accounts(),
		temporary_idxs() {}

public:
	
	bool lookup_user_id(AccountID account, account_db_idx* index_out);

	//! Create new account (id, pk).  Returns a database index
	//! This index (out) is only usable in this account view.
	TransactionProcessingStatus 
	create_new_account(
		AccountID account, const PublicKey pk, account_db_idx* out);

	TransactionProcessingStatus 
	reserve_sequence_number(account_db_idx idx, uint64_t sequence_number) {
		return main_db.reserve_sequence_number(idx, sequence_number);
	}

	void 
	commit_sequence_number(account_db_idx idx, uint64_t sequence_number) {
		main_db.commit_sequence_number(idx, sequence_number);
	}

};

/*! View of the whole database that buffers negative 
side effects before committing a transaction.

Used for block production.
*/
class BufferedMemoryDatabaseView : public AccountCreationView {


protected:
	std::unordered_map<account_db_idx, UserAccountView> accounts;

	UserAccountView& get_existing_account(account_db_idx account);

public:

	BufferedMemoryDatabaseView(MemoryDatabase& main_db)
		: AccountCreationView(main_db) {}

	void commit();
	void unwind();

	TransactionProcessingStatus escrow(
		account_db_idx account, AssetID asset, int64_t amount);
	TransactionProcessingStatus transfer_available(
		account_db_idx account, AssetID asset, int64_t amount);

	using AccountCreationView::reserve_sequence_number;
	using AccountCreationView::commit_sequence_number;
};

/*! View of database that does not buffer negative side effects.

Used for block validation, when we only check database validity
at the end of transaction processing.
*/
class UnbufferedMemoryDatabaseView : public AccountCreationView {

public:

	UnbufferedMemoryDatabaseView(
		MemoryDatabase& main_db)
		: AccountCreationView(main_db) {}

	TransactionProcessingStatus escrow(
		account_db_idx account, AssetID asset, int64_t amount);
	TransactionProcessingStatus transfer_available(
		account_db_idx account, AssetID asset, int64_t amount);

	uint64_t get_persisted_round_number() {
		return main_db.get_persisted_round_number();
	}

	using AccountCreationView::commit;
	using AccountCreationView::reserve_sequence_number;
	using AccountCreationView::commit_sequence_number;
};

/*! Mock database view that either acts as an unbuffered view or as a no-op,
depending on whether or not we are processing a transaction block
whose side effects are already present in the underlying database state.
*/
class LoadLMDBMemoryDatabaseView 
	: public LMDBLoadingWrapper<UnbufferedMemoryDatabaseView> {

	using LMDBLoadingWrapper<UnbufferedMemoryDatabaseView> :: generic_do;
public:

	LoadLMDBMemoryDatabaseView(
		uint64_t current_block_number,
		MemoryDatabase& main_db) 
	: LMDBLoadingWrapper<UnbufferedMemoryDatabaseView>(
		current_block_number, main_db) {}
	
	TransactionProcessingStatus 
	escrow(account_db_idx account, AssetID asset, int64_t amount) {
		return generic_do<&UnbufferedMemoryDatabaseView::escrow>(
			account, asset, amount);
	}
	TransactionProcessingStatus 
	transfer_available(account_db_idx account, AssetID asset, int64_t amount) {
		return generic_do<&UnbufferedMemoryDatabaseView::transfer_available>(
			account, asset, amount);
	}

	void commit() {
		return generic_do<&UnbufferedMemoryDatabaseView::commit>();
	}

	bool lookup_user_id(AccountID account, account_db_idx* index_out) {
		return generic_do<&UnbufferedMemoryDatabaseView::lookup_user_id>(
			account, index_out);
	}
	TransactionProcessingStatus 
	create_new_account(
		AccountID account, const PublicKey pk, account_db_idx* out) {
		return generic_do<&UnbufferedMemoryDatabaseView::create_new_account>(
			account, pk, out);
	}

	TransactionProcessingStatus 
	reserve_sequence_number(account_db_idx idx, uint64_t sequence_number) {
		return generic_do<&UnbufferedMemoryDatabaseView::reserve_sequence_number>(
			idx, sequence_number);
	}

	void commit_sequence_number(account_db_idx idx, uint64_t sequence_number) {
		generic_do<&UnbufferedMemoryDatabaseView::commit_sequence_number>(
			idx, sequence_number);
	}
};



/*! Occasionally it is useful to run test experiments where
we automatically grant money to accounts whenever they run out.

Not for use in actual deployment.
*/
class UnlimitedMoneyBufferedMemoryDatabaseView : public BufferedMemoryDatabaseView {

public:

	UnlimitedMoneyBufferedMemoryDatabaseView(MemoryDatabase& main_db)
		: BufferedMemoryDatabaseView(main_db) {}

	TransactionProcessingStatus escrow(
		account_db_idx account, AssetID asset, int64_t amount);
	TransactionProcessingStatus transfer_available(
		account_db_idx account, AssetID asset, int64_t amount);
};

} /* speedex */