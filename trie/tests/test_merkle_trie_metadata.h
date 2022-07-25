#include <cxxtest/TestSuite.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "trie/merkle_trie.h"

#include "xdr/transaction.h"

#include "utils/big_endian.h"
#include "utils/debug_macros.h"

#include "xdr/types.h"

using namespace speedex;

using xdr::operator==;

class MerkleTrieMetadataTestSuite : public CxxTest::TestSuite {
public:
	void test_size_insert() {
		TEST_START();
		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin>>;
		mt trie;
		mt :: prefix_t key_buf;
		for (uint16_t i = 0; i < 1000; i+= 20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50u, trie.uncached_size());
		TS_ASSERT_EQUALS(50u, trie.size());

		TS_ASSERT(trie.metadata_integrity_check());
	}

	void test_size_merge() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin>>;

		mt trie;
		mt :: prefix_t key_buf;
		for (uint16_t i = 0; i < 80; i+=40) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		mt trie2;
		for (uint16_t i = 0; i < 80; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie2.insert(key_buf);
		}
		TS_ASSERT_EQUALS(2u, trie.size());

		trie.merge_in(std::move(trie2));

		TS_ASSERT_EQUALS(4u, trie.uncached_size());
		TS_ASSERT_EQUALS(4u, trie.size());

		TS_ASSERT(trie.metadata_integrity_check());
	}

	void test_size_merge_larger() {
		TEST_START();
		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin>>;

		mt trie;
		mt :: prefix_t key_buf;
		for (uint16_t i = 0; i < 1000; i+=40) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		mt trie2;
		
		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie2.insert(key_buf);
		}
		TS_ASSERT_EQUALS(25u, trie.size());

		trie.merge_in(std::move(trie2));

		TS_ASSERT_EQUALS(50u, trie.uncached_size());
		TS_ASSERT_EQUALS(50u, trie.size());

		TS_ASSERT(trie.metadata_integrity_check());
	}

	void test_size_delete_direct() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin>>;

		mt trie;
		mt :: prefix_t key_buf;
		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50u, trie.size());
		TS_ASSERT_EQUALS(50u, trie.uncached_size());

		for (uint16_t i = 0; i < 1000; i += 40) {
			write_unsigned_big_endian(key_buf, i);
			TS_ASSERT(trie.perform_deletion(key_buf));
		}

		TS_ASSERT_EQUALS(25u, trie.uncached_size());
		TS_ASSERT_EQUALS(25u, trie.size());

		TS_ASSERT(trie.metadata_integrity_check());
	}

	void test_size_delete_marked() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin>>;

		mt trie;
		mt :: prefix_t key_buf;

		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50u, trie.size());
		TS_ASSERT_EQUALS(50u, trie.uncached_size());

		for (uint16_t i = 0; i < 1000; i += 40) {
			write_unsigned_big_endian(key_buf, i);
			TS_ASSERT(trie.mark_for_deletion(key_buf));
		}
		TS_ASSERT_EQUALS(50u, trie.size());

		trie.perform_marked_deletions();
		TS_ASSERT_EQUALS(25u, trie.size());

		TS_ASSERT(trie.metadata_integrity_check());
	}

	void test_ignore_deleted_subnode_hashes() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin>>;

		mt trie;
		mt :: prefix_t key_buf;

		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50u, trie.size());
		TS_ASSERT_EQUALS(50u, trie.uncached_size());
		
		Hash hash_buf_1, hash_buf_2, hash_buf_3;

		//unsigned char hash_buf_1[32];

		trie.hash(hash_buf_1);

		for (uint16_t i = 0; i < 1000; i += 40) {
			write_unsigned_big_endian(key_buf, i);
			TS_ASSERT(trie.mark_for_deletion(key_buf));
		}
//	unsigned char hash_buf_2[32];
		trie.hash(hash_buf_2);

		trie.perform_marked_deletions();

		//unsigned char hash_buf_3[32];
		trie.hash(hash_buf_3);

		TS_ASSERT_DIFFERS(hash_buf_1, hash_buf_2);
		TS_ASSERT_EQUALS(hash_buf_2, hash_buf_3);

		//TS_ASSERT(memcmp(hash_buf_1.data(), hash_buf_2.data(), 32) != 0);
	//	TS_ASSERT(memcmp(hash_buf_2.data(), hash_buf_3.data(), 32) == 0);
	}

	void test_size_delete_mark_unmark() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin>>;

		mt trie;
		mt :: prefix_t key_buf;
		
		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		for (uint16_t i = 0; i < 1000; i+=40) {
			write_unsigned_big_endian(key_buf, i);
			trie.mark_for_deletion(key_buf);
			uint16_t j = i + 20;
			write_unsigned_big_endian(key_buf, j);
			trie.unmark_for_deletion(key_buf);
		}

		for (uint16_t i = 0; i < 1000; i+=80) {
			write_unsigned_big_endian(key_buf, i);
			trie.unmark_for_deletion(key_buf);
		}

		TS_ASSERT_EQUALS(50u, trie.size());
		trie.perform_marked_deletions();
		TS_ASSERT_EQUALS(38u, trie.size());

		TS_ASSERT(trie.metadata_integrity_check());

	}

