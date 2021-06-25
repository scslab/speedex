#include "modlog/log_entry_fns.h"

namespace speedex {

struct TxIdentifierCompareFn {
	bool operator() (const TxIdentifier& a, const TxIdentifier& b) {
		return (a.owner < b.owner) || ((a.owner == b.owner) && (a.sequence_number < b.sequence_number));
	}
};

struct NewSelfTransactionCompareFn {
	bool operator() (const SignedTransaction& a, const SignedTransaction& b) {
		return  (a.transaction.metadata.sequenceNumber < b.transaction.metadata.sequenceNumber);
	}
};

template<typename value_list, typename CompareFn>
void dedup(value_list& values, CompareFn comparator) {
	for (size_t i = 1u; i < values.size(); i++) {
		if (comparator(values[i], values[i-1])) {
			values.erase(values.begin() + i);
		} else {
			i++;
		}
	}
}

void 
LogMergeFn::value_merge(
	AccountModificationTxList& original_value, 
	const AccountModificationTxList& merge_in_value)
{
	for (const auto& new_tx : merge_in_value.new_transactions_self) {
		original_value.new_transactions_self.push_back(new_tx);
	}
	//original_value.new_transactions_self.insert(
	//	original_value.new_transactions_self.end(),
	//	merge_in_value.new_transactions_self.begin(),
	//	merge_in_value.new_transactions_self.end());
	original_value.identifiers_self.insert(
		original_value.identifiers_self.end(),
		merge_in_value.identifiers_self.begin(),
		merge_in_value.identifiers_self.end());
	original_value.identifiers_others.insert(
		original_value.identifiers_others.end(),
		merge_in_value.identifiers_others.begin(),
		merge_in_value.identifiers_others.end());

	if (original_value.owner != merge_in_value.owner) {
		throw std::runtime_error("owner mismatch when merging logs!!!");
	}
}


void 
LogNormalizeFn::apply_to_value (AccountModificationTxListWrapper& log) {
	std::sort(log.identifiers_self.begin(), log.identifiers_self.end());
	std::sort(log.identifiers_others.begin(), log.identifiers_others.end(), TxIdentifierCompareFn());
	std::sort(log.new_transactions_self.begin(), log.new_transactions_self.end(), NewSelfTransactionCompareFn());

	//dedup
	for (std::size_t i = 1; i < log.identifiers_self.size(); i++) {
		if (log.identifiers_self[i] == log.identifiers_self[i-1]) {
			log.identifiers_self.erase(log.identifiers_self.begin() + i);
		} else {
			i++;
		}
	}

	auto tx_identifier_eq = [] (const TxIdentifier& a, const TxIdentifier& b) -> bool {
		return (a.owner == b.owner) && (a.sequence_number == b.sequence_number);
	};

	dedup(log.identifiers_others, tx_identifier_eq);

	auto transaction_eq = [] (const SignedTransaction& a, const SignedTransaction& b) -> bool {
		return (a.transaction.metadata.sequenceNumber == b.transaction.metadata.sequenceNumber);
	};

	dedup(log.new_transactions_self, transaction_eq);
}


} /* speedex */
