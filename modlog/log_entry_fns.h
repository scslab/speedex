#pragma once

#include <cstdint>

#include "trie/utils.h"
#include "trie/prefix.h"

#include "xdr/database_commitments.h"
#include "xdr/types.h"

namespace speedex {


struct LogInsertFn : public GenericInsertFn {
	using AccountModificationTxListWrapper = XdrTypeWrapper<AccountModificationTxList>;

	static void value_insert(AccountModificationTxList& main_value, const uint64_t self_sequence_number) {
		main_value.identifiers_self.push_back(self_sequence_number);
	}

	static void value_insert(AccountModificationTxList& main_value, const TxIdentifier& other_identifier) {
		main_value.identifiers_others.push_back(other_identifier);
	}

	static void value_insert(AccountModificationTxList& main_value, const SignedTransaction& self_transaction) {
		main_value.new_transactions_self.push_back(self_transaction);
		//if (main_value.new_transactions_self.size() > main_value.new_transactions_self.capacity() || main_value.new_transactions_self.size() > 500000) {
		//	std::printf("%lu %lu %lu\n", main_value.new_transactions_self.size(), main_value.new_transactions_self.capacity(),
		//		main_value.new_transactions_self.size());
		//	throw std::runtime_error("invalid main_value!!!");
		//}
	}
/*
	template<typename AtomicMetadataType, typename ValueType>
	static typename AtomicMetadataType::BaseT 
	metadata_insert(AtomicMetadataType& original_metadata, const ValueType& new_value) {
		//multiple insertions to one key doesn't change any metadata, since metadata is only size
		return typename AtomicMetadataType::BaseT();
	}
*/
	template<typename OutType>
	static AccountModificationTxListWrapper new_value(const AccountIDPrefix prefix) {
		static_assert(std::is_same<OutType, AccountModificationTxListWrapper>::value, "invalid type invocation");
		AccountModificationTxListWrapper out;
		out.owner = prefix.get_account();
		return out;
	}
};

//might like to make these one struct, reduce extra code/etc, but type signatures on metadata merge are slightly diff
struct LogMergeFn {
	static void value_merge(
		AccountModificationTxList& original_value, 
		const AccountModificationTxList& merge_in_value);
};

struct LogNormalizeFn {
	using AccountModificationTxListWrapper = XdrTypeWrapper<AccountModificationTxList>;
	static void apply_to_value (AccountModificationTxListWrapper& log);
};

} /* speedex */