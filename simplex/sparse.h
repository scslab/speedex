#pragma once

#include "xdr/types.h"

#include "simplex/allocator.h"

#include <optional>
#include <set>
#include <vector>
#include <forward_list>

namespace speedex {

struct SparseTUColumn {
	std::forward_list<uint16_t> nonzeros;

	void insert(uint16_t row);
	void insert_maybe(uint16_t row);
	void remove(uint16_t row);

	SparseTUColumn(const SparseTUColumn&) = delete;
	SparseTUColumn() = default;

	void set_singleton(uint16_t row_idx) {
		nonzeros.clear();
		nonzeros.insert_after(nonzeros.before_begin(), row_idx);
	}

	bool empty() {
		return nonzeros.empty();
	}
	uint16_t pop_front() {
		uint16_t front = *nonzeros.begin();
		nonzeros.erase_after(nonzeros.before_begin());
		return front;
	}
};

struct SparseTURow {

	std::vector<uint16_t> pos, neg;

	using int128_t = __int128;

	int128_t value;

	const int128_t& get_value() const {
		return value;
	}

	void set_value(const int128_t& new_value) {
		value = new_value;
	}

	int8_t operator[](uint16_t idx) const;

	void negate() {
		std::swap(pos, neg);
		value *= -1;
	}

	// can only set a value that's not already set
	void set(size_t idx, int8_t value);

	void add(SparseTURow const& other_row, const size_t this_row_idx, int8_t coeff, const size_t nomodify_col, std::vector<SparseTUColumn>& cols);
};

typedef std::vector<bool> NegatedRows;

template<typename T>
class forward_list_iter {
	std::forward_list<T>& list;
	std::forward_list<T>::iterator iter, back_iter;
public:

	forward_list_iter(std::forward_list<T>& list)
		: list(list)
		, iter(list.begin())
		, back_iter(list.before_begin()) {}

	T& operator*() {
		return *iter;
	}

	forward_list_iter& operator++(int) {
		iter++;
		back_iter++;
		return *this;
	}

	void insert(T const& val) {
		iter = list.insert_after(back_iter, val);
	}

	void erase() {
		iter = list.erase_after(back_iter);
	}

	bool at_end() {
		return iter == list.end();
	}
};

extern Allocator alloc;

struct SignedTUColumn {

	//buffered_forward_list pos, neg;
	std::forward_list<uint16_t> pos, neg;

	NegatedRows const& negated;

	SignedTUColumn(NegatedRows const& negations)
		//: pos(alloc)
		//, neg(alloc)
		: pos(), neg()
		, negated(negations)
		{}

	SignedTUColumn(const SignedTUColumn&) = delete;
	SignedTUColumn(SignedTUColumn&&) = default;

	void insert_pos(uint16_t row);
	void insert_neg(uint16_t row);
	void remove_pos(uint16_t row);
	void remove_neg(uint16_t row);

	void set_single_pos(uint16_t row_idx) {
		pos.clear();
		neg.clear();
		if (negated[row_idx]) {
			//neg.before_begin().insert_after(row_idx);
			neg.insert_after(neg.before_begin(), row_idx);
		} else {
			//pos.before_begin().insert_after(row_idx);
			pos.insert_after(pos.before_begin(), row_idx);
		}
	}

	void set(uint16_t row, int8_t value) {
		if (value > 0) {
			insert_pos(row);
		} else if (value < 0) {
			insert_neg(row);
		}
	}

	int8_t operator[](uint16_t row_idx) const {
		for (auto const& p : pos) {
			if (p == row_idx) {
				return negated[row_idx] ? -1 : 1;
			}
		}
		for (auto const& n : neg) {
			if (n == row_idx) {
				return negated[row_idx] ? 1 : -1;
			}
		}
		return 0;
	}
};

struct SignedTURow {
	buffered_forward_list pos, neg;
	//std::forward_list<uint16_t> pos, neg;

