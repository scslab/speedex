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

#include <cstdint>
#include <forward_list>
#include <unordered_map>
#include <vector>

namespace speedex {

/*! View of a single user's account.

Manages the distinction between negative and positive
side effects.

Call commit to persist positive side effects to db.
*/
class UserAccountView {
	MemoryDatabase& main_db;
	UserAccount* main;
	//should be always positive.
	//How much additional asset to add to escrow/available accounts
	//if view is successfully committed.
	std::unordered_map<AssetID, int64_t> available_buffer;

	//should be always negative.
	//How much asset was taken out of accounts during view lifetime,
	// should be returned to owner if view is unwound.
	std::unordered_map<AssetID, int64_t> available_side_effects;

public:

	UserAccountView(
		MemoryDatabase& main_db,
		UserAccount* main)
		: main_db(main_db)
		, main(main)
		{}

	/*! Escrow some amount of an asset.
	Returns SUCCESS on successful escrow,
	and an error code otherwise.
	*/
	TransactionProcessingStatus conditional_escrow(
		AssetID asset, int64_t amount, const char* reason);

	/*! Transfer some amount of an asset to an account.

	Positive means increasing an account's balance (so 
	will get buffered), negative will get conditionally
	withdrawn from main db.

	Returns SUCCESS on successful escrow,
	and an error code otherwise.
	*/
	TransactionProcessingStatus transfer_available(
		AssetID asset, int64_t amount, const char* reason);

	int64_t lookup_available_balance(AssetID asset);

	TransactionProcessingStatus reserve_sequence_number(uint64_t seqno);

	//! View should not be used after commit/unwind.
	void commit();
	void unwind();
};

struct SeqnoReservation
{
	UserAccount* account;
	uint64_t seqno;
};

/*! Database view that also manages creation of new accounts.
New accounts can be used within a transaction, but not outside of that
transaction until the block commits.
*/
class AccountCreationView {
protected:
	MemoryDatabase& main_db;
	
	using new_account_pair_t = std::pair<AccountID, UserAccount>;

	std::forward_list<new_account_pair_t> new_accounts;
	
	//std::unordered_map<account_db_idx, new_account_pair_t> new_accounts;

	std::unordered_map<AccountID, UserAccount*> temporary_idxs;

	std::vector<SeqnoReservation> reservations;

	void commit();
	void unwind();

	AccountCreationView(MemoryDatabase& db)
		: main_db(db)
		, new_accounts()
		, temporary_idxs()
		, reservations() {}

public:

	UserAccount* lookup_user(AccountID account);
	
	//! Create new account (id, pk).  Returns a database index
	//! This index (out) is only usable in this account view.
	TransactionProcessingStatus 
	create_new_account(
		AccountID account, const PublicKey pk, UserAccount** out);

	TransactionProcessingStatus 
	reserve_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		auto status = main_db.reserve_sequence_number(idx, sequence_number);

		if (status == TransactionProcessingStatus::SUCCESS)
		{
			reservations.emplace_back(idx, sequence_number);
		}
		return status;
	}

/*	void 
	commit_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		main_db.commit_sequence_number(idx, sequence_number);
	}

	void release_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		main_db.release_sequence_number(idx, sequence_number);
	} */
};

/*! View of the whole database that buffers negative 
side effects before committing a transaction.

Used for block production.
*/
class BufferedMemoryDatabaseView : public AccountCreationView {


protected:
	std::unordered_map<UserAccount*, UserAccountView> accounts;

	UserAccountView& get_existing_account(UserAccount* account);

public:

	BufferedMemoryDatabaseView(MemoryDatabase& main_db)
		: AccountCreationView(main_db) {}

	void commit();
	void unwind();

	TransactionProcessingStatus escrow(
		UserAccount* account, AssetID asset, int64_t amount, const char* reason);
	TransactionProcessingStatus transfer_available(
		UserAccount* account, AssetID asset, int64_t amount, const char* reason);

	using AccountCreationView::reserve_sequence_number;
	//using AccountCreationView::commit_sequence_number;
	//using AccountCreationView::release_sequence_number;
};

/*! View of database that does not buffer negative side effects.

Used for block validation, when we only check database validity
at the end of transaction processing.

Also used when unwinding transactions.  In this case, only 
things to unwind are balance changes (not account creations),
so no need to commit.
*/
class UnbufferedMemoryDatabaseView : public AccountCreationView {

public:

	UnbufferedMemoryDatabaseView(
		MemoryDatabase& main_db)
		: AccountCreationView(main_db) {}

