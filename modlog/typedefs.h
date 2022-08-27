#pragma once

#include "mtt/trie/utils.h"

#include "xdr/database_commitments.h"
#include "xdr/types.h"

#include <xdrpp/marshal.h>

namespace speedex
{

// warning police
struct ModificationLogFns
{
    static std::vector<uint8_t> serialize(const AccountModificationTxList& v)
    {
        return xdr::xdr_to_opaque(v);
    }
};

typedef trie::XdrTypeWrapper<AccountModificationTxList,
                             &ModificationLogFns::serialize>
    AccountModificationTxListWrapper;

} // namespace speedex
