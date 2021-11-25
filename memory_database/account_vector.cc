#include "memory_database/account_vector.h"

namespace speedex {


UserAccount* 
AccountVector::AccountVectorRow::append(UserAccount&& acct) {
	row[num_active_entries] = std::move(acct);
	UserAccount* out = &row[num_active_entries];
	num_active_entries++;
	return out;
}

size_t 
AccountVector::AccountVectorRow::erase(size_t num_to_erase) {
	size_t num_eraseable = std::min(num_to_erase, num_active_entries);
	num_active_entries -= num_eraseable;
	return num_eraseable;
}

UserAccount* 
AccountVector::AccountVectorRow::get(account_db_idx idx) {
	return &row[idx];
}

size_t
AccountVector::AccountVectorRow::resize(size_t sz) {
	if (num_active_entries > ACCOUNTS_PER_ROW) {
		throw std::runtime_error("assigned past max");
	}
	size_t remaining = ACCOUNTS_PER_ROW - num_active_entries;
	size_t num_addable = std::max(sz, remaining);
	num_active_entries += num_addable;
	return num_addable;
}


AccountVector::AccountVector()
	: accounts()
	, next_open_idx(0)
	, _size(0)
	{
		accounts.push_back(std::make_unique<AccountVectorRow>());
	}

UserAccount* 
AccountVector::emplace_back(UserAccount&& acct) {
	UserAccount* out = accounts[next_open_idx]->append(std::move(acct));
	if (accounts[next_open_idx]->is_full()) {
		next_open_idx++;
		if (accounts.size() == next_open_idx) {
			accounts.push_back(std::make_unique<AccountVectorRow>());
		}
	}
	_size++;
	return out;
}

void 
AccountVector::erase(size_t num_to_erase) {
	_size -= num_to_erase;
	while (num_to_erase != 0) {
		size_t erased = accounts[next_open_idx]->erase(num_to_erase);
		num_to_erase -= erased;
		if (num_to_erase > 0) {
			next_open_idx--;
		}
	}
}

UserAccount* 
AccountVector::get(account_db_idx idx) {
	uint64_t offset = idx >> ROW_OFFSET_LSHIFT;
	uint64_t row_idx = idx & LOWBITS_MASK;
	return accounts[offset]->get(row_idx);
}

void
AccountVector::resize(size_t sz) {
	if(sz <= _size) {
		return;
	}

	size_t num_to_add = sz - _size;

	while (num_to_add > 0) {
		size_t num_added = accounts[next_open_idx] -> resize(num_to_add);
		num_to_add -= num_added;
		_size += num_added;
		if (accounts[next_open_idx] -> is_full()) {
			next_open_idx ++;
			if (accounts.size() == next_open_idx) {
				accounts.push_back(std::make_unique<AccountVectorRow>());
			}
		}
	}
	if (sz != _size) {
		throw std::runtime_error("resize error");
	}
}



} /* speedex */
