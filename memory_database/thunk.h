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

#include "xdr/types.h"

#include <xdrpp/types.h>

namespace speedex
{

class MemoryDatabase;

/*! Database thunk entry.
Key of modified account, and bytes vector to store in database.
*/
struct ThunkKVPair {
	AccountID key;
	xdr::opaque_vec<> msg;

	ThunkKVPair() = default;
};

/*! Transient value for accumulating the contents of a DBPersistenceThunk.

The nonstandard operator= takes in an accountID and computes the ThunkKVPair
for the account.

Used when iterating over an account mod log.
*/
struct KVAssignment {
	ThunkKVPair& kv;
	const MemoryDatabase& db;
	void operator=(const AccountID owner);
};


/*! Stores all of the changes to the account database, to be persisted to disk
later.

Acts as a vector for the purposes of account mod log's 
parallel_accumulate_values().  Returns KVAssignment objects in response to 
operator[].  Operator=, applied to these output values, inserts a ThunkKVPair
into this thunk (at the location that was given to operator[]).
*/
struct DBPersistenceThunk {
	using thunk_list_t = std::vector<ThunkKVPair>;

	std::unique_ptr<thunk_list_t> kvs;
	MemoryDatabase* db;
	uint64_t current_block_number;

	DBPersistenceThunk(MemoryDatabase& db, uint64_t current_block_number)
		: kvs(std::make_unique<thunk_list_t>())
		, db(&db)
		, current_block_number(current_block_number) {}

	// used when building thunk, not when accessing.
	// user should access kvs directly.
	KVAssignment operator[](size_t idx);

	void clear() {
		kvs.reset();
	}

	void resize(size_t sz) {
		kvs->resize(sz);
	}

	void reserve(size_t sz) {
		kvs->reserve(sz);
	}
	
	size_t size() const {
		return kvs->size();
	}
};


} /* speedex */