	TransactionProcessingStatus escrow(
		UserAccount* account, AssetID asset, int64_t amount, const char* reason);
	TransactionProcessingStatus transfer_available(
		UserAccount* account, AssetID asset, int64_t amount, const char* reason);

	//uint64_t get_persisted_round_number() {
//		return main_db.get_persisted_round_number();
//	}

	using AccountCreationView::commit;
	using AccountCreationView::reserve_sequence_number;
	//using AccountCreationView::commit_sequence_number;
	//using AccountCreationView::release_sequence_number;
};

class LoadLMDBMemoryDatabaseView
{
	UnbufferedMemoryDatabaseView base_view;

	MemoryDatabase const& main_db;

	const uint64_t current_block_number;

	bool do_action(const UserAccount* account)
	{
		AccountID id = account -> get_owner();

		return main_db.get_persisted_round_number_by_account(id) < current_block_number;
	}

public:

	LoadLMDBMemoryDatabaseView(
		uint64_t current_block_number,
		MemoryDatabase& main_db)
		: base_view(main_db)
		, main_db(main_db)
		, current_block_number(current_block_number)
	{}

	TransactionProcessingStatus
	escrow(UserAccount* account, AssetID asset, int64_t amount, const char* reason)
	{
		if (do_action(account))
		{
			return base_view.escrow(account, asset, amount, (std::string("loading from db:") + reason).c_str());
		}
		return TransactionProcessingStatus::SUCCESS;
	}

	TransactionProcessingStatus 
	transfer_available(UserAccount* account, AssetID asset, int64_t amount, const char* reason) {
		if (do_action(account))
		{
			return base_view.transfer_available(account, asset, amount, (std::string("loading from db:") + reason).c_str());
		}
		return TransactionProcessingStatus::SUCCESS;
	}

	void commit() {
		base_view.commit();
	}

	UserAccount* lookup_user(AccountID account) {
		return base_view.lookup_user(account);
	}

	TransactionProcessingStatus 
	create_new_account(
		AccountID account, const PublicKey pk, UserAccount** out) {
		// ok to call this even if account doesn't exist yet
		if (main_db.get_persisted_round_number_by_account(account) < current_block_number)
		{
			return base_view.create_new_account(account, pk, out);
		}
		else
		{
			*out = base_view.lookup_user(account);
			return TransactionProcessingStatus::SUCCESS;
		}
	}

	TransactionProcessingStatus 
	reserve_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		if (do_action(idx))
		{
			return base_view.reserve_sequence_number(idx, sequence_number);
		}
		return TransactionProcessingStatus::SUCCESS;
	}

	/*void commit_sequence_number(UserAccount* idx, uint64_t sequence_number)
	{
		if (do_action(idx))
		{
			base_view.commit_sequence_number(idx, sequence_number);
		}
	}

	void release_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		throw std::runtime_error("release_sequence_number should never be called when replaying trusted block");
	} */
};

/*! Mock database view that either acts as an unbuffered view or as a no-op,
depending on whether or not we are processing a transaction block
whose side effects are already present in the underlying database state.
*/
/*
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
	escrow(UserAccount* account, AssetID asset, int64_t amount) {
		return generic_do<&UnbufferedMemoryDatabaseView::escrow>(
			account, asset, amount);
	}
	TransactionProcessingStatus 
	transfer_available(UserAccount* account, AssetID asset, int64_t amount) {
		return generic_do<&UnbufferedMemoryDatabaseView::transfer_available>(
			account, asset, amount);
	}

	void commit() {
		return generic_do<&UnbufferedMemoryDatabaseView::commit>();
	}

	UserAccount* lookup_user(AccountID account) {
		return unconditional_do<&UnbufferedMemoryDatabaseView::lookup_user>(account);
	}

	TransactionProcessingStatus 
	create_new_account(
		AccountID account, const PublicKey pk, UserAccount** out) {
		return generic_do<&UnbufferedMemoryDatabaseView::create_new_account>(
			account, pk, out);
	}

	TransactionProcessingStatus 
	reserve_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		return generic_do<&UnbufferedMemoryDatabaseView::reserve_sequence_number>(
			idx, sequence_number);
	}

	void commit_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		generic_do<&UnbufferedMemoryDatabaseView::commit_sequence_number>(
			idx, sequence_number);
	}

	void release_sequence_number(UserAccount* idx, uint64_t sequence_number) {
		generic_do<&UnbufferedMemoryDatabaseView::release_sequence_number>(
			idx, sequence_number);
	}
}; */

} /* speedex */