#include <cxxtest/TestSuite.h>

#include <cstdint>

#include "utils/big_endian.h"
#include "utils/manage_data_dirs.h"

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

	void test_good_insert() {
		BlockHeaderHashMap map;

		Hash buf;

		uint64_t value = 0x1234;
		write_unsigned_big_endian(buf, value);

		map.insert(1, buf);

		Hash hash1;
		map.hash(hash1);

		value = 0x2341;
		write_unsigned_big_endian(buf, value);
		map.insert(2, buf);
		Hash hash2;
		map.hash(hash2);

		TS_ASSERT_DIFFERS(hash1, hash2);
	}

	void test_bad_first_insert() {
		BlockHeaderHashMap map;
		Hash hash;

		TS_ASSERT_THROWS_ANYTHING(map.insert(2, hash));
	}

	void test_mixed_inserts() {

		BlockHeaderHashMap map;
		Hash buf;

		uint64_t value = 0x1234;
		write_unsigned_big_endian(buf, value);
		map.insert(1, buf);

		Hash hash1;
		map.hash(hash1);

		TS_ASSERT_THROWS_ANYTHING(map.insert(1, buf));
	}

	void test_lmdb_persist() {
		BlockHeaderHashMap map;
		Hash buf;

		map.open_lmdb_env();
		map.create_lmdb();

		uint64_t value = 0x1234;
		write_unsigned_big_endian(buf, value);
		map.insert(1, buf);

		value = 0x1235;
		write_unsigned_big_endian(buf, value);
		map.insert(2, buf);

		map.persist_lmdb(0);
		map.persist_lmdb(1);
		map.persist_lmdb(2);

		TS_ASSERT_THROWS_ANYTHING(map.persist_lmdb(3));
	}

	void test_lmdb_rollback() {
		BlockHeaderHashMap map;
		Hash buf;

		map.open_lmdb_env();
		map.create_lmdb();

		uint64_t value = 0x1234;
		write_unsigned_big_endian(buf, value);
		map.insert(1, buf);

		Hash hash1;
		map.hash(hash1);

		value = 0x1235;
		write_unsigned_big_endian(buf, value);
		map.insert(2, buf);

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
		{
			BlockHeaderHashMap map;
			Hash buf;

			map.open_lmdb_env();
			map.create_lmdb();

			uint64_t value = 0x1234;
			write_unsigned_big_endian(buf, value);
			map.insert(1, buf);

			Hash hash1;
			map.hash(hash1);

			value = 0x1235;
			write_unsigned_big_endian(buf, value);
			map.insert(2, buf);

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

		uint64_t recalled_value = 0x1235;
		Hash buf;
		write_unsigned_big_endian(buf, recalled_value);

		auto test_recall = map.get_hash(2);

		TS_ASSERT((bool)test_recall);
		TS_ASSERT_EQUALS(*test_recall, buf);

		TS_ASSERT_THROWS_ANYTHING(map.insert(2, first_hash));
	}
};