	using int128_t = __int128;

	int128_t value;

	const size_t negation_idx;
	NegatedRows& negations;

	SignedTURow(NegatedRows& negations)
		: pos(alloc)
		, neg(alloc)
		, value(0)
		, negation_idx(negations.size())
		, negations(negations)
	{
		negations.push_back(false);
	}

	SignedTURow(SignedTURow&& other)
		: pos(std::move(other.pos))
		, neg(std::move(other.neg))
		, value(other.value)
		, negation_idx(other.negation_idx)
		, negations(other.negations)
		{}

	int128_t get_value() const {
		return is_negated() ? -value : value;
	}

	void set_value(const int128_t& new_value) {
		value = is_negated() ? -new_value : new_value;
	}

	void negate() {
		negations[negation_idx] = !negations[negation_idx];
	}

	bool is_negated() const {
		return negations[negation_idx];
	}

	void set(size_t idx, int8_t value);

	//void check() const;

	class iterator {
		buffered_forward_list_iter pos_it, neg_it;
		//forward_list_iter<uint16_t> pos_it, neg_it;
		const bool negated;
		const uint16_t row_idx;

	public:
		iterator(SignedTURow& row, bool negated, uint16_t row_idx)
		//	: pos_it2(row.pos2)
		//	, neg_it2(row.neg2)
			: pos_it(row.pos)
			, neg_it(row.neg)
			, negated(negated)
			, row_idx(row_idx) {}

		void insert_pos(uint16_t idx);
		void insert_neg(uint16_t idx);

		bool try_erase_pos(uint16_t idx);
		bool try_erase_neg(uint16_t idx);

		void guarded_insert_pos(uint16_t idx, SignedTUColumn& mod_col) {
		//	std::printf("guarded_insert_pos: insert col %u to row %u\n", idx, row_idx);
			if (!try_erase_neg(idx)) {
		//		std::printf("erase_neg failed, inserting pos\n");
				insert_pos(idx);
				mod_col.insert_pos(row_idx);
			} else {
				mod_col.remove_neg(row_idx);
			}
		}
		void guarded_insert_neg(uint16_t idx, SignedTUColumn& mod_col) {
		//	std::printf("guarded_insert_neg: insert col %u to row %u\n", idx, row_idx);
			if (!try_erase_pos(idx)) {
		//		std::printf("erase_pos failed, inserting neg\n");
				insert_neg(idx);
				mod_col.insert_neg(row_idx);
			} else {
				mod_col.remove_pos(row_idx);
			}
		}
	};

	//row_idx should be index of this
	iterator begin_insert(uint16_t row_idx) {
		return iterator(*this, is_negated(),row_idx);
	}

	int8_t operator[](uint16_t col_idx) const {
		for (auto const& p : pos) {
			if (p == col_idx) {
				return is_negated() ? -1 : 1;
			}
		}
		for (auto const& n : neg) {
			if (n == col_idx) {
				return is_negated() ? 1 : -1;
			}
		}
		return 0;
	}
};

struct SparseTableau {

	using int128_t = __int128;

	NegatedRows negations;
	std::vector<SignedTURow> rows;
	std::vector<SignedTUColumn> cols;

	SparseTableau(uint16_t num_cols)
		: negations()
		, rows()
		, cols()
	{
		cols.reserve(num_cols);
		for (size_t i = 0; i < num_cols; i++) {
			cols.emplace_back(negations);
		}
	}

	void add_row() {
		rows.emplace_back(negations);
	}

	void do_pivot(uint16_t pivot_row, uint16_t pivot_col);

	uint16_t get_pivot_row(uint16_t col_idx) const;

	void set(uint16_t row, uint16_t col, int8_t value);

	int8_t get(uint16_t row, uint16_t col) const;
	void integrity_check(bool print_warning = true) const;

	void print_row(uint16_t row_idx) const;
	void print(std::string s = "") const;
};

} /* speedex */

