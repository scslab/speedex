#include <cxxtest/TestSuite.h>

#include "hotstuff/vm/speculative_exec_gadget.h"

#include <cstdint>

using namespace hotstuff;

class SpeculativeExecGadgetTestSuite : public CxxTest::TestSuite {
public:
	void test_sequential_success()
	{
		SpeculativeExecGadget<uint64_t> gadget;

		gadget.add_height_pair(1, {0});
		gadget.add_height_pair(2, {1});
		gadget.add_height_pair(3, {2});

		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(1), 0u);
		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(2), 1u);
		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(3), 2u);

		gadget.add_height_pair(4, {3});
		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(4), 3u);
	}

	void test_get_lowest()
	{
		SpeculativeExecGadget<uint64_t> gadget;

		gadget.add_height_pair(1, 0);
		gadget.add_height_pair(2, 1);
		gadget.add_height_pair(3, 2);

		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(1), 0u);
		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(2), 1u);

		gadget.add_height_pair(4, 3);

		TS_ASSERT_EQUALS(gadget.get_lowest_speculative_hotstuff_height().second, 2u);
	}

	void test_hs_gap()
	{
		SpeculativeExecGadget<uint64_t> gadget;
		gadget.add_height_pair(5, 0);
		gadget.add_height_pair(10, 1);

		TS_ASSERT_THROWS_ANYTHING(gadget.get_lowest_speculative_hotstuff_height());

		TS_ASSERT_THROWS_ANYTHING(gadget.on_commit_hotstuff(5));
	}

	void test_revert()
	{
		SpeculativeExecGadget<uint64_t> gadget;
		gadget.add_height_pair(1, 100);
		gadget.add_height_pair(2, 101);
		gadget.on_commit_hotstuff(1);

		gadget.clear();

		gadget.add_height_pair(2, 102);
		TS_ASSERT_EQUALS(gadget.on_commit_hotstuff(2), 102u);
	}

};
