#pragma once

/*! Utility functions to manage inserting account modifications to the 
account modification trie's value entries.

Accounts could be modified in different ways (i.e. by sending new transactions
or receiving payments).  Code here is passed into the trie insert() logic
and called on trie values.
*/

#include <cstdint>

#include "mtt/trie/prefix.h"

#include "modlog/typedefs.h"

#include "xdr/database_commitments.h"
#include "xdr/types.h"

namespace speedex {

//! Insert modifications to the log for one account
struct LogInsertFn {//: public trie::GenericInsertFn<AccountModificationTxListWrapper> {

	using AccountIDPrefix = trie::UInt64Prefix;

	//! Log that an account has been modified by one of its own past 
	//! transactions (e.g. an offer has cleared).
	static void value_insert(
		AccountModificationTxList& main_value, 
		const uint64_t self_sequence_number);

	//! Log that an account has been modified by a transaction from another
	//! account (e.g. a payment).
	static void value_insert(
		AccountModificationTxList& main_value, 
		const TxIdentifier& other_identifier);

	//! Log that an account has been modified by itself, when it sends a new
	//! transaction.
	static void value_insert(
		AccountModificationTxList& main_value, 
		const SignedTransaction& self_transaction);

	//! Initialize an empty account modification log entry.
	static AccountModificationTxListWrapper 
	new_value(const AccountIDPrefix& prefix);
};

struct LogKeyOnlyInsertFn
{
	using AccountIDPrefix = trie::UInt64Prefix;

	static void 
	value_insert(
		AccountModificationTxList& main_value,
		const void*);

	static AccountModificationTxListWrapper 
	new_value(const AccountIDPrefix& prefix);
};

//might like to make these one struct, reduce extra code/etc, but type signatures on metadata merge are slightly diff
struct LogMergeFn {
	static void value_merge(
		AccountModificationTxList& original_value, 
		const AccountModificationTxList& merge_in_value);
};

struct LogNormalizeFn {
	//! Set account modification logs to a canonical representation.
	//! This means de-duplicating and sorting modification log lists.
	static void apply_to_value (AccountModificationTxListWrapper& log);
};

} /* speedex */