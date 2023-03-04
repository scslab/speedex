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

#include "memory_database/thunk.h"

#include "memory_database/memory_database.h"

namespace speedex
{

void KVAssignment::operator=(const AccountID account) {

	UserAccount* idx = db.lookup_user(account);
	//account_db_idx idx;
	//if (!db.lookup_user_id(account, &idx)) {
	if (idx == nullptr) {
		throw std::runtime_error("can't commit invalid account");
	}
	AccountCommitment commitment = db.produce_commitment(idx);
	kv.key = account;
	kv.msg = xdr::xdr_to_opaque(commitment);
} 

KVAssignment 
DBPersistenceThunk::operator[](size_t idx) {
	if (idx >= kvs->size()) {
		throw std::runtime_error(
			std::string("invalid kvs access: ")
			+ std::to_string(idx)
			+ std::string("(size: ")
			+ std::to_string(kvs->size())
			+ std::string(")"));
	}

	return KVAssignment{kvs->at(idx), *db};
}

} /* speedex */
