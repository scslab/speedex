#pragma once

#include "memory_database/typedefs.h"
#include "memory_database/user_account.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace speedex {

class AccountVector {

	constexpr static uint64_t ACCOUNTS_PER_ROW = 0x1'0000; // 2^16
	constexpr static uint64_t LOWBITS_MASK = 0xFFFF; // 2^16 - 1
	constexpr static uint64_t ROW_OFFSET_LSHIFT = 16;

	class AccountVectorRow {
		size_t num_active_entries;
		std::array<UserAccount, ACCOUNTS_PER_ROW> row;

	public:
		AccountVectorRow()
			: num_active_entries(0)
			, row()
			{}

		bool is_full() const {
			return num_active_entries == ACCOUNTS_PER_ROW;
		}

		UserAccount* append(UserAccount&& acct);
		//returns num_erased
		size_t erase(size_t num_to_erase);
		UserAccount* get(account_db_idx idx);
		size_t resize(size_t sz);
	};

	std::vector<std::unique_ptr<AccountVectorRow>> accounts;
	size_t next_open_idx;
	size_t _size;

public:

	AccountVector();

	UserAccount* emplace_back(UserAccount&& acct);

	void erase(size_t num_to_erase);
	UserAccount* get(account_db_idx idx);

	size_t size() const {
		return _size;
	}

	void resize(size_t sz);

};


} /* speedex */
