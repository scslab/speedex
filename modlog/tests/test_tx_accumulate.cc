#include <catch2/catch_test_macros.hpp>


#include "modlog/account_modification_log.h"

#include "xdr/transaction.h"


namespace speedex
{

using xdr::operator==;


TEST_CASE("tx accumulate", "[modlog]")
{
	AccountModificationLog log;

	std::vector<SignedTransaction> expect;
	{
		SerialAccountModificationLog s(log);

		SignedTransaction tx;
		tx.transaction.metadata.sourceAccount = 0x0000;
		tx.transaction.metadata.sequenceNumber = 1llu << 8;

		s.log_new_self_transaction(tx);

		expect.push_back(tx);
	}

	SECTION("one tx")
	{
		log.merge_in_log_batch();

		std::vector<SignedTransaction> result;
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

			expect.push_back(tx);
		}

		log.merge_in_log_batch();

		std::vector<SignedTransaction> result;
		log.parallel_accumulate_values(result);

		REQUIRE(result == expect);

		log.test_metadata_integrity();
	}
}

}