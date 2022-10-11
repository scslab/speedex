#include <catch2/catch_test_macros.hpp>


#include "modlog/account_modification_log.h"

#include "xdr/transaction.h"


namespace speedex
{

using xdr::operator==;

void add_tx(SignedTransactionList& list, const SignedTransaction& tx)
{
	list.push_back(tx);
}

void add_tx(AccountModificationBlock& list, const SignedTransaction& tx)
{
	for (auto& l : list)
	{
		if (l.owner == tx.transaction.metadata.sourceAccount)
		{
			l.new_transactions_self.push_back(tx);
		}
	}
}


TEST_CASE("tx accumulate", "[modlog]")
{
	AccountModificationLog log;

	AccountModificationBlock expect;
	{
		SerialAccountModificationLog s(log);

		SignedTransaction tx;
		tx.transaction.metadata.sourceAccount = 0x0000;
		tx.transaction.metadata.sequenceNumber = 1llu << 8;

		s.log_new_self_transaction(tx);
		add_tx(expect, tx);
	}

	SECTION("one tx")
	{
		log.merge_in_log_batch();

		AccountModificationBlock result;
		log.parallel_accumulate_values(result);

		REQUIRE(result == expect);

		log.test_metadata_integrity();
	}
	SECTION("second tx, same account")
	{
		{
			SerialAccountModificationLog s(log);

			SignedTransaction tx;
			tx.transaction.metadata.sourceAccount = 0x0000;
			tx.transaction.metadata.sequenceNumber = 2llu << 8;

			s.log_new_self_transaction(tx);

			add_tx(expect, tx);
		}

		log.merge_in_log_batch();

		AccountModificationBlock result;
		log.parallel_accumulate_values(result);

		REQUIRE(result == expect);

		log.test_metadata_integrity();
	}
}

}