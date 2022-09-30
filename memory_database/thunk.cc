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
