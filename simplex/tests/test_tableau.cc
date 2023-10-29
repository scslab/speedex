#include <catch2/catch_test_macros.hpp>

#include "xdr/types.h"

#include "simplex/sparse.h"

#include "simplex/allocator.h"

namespace speedex
{


TEST_CASE("tableau tests", "[simplex]")
{
	alloc.clear();
	c_alloc.clear();

	SECTION("test_set")
	{
		SparseTableau tableau(5);

		tableau.add_row();

		tableau.set(0, 3, 1);
		tableau.set(0, 2, -1);

		REQUIRE(tableau.get(0, 2) == -1);

		tableau.rows[0].negate();

		REQUIRE(tableau.get(0, 3) == -1);
	}

	SECTION("test_multirow")
	{
		SparseTableau tableau(5);

		tableau.add_row();
		tableau.rows[0].negate();
		tableau.add_row();
		tableau.rows[1].negate();
		tableau.add_row();

		tableau.set(0, 3, 1);
		tableau.set(0, 1, -1);
		tableau.set(0, 4, 1);

		tableau.integrity_check(false);

		tableau.set(2, 3, 1);

		tableau.set(1, 2, 1);
		tableau.set(1, 3, 1);
		tableau.set(1, 4, -1);

		tableau.set(2, 1, -1);
		tableau.set(2, 4, -1);

		tableau.integrity_check(false);
	}

	SECTION("test_pivot_row_select")
	{
		SparseTableau tableau(5);

		tableau.add_row();
		tableau.rows[0].negate();
		tableau.add_row();
		tableau.rows[1].negate();
		tableau.add_row();

		tableau.set(0, 3, 1);
		tableau.set(0, 1, -1);
		tableau.set(0, 4, 1);

		tableau.set(1, 1, 1);
		tableau.set(1, 4, 1);
		tableau.rows[1].negate();
		tableau.set(1, 3, 1);

		tableau.set(1, 0, -1);

		tableau.set(0, 0, 1);
		tableau.set(2, 0, 1);

		tableau.rows[0].set_value(100);
		tableau.rows[1].set_value(200);
		tableau.rows[2].set_value(50);

		tableau.integrity_check(false);

		REQUIRE(tableau.get_pivot_row(0) == 2);

		REQUIRE_THROWS(tableau.get_pivot_row(1));

		REQUIRE(tableau.get_pivot_row(4) == 0);
	}

	SECTION("test_insert_sequential")
	{
		buffered_forward_list list;
		auto it = list.before_begin();

		it = it.insert_after(1);
		it = it.insert_after(2);
		it = it.insert_after(3);

		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 3);
		REQUIRE(res[0] == 1);
		REQUIRE(res[1] == 2);
		REQUIRE(res[2] == 3);
	}

	SECTION("test_insert_nonsequential")
	{
		buffered_forward_list list;

		auto it = list.before_begin();

		it.insert_after(3);
		it.insert_after(2);
		it.insert_after(1);
		++it;
		it.insert_after(5);

		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 4);
		REQUIRE(res[0] == 1);
		REQUIRE(res[1] == 5);
		REQUIRE(res[2] == 2);
		REQUIRE(res[3] == 3);
	}

	SECTION("test_erase_front")
	{
		buffered_forward_list list;

		auto it = list.before_begin();

		//produce 1 2 3 4
		it.insert_after(2);
		it.insert_after(1);
		++it;
		++it;
		it.insert_after(4);
		it.insert_after(3);

		it = list.before_begin();
		it.erase_after();

		it = list.begin();
		it.erase_after();


		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 2);
		REQUIRE(res[0] == 2);
		REQUIRE(res[1] == 4);
	}

	SECTION("test_buffered_iter")
	{
		buffered_forward_list list;

		buffered_forward_list_iter it(list);

		it.insert(1);
		it++;
		it.insert(2);

		buffered_forward_list_iter it2(list);

		while (!it2.at_end()) {
			it2++;
		}
		it2.insert(3);

		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 3);
		REQUIRE(res[0] == 1);
		REQUIRE(res[1] == 2);
		REQUIRE(res[2] == 3);
	}

	SECTION("test_erase_iter")
	{
		buffered_forward_list list;

		auto it = list.before_begin();

		it = it.insert_after(1);
		it = it.insert_after(2);
		it = it.insert_after(3);

		it = list.begin();

		auto it_after = it.erase_after();

		REQUIRE(*it == 1);
		REQUIRE(*it_after == 3);


	}

	SECTION("test_huge_list")
	{
		buffered_forward_list list;

		auto it = list.before_begin();

		for (size_t i = 0; i < 0x4'0000; i++) {
			it.insert_after(i % 0x1'0000);
			it++;
		}
	}

	SECTION("test_c_insert_sequential")
	{

		compressed_forward_list list;
		auto it = list.before_begin();

		it = it.insert_after(1);
		it = it.insert_after(2);
		it = it.insert_after(3);
		it = it.insert_after(4);
		it = it.insert_after(5);

		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 5);
		REQUIRE(res[0] == 1);
		REQUIRE(res[1] == 2);
		REQUIRE(res[2] == 3);
		REQUIRE(res[3] == 4);
		REQUIRE(res[4] == 5);
	}

	SECTION("test_c_insert_reverse")
	{
		compressed_forward_list list;
		auto it = list.before_begin();

		it.insert_after(5);
		it.insert_after(4);
		it.insert_after(3);
		it.insert_after(2);
		it.insert_after(1);


		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 5);
		REQUIRE(res[0] == 1);
		REQUIRE(res[1] == 2);
		REQUIRE(res[2] == 3);
		REQUIRE(res[3] == 4);
		REQUIRE(res[4] == 5);
	}

	SECTION("test_c_insert_mixed")
	{
		compressed_forward_list list;
		auto it = list.before_begin();

		it.insert_after(3);
		it.insert_after(2);
		it.insert_after(1);
		++it;
		++it;
		++it;
		it.insert_after(5);
		it.insert_after(4);
		
		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}
		
		REQUIRE(res.size() == 5);
		REQUIRE(res[0] == 1);
		REQUIRE(res[1] == 2);
		REQUIRE(res[2] == 3);
		REQUIRE(res[3] == 4);
		REQUIRE(res[4] == 5);
	}

	SECTION("test_c_erase_head")
	{
		compressed_forward_list list;
		auto it = list.before_begin();

		it = it.insert_after(1);
		it = it.insert_after(2);

		it = list.before_begin();

		it.erase_after();
		
		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 1);
		REQUIRE(res[0] == 2);
	}

	SECTION("test_c_erase_iter")
	{
		compressed_forward_list list;

		auto it = list.before_begin();

		it = it.insert_after(1);
		it = it.insert_after(2);
		it = it.insert_after(3);

		it = list.begin();

		auto it_after = it.erase_after();

		REQUIRE(*it == 1);
		REQUIRE(*it_after == 3);
	}

	SECTION("test_empty_list")
	{
		compressed_forward_list list;

		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 0);
	}

	SECTION("test_erase_firstiter_condition")
	{
		compressed_forward_list list;

		auto it = list.before_begin();
		it = it.insert_after(1);
		it = it.insert_after(2);
		it = it.insert_after(3);
		it = list.before_begin();
		it.erase_after();

		std::vector<uint16_t> res;
		for (auto val : list) {
			res.push_back(val);
		}

		REQUIRE(res.size() == 2);
		REQUIRE(res[0] == 2);
		REQUIRE(res[1] == 3);
	}
}


} // namespace speedex
