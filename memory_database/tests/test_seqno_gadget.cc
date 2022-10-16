#include <catch2/catch_test_macros.hpp>

#include "memory_database/sequence_tracker.h"
#include "xdr/transaction.h"

#include "test_utils/formatting.h"

namespace speedex
{
using test::make_seqno;

TEST_CASE("case that caused error", "[seqno]")
{
	detail::UInt64SequenceTracker tracker(make_seqno(66));

	REQUIRE(tracker.reserve_sequence_number(make_seqno(67)) == TransactionProcessingStatus::SUCCESS);
	REQUIRE(tracker.reserve_sequence_number(make_seqno(68)) == TransactionProcessingStatus::SUCCESS);

	tracker.release_sequence_number(make_seqno(68));

	REQUIRE(tracker.tentative_commitment() == make_seqno(67));

	tracker.commit();

	REQUIRE(tracker.produce_commitment() == make_seqno(67));

}

TEST_CASE("uint64 release seqno", "[seqno]")
{
	detail::UInt64SequenceTracker tracker(make_seqno(100));

	REQUIRE(tracker.reserve_sequence_number(make_seqno(101)) == TransactionProcessingStatus::SUCCESS);

	SECTION("wasn't reserved too low")
	{
		REQUIRE_THROWS(tracker.release_sequence_number(make_seqno(100)));
	}
	SECTION("even lower")
	{
		REQUIRE_THROWS(tracker.release_sequence_number(make_seqno(99)));
	}

	REQUIRE(tracker.tentative_commitment() == make_seqno(101));

	tracker.release_sequence_number(make_seqno(101));

	REQUIRE(tracker.tentative_commitment() == make_seqno(100));

	REQUIRE(tracker.reserve_sequence_number(make_seqno(101)) == TransactionProcessingStatus::SUCCESS);

	tracker.commit();

	REQUIRE(tracker.produce_commitment() == make_seqno(101));
}

TEST_CASE("uint64 release different seqno", "[seqno]")
{
	detail::UInt64SequenceTracker tracker(make_seqno(100));

}

TEST_CASE("uint64 seqno", "[seqno]")
{
	detail::UInt64SequenceTracker tracker(make_seqno(100));

	REQUIRE(tracker.reserve_sequence_number(make_seqno(101)) == TransactionProcessingStatus::SUCCESS);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(164)) == TransactionProcessingStatus::SUCCESS);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(165)) == TransactionProcessingStatus::SEQ_NUM_TOO_HIGH);
	REQUIRE(tracker.reserve_sequence_number(make_seqno(99)) == TransactionProcessingStatus::SEQ_NUM_TOO_LOW);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(101)) == TransactionProcessingStatus::SEQ_NUM_TEMP_IN_USE);

	REQUIRE(tracker.produce_commitment() == make_seqno(100));
	REQUIRE(tracker.tentative_commitment() == make_seqno(164));

	SECTION("commit")
	{
		tracker.commit();
		REQUIRE(tracker.produce_commitment() == make_seqno(164));
	}
	SECTION("rollback")
	{
		tracker.rollback();

		REQUIRE(tracker.produce_commitment() == make_seqno(100));
		REQUIRE(tracker.tentative_commitment() == make_seqno(100));
	}
}

TEST_CASE("bounded seqno", "[seqno]")
{
	detail::BoundedSequenceTracker<256> tracker(make_seqno(100));

	REQUIRE(tracker.reserve_sequence_number(make_seqno(101)) == TransactionProcessingStatus::SUCCESS);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(164)) == TransactionProcessingStatus::SUCCESS);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(165)) == TransactionProcessingStatus::SUCCESS);
	REQUIRE(tracker.reserve_sequence_number(make_seqno(356)) == TransactionProcessingStatus::SUCCESS);
	REQUIRE(tracker.reserve_sequence_number(make_seqno(357)) == TransactionProcessingStatus::SEQ_NUM_TOO_HIGH);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(99)) == TransactionProcessingStatus::SEQ_NUM_TOO_LOW);

	REQUIRE(tracker.reserve_sequence_number(make_seqno(101)) == TransactionProcessingStatus::SEQ_NUM_TEMP_IN_USE);

	REQUIRE(tracker.produce_commitment() == make_seqno(100));
	REQUIRE(tracker.tentative_commitment() == make_seqno(356));

	SECTION("commit")
	{
		tracker.commit();
		REQUIRE(tracker.produce_commitment() == make_seqno(356));
	}
	SECTION("rollback")
	{
		tracker.rollback();

		REQUIRE(tracker.produce_commitment() == make_seqno(100));
		REQUIRE(tracker.tentative_commitment() == make_seqno(100));
	}
}

} // speedex