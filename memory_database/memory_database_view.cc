#include "memory_database/memory_database_view.h"

#include "utils/debug_macros.h"


namespace speedex {

TransactionProcessingStatus UserAccountView::conditional_escrow(
	AssetID asset, int64_t amount) {
	if (amount < 0) {
		//freeing escrowed money
		available_buffer[asset] -= amount;
		return TransactionProcessingStatus::SUCCESS;
	}
	int64_t current_available_buffer = available_buffer[asset];
	int64_t new_buffer = current_available_buffer - amount;
	if (new_buffer < 0) {
		INFO("new_buffer = %ld", new_buffer);
		INFO("current amount:%ld", main.lookup_available_balance(asset));
		auto result = main.conditional_escrow(asset, -new_buffer);
		if (!result) {
			return TransactionProcessingStatus::INSUFFICIENT_BALANCE;
		}
		available_side_effects[asset] += new_buffer;
		new_buffer = 0;
	}
	available_buffer[asset] = new_buffer;
	return TransactionProcessingStatus::SUCCESS;
}

TransactionProcessingStatus UserAccountView::transfer_available(
	AssetID asset, int64_t amount) {
	
	int64_t current_buffer = available_buffer[asset];
	int64_t new_buffer = current_buffer + amount;
	if (new_buffer < 0) {
		bool result = main.conditional_transfer_available(asset, new_buffer);
		if (!result) {
			return TransactionProcessingStatus::INSUFFICIENT_BALANCE;
		}
		available_side_effects[asset] += new_buffer;
		new_buffer = 0;
	}
	available_buffer[asset] = new_buffer;
	return TransactionProcessingStatus::SUCCESS;
}

int64_t
UserAccountView::lookup_available_balance(AssetID asset) {
	return main.lookup_available_balance(asset) + available_buffer[asset];
}

void UserAccountView::commit() {
	for (auto iter = available_buffer.begin(); 
		iter != available_buffer.end(); 
		iter++) {
		main.transfer_available(iter->first, iter->second);
	}
}

void UserAccountView::unwind() {
	for (auto iter = available_side_effects.begin(); 
		iter != available_side_effects.end(); 
		iter++) {
		main.transfer_available(iter->first, -iter->second);
	}
}

TransactionProcessingStatus
BufferedMemoryDatabaseView::escrow(
	UserAccount* account, AssetID asset, int64_t amount) {
	

	for (auto& it : new_accounts) {
		if (&(it.second) == account) {
			auto res = account ->conditional_escrow(asset, amount);
			if (res) {
				return TransactionProcessingStatus::SUCCESS;
			} else {
				return TransactionProcessingStatus::INSUFFICIENT_BALANCE;
			}
		}
	}
	/*auto it = new_accounts.find(account);

	if (it != new_accounts.end()) {
		//newly created acct local to this db view
		return (*it).second.second.conditional_escrow(asset, amount);
	}*/
	/*if (account >= db_size) {
		TX_INFO("escrowing %ld units of %lu from local account_db_idx %lu", 
			amount, asset, account);

		//account doesn't exist in committed db, but it might
		// have been provisionally
		//created in this view.
		//if so, then the account_db_idx would be greater than db_size and must
		//be present in new_accounts.
		bool status = new_accounts.find(account)
			->second.second.conditional_escrow(asset, amount);
		if (status) {
			return TransactionProcessingStatus::SUCCESS;
		} else {
			return TransactionProcessingStatus::INSUFFICIENT_BALANCE;
		}
	}*/
	TX_INFO("escrowing %ld units of %lu from existing account_db_idx %lu", 
		amount, asset, account);

	auto& view = get_existing_account(account);

	auto status = view.conditional_escrow(asset, amount);

	TX_F(
		if (status != TransactionProcessingStatus::SUCCESS) {
			auto avail = view.lookup_available_balance(asset);
			TX("avail was %lu, request was %lu", avail, amount);
		}

	);

	return status;
}

TransactionProcessingStatus 
BufferedMemoryDatabaseView::transfer_available(
	UserAccount* account, AssetID asset, int64_t amount) {
	
	//auto it = new_accounts.find(account);

	for (auto& it : new_accounts) {
		if (&(it.second) == account) {
			// using direct ptr method because account is not yet in main memdb
			auto res = account ->conditional_transfer_available(asset, amount);

			if (res) {
				return TransactionProcessingStatus::SUCCESS;
			} else {
				return TransactionProcessingStatus::INSUFFICIENT_BALANCE;
			}
		}
	}
/*
	if (account >= db_size) {
		bool status = new_accounts.find(account)
			->second.second.conditional_transfer_available(asset, amount);
		if (status) {
			return TransactionProcessingStatus::SUCCESS;
		} else {
			throw std::runtime_error(
				"we passed an index out of db view that doesn't actually exist!\
				 error in memory_database_view.cc");
			// this branch is only called when the account idx "account"
			// refers to a newly created account
			// made in another thread.
			// in fact this branch should never happen
		}
	}*/

	auto& view = get_existing_account(account);
	return view.transfer_available(asset, amount);
}

UserAccountView& 
BufferedMemoryDatabaseView::get_existing_account(UserAccount* account) {
	auto iter = accounts.find(account);
	if (iter != accounts.end()) {
		return iter->second;
	}
	if (account == nullptr) {
		throw std::runtime_error("cant' deref null account ptr");
	}
	accounts.emplace(account, *account);//main_db.find_account(account));
	return accounts.at(account);
}

void 
BufferedMemoryDatabaseView::commit() {
	for(auto iter = accounts.begin(); 
		iter != accounts.end(); 
		iter++) {
		iter->second.commit();
	}
	AccountCreationView::commit();
}

void 
BufferedMemoryDatabaseView::unwind() {
	for(auto iter = accounts.begin(); 
		iter != accounts.end(); 
		iter++) {
		iter->second.unwind();
	}
	for (auto iter = new_accounts.begin(); iter != new_accounts.end(); iter++) {
		main_db.release_account_creation(iter->first);
	}
}

TransactionProcessingStatus 
UnbufferedMemoryDatabaseView::escrow(
	UserAccount* account, AssetID asset, int64_t amount) {

	account -> escrow(asset, amount);

	/*if (account < db_size) {
		main_db.escrow(account, asset, amount);
	} else {
		new_accounts.find(account)->second.second.escrow(asset, amount);
	}*/
	return TransactionProcessingStatus::SUCCESS;
}
TransactionProcessingStatus 
UnbufferedMemoryDatabaseView::transfer_available(
	UserAccount* account, AssetID asset, int64_t amount) {
	
	account -> transfer_available(asset, amount);
	/*if (account < db_size) {
		main_db.transfer_available(account, asset, amount);
	} else {
		new_accounts.find(account)
			->second.second.transfer_available(asset, amount);
	} */
	return TransactionProcessingStatus::SUCCESS;
}


UserAccount*
AccountCreationView::lookup_user(AccountID account) {
	auto* main_acct = main_db.lookup_user(account);
	if (main_acct != nullptr) {
		return main_acct;
	}
	
	auto iter = temporary_idxs.find(account);
	if (iter == temporary_idxs.end()) {
		return nullptr;
	}
	return iter->second;

}
/*
bool 
AccountCreationView::lookup_user_id(
	AccountID account, account_db_idx* index_out) {
	if (main_db.lookup_user_id(account, index_out)) {
		return true;
	}

	auto iter = temporary_idxs.find(account);
	if (iter == temporary_idxs.end()) {
		return false;
	}
	*index_out = iter->second;
	return true;
}
 */

TransactionProcessingStatus 
AccountCreationView::create_new_account(
	AccountID account, const PublicKey pk, UserAccount** out) {
	auto status = main_db.reserve_account_creation(account);
	if (status != TransactionProcessingStatus::SUCCESS) {
		return status;
	}

	auto it = new_accounts.insert_after(new_accounts.before_begin(), {account, UserAccount(account, pk)});

	//account_db_idx new_idx = temporary_idxs.size() + db_size;

	*out = &((*it).second);
	temporary_idxs.emplace(account, *out);
	/*new_accounts.emplace(std::piecewise_construct,
		std::forward_as_tuple(new_idx),
		std::forward_as_tuple(std::piecewise_construct,
			std::forward_as_tuple(account), 
			std::forward_as_tuple(account, pk)));
	*out = new_idx;*/
	return TransactionProcessingStatus::SUCCESS;
}

void 
AccountCreationView::commit() {
	for (auto iter = new_accounts.begin(); iter != new_accounts.end(); iter++) {
		main_db.commit_account_creation(
			iter->first, std::move(iter->second));
	}
}
/*
TransactionProcessingStatus 
UnlimitedMoneyBufferedMemoryDatabaseView::escrow(
	account_db_idx account, AssetID asset, int64_t amount) {

	if (amount > 0) {
		if (account >= db_size) {
			new_accounts.find(account)
				->second.second.transfer_available(asset, amount);
		} else {
			get_existing_account(account).transfer_available(asset, amount);
		}
	}

	return BufferedMemoryDatabaseView::escrow(account, asset, amount);
}

TransactionProcessingStatus 
UnlimitedMoneyBufferedMemoryDatabaseView::transfer_available(
	account_db_idx account, AssetID asset, int64_t amount) {
	
	if (amount < 0) {
		if (account >= db_size) {
			new_accounts.find(account)
				->second.second.transfer_available(asset, -amount);
		} else {
			get_existing_account(account).transfer_available(asset, -amount);
		}
	}

	return BufferedMemoryDatabaseView::transfer_available(
		account, asset, amount);
} */

} /* speedex */
