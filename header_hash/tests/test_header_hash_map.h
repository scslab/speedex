#include <cxxtest/TestSuite.h>

#include <cstdint>

#include "utils/big_endian.h"
#include "utils/manage_data_dirs.h"
#include "utils/hash.h"

#include "header_hash/block_header_hash_map.h"

#include "xdr/types.h"

using namespace speedex;

using xdr::operator==;

class HeaderHashTestSuite : public CxxTest::TestSuite {
public:

	void setUp() {
		clear_header_hash_lmdb_dir();
		make_header_hash_lmdb_dir();
	}

	void tearDown() {
		clear_header_hash_lmdb_dir();
	}

	Block make_block(uint64_t value, uint64_t block_number) {
		Block buf;
		write_unsigned_big_endian(buf.prevBlockHash, value);
		buf.blockNumber = block_number;
		return buf;
	}

	void test_good_insert() {
		BlockHeaderHashMap map;

		map.insert(make_block(0x1234, 1), true);

		Hash hash1;
		map.hash(hash1);

		map.insert(make_block(0x2341, 2), true);
		
		Hash hash2;
		map.hash(hash2);

		TS_ASSERT_DIFFERS(hash1, hash2);
	}

	void test_bad_first_insert() {
		BlockHeaderHashMap map;
		Hash hash;

		TS_ASSERT_THROWS_ANYTHING(map.insert(make_block(0, 2), true));
	}

	void test_mixed_inserts() {

		BlockHeaderHashMap map;

		map.insert(make_block(0x1234, 1), true);

		Hash hash1;
		map.hash(hash1);

		TS_ASSERT_THROWS_ANYTHING(map.insert(make_block(0x2341, 1), true));
	}

	void test_lmdb_persist() {
		BlockHeaderHashMap map;

		map.open_lmdb_env();
		map.create_lmdb();

		map.insert(make_block(0x1234, 1), true);

		map.insert(make_block(0x1235, 2), true);

		map.persist_lmdb(0);
		map.persist_lmdb(1);
		map.persist_lmdb(2);

		TS_ASSERT_THROWS_ANYTHING(map.persist_lmdb(3));
	}

	void test_lmdb_rollback() {
		BlockHeaderHashMap map;

		map.open_lmdb_env();
		map.create_lmdb();

		map.insert(make_block(0x1234, 1), true);

		Hash hash1;
		map.hash(hash1);

		map.insert(make_block(0x1235, 2), true);

		Hash hash3;
		map.hash(hash3);

		map.persist_lmdb(1);
		map.rollback_to_committed_round(1);

		Hash hash2;
		map.hash(hash2);

		TS_ASSERT_EQUALS(hash1, hash2);
		TS_ASSERT_DIFFERS(hash1, hash3);
	}

	void test_lmdb_reload() {
		Hash first_hash;

		Hash round2_recall_hash;
		{
			BlockHeaderHashMap map;
			
			map.open_lmdb_env();
			map.create_lmdb();

			map.insert(make_block(0x1234, 1), true);

			Hash hash1;
			map.hash(hash1);

			Block block2 = make_block(0x1235, 2);

			round2_recall_hash = hash_xdr(block2);

			map.insert(block2, true);

			map.persist_lmdb(2);

			map.hash(first_hash);
		}
		
		BlockHeaderHashMap map;

		map.open_lmdb_env();
		map.open_lmdb();
		map.load_lmdb_contents_to_memory();

		Hash second_hash;
		map.hash(second_hash);

		TS_ASSERT_EQUALS(first_hash, second_hash);

		Block expected_block = make_block(0x1235, 2);
		Hash expected_hash = hash_xdr(expected_block);

		auto test_recall = map.get(2);

		TS_ASSERT((bool)test_recall);
		TS_ASSERT_EQUALS(test_recall -> hash, expected_hash);

		// ensure we don't overwrite persisted data post reload
		TS_ASSERT_THROWS_ANYTHING(map.insert(expected_block, true));
	}
};
