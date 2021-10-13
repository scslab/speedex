#include <cxxtest/TestSuite.h>

#include "hotstuff/vm/vm_height_map_gadget.h"

#include <cstdint>

using namespace hotstuff;

class HeightMapGadgetTestSuite : public CxxTest::TestSuite {
public:
	void test_sequential_success()
	{
		HeightMapGadget gadget;

		gadget.add_height_pair(1, 0);
		gadget.add_height_pair(2, 1);
		gadget.add_height_pair(3, 2);

		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(1), 0);
		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(2), 1);
		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(3), 2);

		gadget.add_height_pair(4, 3);
		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(4), 3);
	}

	void test_gaps_success()
	{
		HeightMapGadget gadget;

		gadget.add_height_pair(5, 0);
		gadget.add_height_pair(10, 1);
		gadget.add_height_pair(12, 2);

		TS_ASSERT(!gadget.on_commit_hotstuff(1));
		TS_ASSERT(!gadget.on_commit_hotstuff(4));
		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(5), 0);
		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(10), 1);

		gadget.add_height_pair(15, 3);

		TS_ASSERT(!gadget.on_commit_hotstuff(11));
		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(12), 2);
		TS_ASSERT(!gadget.on_commit_hotstuff(14));
	}

	void test_hs_gap()
	{
		HeightMapGadget gadget;
		gadget.add_height_pair(5, 0);
		gadget.add_height_pair(10, 1);

		TS_ASSERT_EQUALS(*gadget.on_commit_hotstuff(5), 0);

		TS_ASSERT_THROWS_ANYTHING(gadget.on_commit_hotstuff(15));
	}

};