/*	void _test_delete_lt_key() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin>>;

		mt trie;
		mt :: prefix_t key_buf;
		
		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50, trie.size());

		uint16_t threshold = 0;

		write_unsigned_big_endian(key_buf, threshold);
		trie.mark_subtree_lt_key_for_deletion(key_buf);

		TS_ASSERT_EQUALS(trie.num_deleted_subnodes(), 0);

		threshold = 200;

		write_unsigned_big_endian(key_buf, threshold);

		trie.mark_subtree_lt_key_for_deletion(key_buf);
		TS_ASSERT_EQUALS(trie.num_deleted_subnodes(), 10);
	}

	void _test_delete_lt_key_long() {
		TEST_START();
		using mt = MerkleTrie<ByteArrayPrefix<8>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin>>;

		mt trie;
		mt :: prefix_t buf;

		uint64_t i = 0xFF00FF00;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);

		i = 0xFF00FF10;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);

		i = 0xFF00FF30;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);


		uint64_t threshold = 0xFF00FF10;
		write_unsigned_big_endian(buf, threshold);
		trie.mark_subtree_lt_key_for_deletion(buf);
		TS_ASSERT_EQUALS(trie.num_deleted_subnodes(), 1);


		i = 0xF0000000;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);


		i = 0xF0000001;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);

		i = 0xFE000000;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);

		i = 0xFE000001;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);



		trie.clear_marked_deletions();

		threshold = 0xFD000000;
		write_unsigned_big_endian(buf, threshold);
		trie.mark_subtree_lt_key_for_deletion(buf);

		TS_ASSERT_EQUALS(trie.num_deleted_subnodes(), 2);
	}

	void _test_delete_lt_key_long_weirdcase() {
		TEST_START();
		//proves that "the impossible happened" case actually can happen reasonably simply.

		using mt = MerkleTrie<ByteArrayPrefix<8>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin>>;

		mt trie;
		mt :: prefix_t buf;

		uint64_t i = 0xFF000001;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);

		i = 0xFF000000;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);
		i = 0xFF000002;
		write_unsigned_big_endian(buf, i);
		trie.insert(buf);

		uint64_t threshold = 0x0000F000;
		write_unsigned_big_endian(buf, threshold);
		trie.mark_subtree_lt_key_for_deletion(buf);
		TS_ASSERT_EQUALS(trie.num_deleted_subnodes(), 0);
	} */


	void test_rollback_disjoint() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin, RollbackMixin>>;

		mt trie;
		mt :: prefix_t key_buf;

		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		for (uint16_t i = 10; i < 1010; i += 20) {
			write_unsigned_big_endian(key_buf, i);
			trie.template insert<RollbackInsertFn<EmptyValue>>(key_buf);
		}

		TS_ASSERT_EQUALS(100u, trie.size());

		trie.do_rollback();

		TS_ASSERT_EQUALS(50u, trie.uncached_size());
		TS_ASSERT_EQUALS(50u, trie.size());

	}

	void test_clear_rollback_parallel() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<4>, EmptyValue, CombinedMetadata<SizeMixin, DeletableMixin, RollbackMixin>>;

		mt trie;
		mt :: prefix_t key_buf;
		
		for (uint32_t i = 0; i < 10000; i+=207) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}


		for (uint16_t i = 10; i < 10100; i += 207) {
			write_unsigned_big_endian(key_buf, i);
			trie.template insert<RollbackInsertFn<EmptyValue>>(key_buf);
		}
		
		TS_ASSERT_EQUALS(98u, trie.size());

		trie.clear_rollback();

		TS_ASSERT_EQUALS(98u, trie.size());
		TS_ASSERT_EQUALS(98u, trie.uncached_size());
		TS_ASSERT(trie.metadata_integrity_check());
	} 
};