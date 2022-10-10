#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <set>

#include "xdr/database_commitments.h"
#include "xdr/transaction.h"

namespace speedex
{

[[maybe_unused]] static std::strong_ordering
operator<=>(const TxIdentifier& a, const TxIdentifier& b)
{
    auto res = a.owner <=> b.owner;

    if (res != std::strong_ordering::equal)
    {
        return res;
    }
    return a.sequence_number <=> b.sequence_number;
}

namespace detail
{
struct TxComparator
{
    // this gets around -Wsubobject-linkage, which
    // occurs if you do the recommended set<STx, decltype(cmp)>
    bool operator()(const SignedTransaction& a,
                    const SignedTransaction& b) const
    {
        return a.transaction.metadata.sequenceNumber
               < b.transaction.metadata.sequenceNumber;
    }
};

} // namespace detail

class AccountModificationEntry
{
    std::optional<AccountID> owner;

    std::set<uint64_t> identifiers_self;
    std::set<TxIdentifier> identifiers_other;
    std::set<SignedTransaction, detail::TxComparator> new_transactions_self;

    friend struct EntryAccumulateValuesFn;
    friend struct TxCountMetadata;

    std::pair<uint8_t*, size_t> serialize_xdr() const;

public:
    AccountModificationEntry()
        : owner()
        , identifiers_self()
        , identifiers_other()
        , new_transactions_self()
    {}

    AccountModificationEntry(AccountID owner)
        : owner(owner)
        , identifiers_self()
        , identifiers_other()
        , new_transactions_self()
    {}

    void add_identifier_self(uint64_t id);
    void add_identifier_other(TxIdentifier const& id);
    void add_tx_self(const SignedTransaction& tx);

    void merge_value(AccountModificationEntry& other_value);

    void copy_data(std::vector<uint8_t>& buf) const
    {
        auto [ptr, sz] = serialize_xdr();
        buf.insert(buf.end(), ptr, ptr + sz);
        delete[] ptr;
    }
};

struct TxCountMetadata
{
    int32_t num_txs = 0;

    TxCountMetadata& operator+=(const TxCountMetadata& other)
    {
        num_txs += other.num_txs;
        return *this;
    }

    friend TxCountMetadata operator-(TxCountMetadata lhs,
                                     TxCountMetadata const& rhs)
    {
        lhs.num_txs -= rhs.num_txs;
        return lhs;
    }

    bool operator==(const TxCountMetadata& other) const = default;

    TxCountMetadata operator-() const
    {
        return TxCountMetadata{ .num_txs = -this->num_txs };
    }

    constexpr static TxCountMetadata zero()
    {
        return TxCountMetadata{ .num_txs = 0 };
    }

    static TxCountMetadata from_value(AccountModificationEntry const& val)
    {
        return TxCountMetadata{ .num_txs = static_cast<int32_t>(
                                    val.new_transactions_self.size()) };
    }

    std::string to_string() const
    {
        return std::string("ntxs: ") + std::to_string(num_txs);
    }
};

struct EntryAccumulateValuesFn
{
    template<typename VectorType>
    static void accumulate(VectorType& vector,
                           size_t vector_offset,
                           const AccountModificationEntry& value)
    {
        for (auto const& tx : value.new_transactions_self)
        {
            vector[vector_offset] = tx;
            vector_offset++;
        }
    }

    template<typename MetadataType>
    static size_t size_increment(const MetadataType& metadata)
    {
        return metadata.metadata.num_txs;
    }

    template<typename MetadataType>
    static size_t vector_size(const MetadataType& root_metadata)
    {
        std::printf("starting vector_size: %lu\n",
                    root_metadata.metadata.num_txs);
        return root_metadata.metadata.num_txs;
    }
};

} // namespace speedex
