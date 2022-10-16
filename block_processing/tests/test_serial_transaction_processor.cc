#include <catch2/catch_test_macros.hpp>

#include "speedex/speedex_management_structures.h"

#include "block_processing/serial_transaction_processor.h"

#include "test_utils/formatting.h"

#include "utils/transaction_type_formatter.h"

namespace speedex
{

using xdr::operator==;

using test::make_seqno;


TEST_CASE("ensure fee failure unwinds seqno", "[tx]")
{
	//this case caused several days of angst

	uint16_t num_assets = 5;

	SpeedexManagementStructures management_structures(
		num_assets,
		ApproximationParameters{
			.tax_rate = 10,
			.smooth_mult = 10
		},
		SpeedexRuntimeConfigs{
			.check_sigs = false
		});

	auto& db = management_structures.db;

	MemoryDatabaseGenesisData data;
	data.id_list = {0, 1, 2, 3, 4};
	data.pk_list.resize(data.id_list.size());

	auto init_lambda = [&](UserAccount& user)
	{
		// enough for one transaction, not two
		db.transfer_available(&user, 0, 20);
		
		db.transfer_available(&user, 1, 100000);
		user.commit();
	};

	db.install_initial_accounts_and_commit(data, init_lambda);

	SignedTransaction tx1;
	tx1.transaction.metadata.sourceAccount = 0;
	tx1.transaction.metadata.sequenceNumber = make_seqno(1);
	tx1.transaction.operations.push_back(test::make_payment(1, 1, 100));
	tx1.transaction.maxFee = tx_formatter::compute_min_fee(tx1);
	// expect it to be 15 units of 0
	REQUIRE(tx1.transaction.maxFee == 15);

	SignedTransaction tx2;
	tx2.transaction.metadata.sourceAccount = 0;
	tx2.transaction.metadata.sequenceNumber = make_seqno(2);
	tx2.transaction.operations.push_back(test::make_payment(1, 1, 100));
	tx2.transaction.maxFee = tx_formatter::compute_min_fee(tx2);

	SerialTransactionProcessor tx_processor(management_structures);

	BlockStateUpdateStatsWrapper stats;
	SerialAccountModificationLog log(management_structures.account_modification_log);

	REQUIRE(tx_processor.process_transaction(tx1, stats, log) == TransactionProcessingStatus::SUCCESS);
	
	UserAccount* idx = db.lookup_user(0);
	REQUIRE((bool)idx);

	SECTION("one seq bump after one tx")
	{
		auto tentative_commitment = idx -> tentative_commitment();
		auto produce_commitment = idx -> produce_commitment();

		REQUIRE(tentative_commitment.last_committed_id == make_seqno(1));
		REQUIRE(produce_commitment.last_committed_id == make_seqno(0));

		idx -> commit();
		produce_commitment = idx -> produce_commitment();

		REQUIRE(tentative_commitment == produce_commitment);
	}

	SECTION("second tx fails, does not bump seqno")
	{
		REQUIRE(tx_processor.process_transaction(tx2, stats, log) == TransactionProcessingStatus::INSUFFICIENT_BALANCE);

		auto tentative_commitment = idx -> tentative_commitment();
		auto produce_commitment = idx -> produce_commitment();

		REQUIRE(tentative_commitment.last_committed_id == make_seqno(1));
		REQUIRE(produce_commitment.last_committed_id == make_seqno(0));


		idx -> commit();
		produce_commitment = idx -> produce_commitment();

		REQUIRE(tentative_commitment == produce_commitment);

	}








}


}