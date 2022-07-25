#include "filtering/filter_log.h"

#include <stdexcept>

namespace speedex {

void
FilterLog::add_txs(std::vector<SignedTransaction> const& txs, MemoryDatabase const& db)
{
	serial_cache_t cache;

	auto serial_add_lambda = [&cache, &txs, &db, this] (tbb::blocked_range<size_t> r)
	{
		auto& local_log = cache.get(entries);

		for (size_t i = r.begin(); i < r.end(); i++)
		{
			auto const& tx = txs[i];
			local_log.template insert<AccountFilterInsertFn>(tx.transaction.metadata.sourceAccount, std::make_pair(tx, &db));
		}
	};

	tbb::blocked_range<size_t> r(0, txs.size());
	tbb::parallel_for(
		r,
		serial_add_lambda);

	entries.template batch_merge_in<AccountFilterMergeFn>(cache);


	auto validate_lambda = [&db, this] (AccountFilterEntry& entry)
	{
		entry.compute_validity(db);
	};
	entries.parallel_apply(validate_lambda);
}

bool 
FilterLog::check_valid_account(AccountID const& account) const
{
	const auto* res = entries.get_value(account);
	if (!res) {
		return false;
	}
	return res -> check_valid();
}



} /* speedex */
