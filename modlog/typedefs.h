#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mtt/trie/utils.h>
#include <mtt/trie/prefix.h>

#include "xdr/database_commitments.h"
#include "xdr/types.h"

#include <xdrpp/marshal.h>

namespace speedex
{

class TxCountMetadata;

typedef trie::UInt64Prefix AccountIDPrefix;

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

typedef AccountModificationTxListWrapper LogValueT;

typedef std::conditional<std::is_same<LogValueT, AccountModificationTxListWrapper>::value,
	void,
	TxCountMetadata>::type LogValueMetadataT;

} // namespace speedex
