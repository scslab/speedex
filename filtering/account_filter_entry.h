#pragma once

#include "xdr/transaction.h"
#include "xdr/types.h"

#include "trie/prefix.h"

#include <cstdint>
#include <map>

namespace speedex
{

class MemoryDatabase;

class AccountFilterEntry
{
	AccountID account;
	uint64_t min_seq_no;
	bool initialized;

	std::map<uint64_t, SignedTransaction> txs;

	std::map<AssetID, int64_t> required_assets;

	bool found_error = false;
	bool checked_reqs_cached = false;

	void add_req(AssetID const& asset, int64_t amount);
	void log_reqs_invalid();
	void log_reqs_valid();

	void compute_reqs();

	void assert_initialized() const;

public:

	AccountFilterEntry()
		: account(0)
		, min_seq_no(UINT64_MAX)
		, initialized(false)
		, txs()
		, required_assets()
		{}

	AccountFilterEntry(AccountID account);

	void add_tx(SignedTransaction const& tx, MemoryDatabase const& db);

	void compute_validity(MemoryDatabase const& db);

	void merge_in(AccountFilterEntry& other);

	bool check_valid() const;
};

struct AccountFilterInsertFn
{
	static AccountFilterEntry 
	new_value(AccountIDPrefix const& prefix)
	{
		try {
		return AccountFilterEntry(prefix.get_account());
		} 
		catch(...)
		{
			std::printf("wat2\n");
			std::fflush(stdout);
			throw;
		}
	}

	static void 
	value_insert(AccountFilterEntry& to_modify, std::pair<SignedTransaction, const MemoryDatabase*> const& inserted)
	{
		try{
			to_modify.add_tx(inserted.first, *(inserted.second));
		} 
		catch(...)
		{
			std::printf("wat\n");
			std::fflush(stdout);
			throw;
		}
	}
};

struct AccountFilterMergeFn
{
	static void
	value_merge(AccountFilterEntry& into, AccountFilterEntry& from)
	{
		into.merge_in(from);
	}
};

} /* speedex */
