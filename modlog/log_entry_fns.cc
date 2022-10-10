#include "modlog/log_entry_fns.h"

#include "modlog/account_modification_entry.h"

namespace speedex
{

void
LogEntryInsertFn::value_insert(AccountModificationEntry& main_value,
                          const uint64_t self_sequence_number)
{
    main_value.add_identifier_self(self_sequence_number);
}

void
LogEntryInsertFn::value_insert(AccountModificationEntry& main_value,
                          const TxIdentifier& other_identifier)
{
    main_value.add_identifier_other(other_identifier);
}

void
LogEntryInsertFn::value_insert(AccountModificationEntry& main_value,
                          const SignedTransaction& self_transaction)
{
    main_value.add_tx_self(self_transaction);
}

AccountModificationEntry
LogEntryInsertFn::new_value(const AccountIDPrefix& prefix)
{
    return AccountModificationEntry (prefix.uint64());
}

void
LogListInsertFn::value_insert(AccountModificationTxListWrapper& main_value,
                          const uint64_t self_sequence_number)
{
    main_value.identifiers_self.push_back(self_sequence_number);
}

//! Log that an account has been modified by a transaction from another
//! account (e.g. a payment).
void
LogListInsertFn::value_insert(AccountModificationTxListWrapper& main_value,
                          const TxIdentifier& other_identifier)
{
    main_value.identifiers_others.push_back(other_identifier);
}

//! Log that an account has been modified by itself, when it sends a new
//! transaction.
void
LogListInsertFn::value_insert(AccountModificationTxListWrapper& main_value,
                          const SignedTransaction& self_transaction)
{
    main_value.new_transactions_self.push_back(self_transaction);
}

//! Initialize an empty account modification log entry.
AccountModificationTxListWrapper
LogListInsertFn::new_value(const AccountIDPrefix& prefix)
{
    AccountModificationTxListWrapper out;
    out.owner = prefix.uint64();
    return out;
}

AccountModificationTxListWrapper
LogKeyOnlyInsertFn::new_value(const AccountIDPrefix& prefix)
{
    AccountModificationTxListWrapper out;
    out.owner = prefix.uint64();
    return out;
}

void 
LogKeyOnlyInsertFn::value_insert(
    AccountModificationTxListWrapper& main_value,
    const void*)
{

}

struct TxIdentifierCompareFn
{
    bool operator()(const TxIdentifier& a, const TxIdentifier& b)
    {
        return (a.owner < b.owner)
               || ((a.owner == b.owner)
                   && (a.sequence_number < b.sequence_number));
    }
};

struct NewSelfTransactionCompareFn
{
    bool operator()(const SignedTransaction& a, const SignedTransaction& b)
    {
        return (a.transaction.metadata.sequenceNumber
                < b.transaction.metadata.sequenceNumber);
    }
};

template<typename value_list, typename CompareFn>
void
dedup(value_list& values, CompareFn comparator)
{
    for (size_t i = 1u; i < values.size(); i++)
    {
        if (comparator(values[i], values[i - 1]))
        {
            values.erase(values.begin() + i);
        } else
        {
            i++;
        }
    }
}

void
LogMergeFn::value_merge(AccountModificationTxListWrapper& original_value,
                        AccountModificationTxListWrapper& merge_in_value)
{
    for (const auto& new_tx : merge_in_value.new_transactions_self)
    {
        original_value.new_transactions_self.push_back(new_tx);
    }
    // original_value.new_transactions_self.insert(
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

    if (original_value.owner != merge_in_value.owner)
    {
        throw std::runtime_error("owner mismatch when merging logs!!!");
    }
}

void
LogNormalizeFn::apply_to_value(AccountModificationTxListWrapper& log)
{
    std::sort(log.identifiers_self.begin(), log.identifiers_self.end());
    std::sort(log.identifiers_others.begin(),
              log.identifiers_others.end(),
              TxIdentifierCompareFn());
    std::sort(log.new_transactions_self.begin(),
              log.new_transactions_self.end(),
              NewSelfTransactionCompareFn());

    // dedup
    for (std::size_t i = 1; i < log.identifiers_self.size(); i++)
    {
        if (log.identifiers_self[i] == log.identifiers_self[i - 1])
        {
            log.identifiers_self.erase(log.identifiers_self.begin() + i);
        } else
        {
            i++;
        }
    }

    auto tx_identifier_eq
        = [](const TxIdentifier& a, const TxIdentifier& b) -> bool {
        return (a.owner == b.owner) && (a.sequence_number == b.sequence_number);
    };

    dedup(log.identifiers_others, tx_identifier_eq);

    auto transaction_eq
        = [](const SignedTransaction& a, const SignedTransaction& b) -> bool {
        return (a.transaction.metadata.sequenceNumber
                == b.transaction.metadata.sequenceNumber);
    };

    dedup(log.new_transactions_self, transaction_eq);
}

} // namespace speedex
