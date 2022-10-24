#include <catch2/catch_test_macros.hpp>

#include "crypto/crypto_utils.h"
#include "memory_database/memory_database.h"

#include "filtering/account_filter_entry.h"
#include "filtering/filter_log.h"

#include "xdr/types.h"

namespace speedex
{

void make_pks(MemoryDatabaseGenesisData& data)
{
	DeterministicKeyGenerator key_gen;

	data.pk_list.resize(data.id_list.size());
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, data.id_list.size()),
		[&key_gen, &data](auto r) {
			for (size_t i = r.begin(); i < r.end(); i++) {
				data.pk_list[i] = key_gen.deterministic_key_gen(data.id_list[i]).second;
			}
		});
}

Operation make_payment_op(AccountID const& to, AssetID asset, int64_t amount)
{
	Operation out;
	out.body.type(PAYMENT);
	out.body.paymentOp().receiver = to;
	out.body.paymentOp().asset = asset;
	out.body.paymentOp().amount = amount;
	return out;
}

SignedTransaction
make_payment_tx(AccountID const& from, uint64_t seqno, uint64_t fee, AccountID const& to, AssetID asset, int64_t amount)
{
	DeterministicKeyGenerator key_gen;
	auto sk = key_gen.deterministic_key_gen(from).first;

	SignedTransaction out;
	out.transaction.operations.push_back(make_payment_op(to, asset, amount));
	out.transaction.metadata.sourceAccount = from;
	out.transaction.metadata.sequenceNumber = seqno;
	out.transaction.maxFee = fee;
	sign_transaction(out, sk);

	return out;
}

SignedTransaction
make_empty_tx(AccountID const& from, uint64_t seqno, uint64_t fee)
{
	DeterministicKeyGenerator key_gen;
	auto sk = key_gen.deterministic_key_gen(from).first;

	SignedTransaction out;
	out.transaction.metadata.sourceAccount = from;
	out.transaction.metadata.sequenceNumber = seqno;
	out.transaction.maxFee = fee;
	sign_transaction(out, sk);

	return out;
}

TEST_CASE("single account", "[filtering]")
{
	const AccountID id = 0x1234;
	AccountFilterEntry entry(id);

	AccountCreationFilter acf;
	MemoryDatabase db;
	MemoryDatabaseGenesisData memdb_genesis;
	memdb_genesis.id_list.push_back(id);

	make_pks(memdb_genesis);
	int64_t default_amount = 10;
	size_t num_assets = 5;

	uint64_t initial_seqno = 50 * 256;

	auto account_init_lambda = [&] (UserAccount& user_account) -> void {
		for (auto i = 0u; i < num_assets; i++) {
			db.transfer_available(&user_account, i, default_amount);
		}
		REQUIRE(user_account.reserve_sequence_number(initial_seqno) == TransactionProcessingStatus::SUCCESS);
		user_account.commit_sequence_number(initial_seqno);
		user_account.commit();
	};

	db.install_initial_accounts_and_commit(memdb_genesis, account_init_lambda);

	SECTION("uncomputed, throws")
	{
		REQUIRE_THROWS(entry.check_valid());
	}

	SECTION("no txs, is valid")
	{
		entry.compute_validity(db, acf);
		//misnomer here, valid_no_txs is when there's no txs so accountfilterentry doesn't exist
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}

	SECTION("empty tx, just fee (payable)")
	{
		auto tx = make_empty_tx(id, initial_seqno + 10 * 256, 10);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}
	SECTION("empty tx, just fee (not payable)")
	{
		auto tx = make_empty_tx(id, initial_seqno + 10 * 256, 100000);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::MISSING_REQUIREMENT);
	}
	SECTION("empty tx, just fee (bad seqno)")
	{
		auto tx = make_empty_tx(id, initial_seqno - 10 * 256, 10);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}
	SECTION("empty tx, just fee (bad seqno, bad fee ignored)")
	{
		auto tx = make_empty_tx(id, initial_seqno - 10 * 256, 10000000);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}
	SECTION("payment tx good")
	{
		auto tx = make_payment_tx(id, initial_seqno + 10 * 256, 10, id, 1, 10);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}	
	SECTION("payment tx bad")
	{
		auto tx = make_payment_tx(id, initial_seqno + 10 * 256, 10, id, 1, 11);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::MISSING_REQUIREMENT);
	}
	SECTION("payment tx bad conflict with fee")
	{
		auto tx = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 0, 6);
		entry.add_tx(tx, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::MISSING_REQUIREMENT);
	}
	SECTION("two payment tx good")
	{
		auto tx1 = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 1, 6);
		auto tx2 = make_payment_tx(id, initial_seqno + 11 * 256, 5, id, 2, 6);
		entry.add_tx(tx1, db);
		entry.add_tx(tx2, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}
	SECTION("two payment tx bad")
	{
		auto tx1 = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 1, 6);
		auto tx2 = make_payment_tx(id, initial_seqno + 11 * 256, 5, id, 1, 6);
		entry.add_tx(tx1, db);
		entry.add_tx(tx2, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::MISSING_REQUIREMENT);
	}
	SECTION("same seqno fail")
	{
		auto tx1 = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 1, 1);
		auto tx2 = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 2, 1);
		entry.add_tx(tx1, db);
		entry.add_tx(tx2, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::INVALID_DUPLICATE);
	}
	SECTION("same seqno duplicate ok")
	{
		auto tx1 = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 1, 10);
		auto tx2 = make_payment_tx(id, initial_seqno + 10 * 256, 5, id, 1, 10);
		entry.add_tx(tx1, db);
		entry.add_tx(tx2, db);
		entry.compute_validity(db, acf);
		REQUIRE(entry.check_valid() == FilterResult::VALID_HAS_TXS);
	}
}

}
