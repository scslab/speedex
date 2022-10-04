#include <catch2/catch_test_macros.hpp>

#include "modlog/account_modification_entry.h"
#include "modlog/log_entry_fns.h"
#include "modlog/typedefs.h"

#include "utils/debug_utils.h"

namespace speedex
{

TEST_CASE("compare serialization", "[modlog]")
{
	AccountID owner = 0xAABBCCDD'EEFF0011;

	AccountModificationEntry entry(owner);
	AccountModificationTxListWrapper list;
	list.owner = owner;

	auto require_eq = [&] ()
	{
		std::vector<uint8_t> entry_bytes;
		std::vector<uint8_t> list_bytes;

		LogNormalizeFn::apply_to_value(list);

		entry.copy_data(entry_bytes);
		list.copy_data(list_bytes);

		if (entry_bytes == list_bytes)
		{
			SUCCEED();
			return;
		}

		std::stringstream sstr;

		sstr << "list:  " << debug::array_to_str(list_bytes) << "\nentry: " << debug::array_to_str(entry_bytes) << "\n";

		INFO(sstr.str());
		FAIL();
	};

	auto add_id_self = [&] (uint64_t id)
	{
		LogListInsertFn::value_insert(list, id);
		LogEntryInsertFn::value_insert(entry, id);
	};

	auto add_id_other = [&] (AccountID other_id, uint64_t seqno)
	{
		TxIdentifier id{other_id, seqno};
		LogListInsertFn::value_insert(list, id);
		LogEntryInsertFn::value_insert(entry, id);
	};

	auto add_tx = [&] (SignedTransaction const& tx)
	{
		LogListInsertFn::value_insert(list, tx);
		LogEntryInsertFn::value_insert(entry, tx);
	};

	SECTION("self")
	{
		add_id_self(0x00001111);


		add_id_self(0x00000000);

		add_id_self(0xFFFFFFFF);

		require_eq();
	}
	SECTION("other")
	{
		add_id_other(0x00001111, 0x1234);
		add_id_other(0x00001111, 0x1235);
		add_id_other(0x00001111, 0x0000);
		add_id_other(0, 0);

		require_eq();
	}

	SignedTransaction tx1;
	tx1.transaction.metadata.sequenceNumber = 0x1111;
	SignedTransaction tx2;
	tx2.transaction.metadata.sequenceNumber = 0x1112;

	SECTION("forwards tx")
	{
		add_tx(tx1);
		add_tx(tx2);
		require_eq();
	}

	SECTION("backwards tx")
	{
		add_tx(tx2);
		add_tx(tx1);
		require_eq();
	}
}

}