#include "simplex/sparse.h"

namespace speedex {




template<typename list_t>
void print_list(list_t const& l) {
	for (auto const& l2 : l) {
		std::printf("%u ", l2);
	}
	std::printf("\n");
}

template<typename list_t>
void check_incr_list(list_t const& l, std::string err = {""}) {
	if (l.begin() == l.end()) return;
	std::optional<uint16_t> val = std::nullopt;
	for (auto const& v : l) {
		if (!val) {
			val = v;
		}
		else if (v <= *val) {

			print_list(l);
			throw std::runtime_error(std::string("invalid list! ") + err);
		}
		val = v;
	}
}

void add_list(
	std::forward_list<uint16_t>& match_sign_dst, 
	std::forward_list<uint16_t>& opp_sign_dst, 
	std::forward_list<uint16_t> const& src, 
	const size_t dst_row_idx, 
	std::vector<SparseTUColumn>& cols)
{

	forward_list_iter match_iter(match_sign_dst);
	forward_list_iter opp_iter(opp_sign_dst);

	//auto match_iter = match_sign_dst.begin();
	//auto opp_iter = opp_sign_dst.begin();

	auto src_iter = src.begin();

	//size_t src_idx = 0;
	while (src_iter != src.end()) {
		while ((!match_iter.at_end()) && (*match_iter < *src_iter)) {
		//while (match_iter != match_sign_dst.end() && (*match_iter) < src[src_idx]) {
			match_iter++;
		}
		while ((!opp_iter.at_end()) && (*opp_iter < *src_iter)) {
		//while (opp_iter != opp_sign_dst.end() && (*opp_iter) < src[src_idx]) {
			opp_iter++;
		}

		bool present_in_opp = (!opp_iter.at_end()) && (*opp_iter == *src_iter);
		//bool present_in_opp = (opp_iter != opp_sign_dst.end()) && ((*opp_iter) == src[src_idx]);
	
		if (present_in_opp) {
			cols[*opp_iter].remove(dst_row_idx);
			opp_iter.erase();
			//opp_iter = opp_sign_dst.erase(opp_iter);
		} else {
			match_iter.insert(*src_iter);
		//	match_iter = match_sign_dst.insert(match_iter, src[src_idx]);
			cols[*match_iter].insert(dst_row_idx);
		}
		src_iter ++;
	}
}

void
add_list(
	std::vector<uint16_t>& match_sign_dst, 
	std::vector<uint16_t>& opp_sign_dst, 
	std::vector<uint16_t> const& src, 
	const size_t dst_row_idx, 
	const size_t nomodify_col,
	std::vector<SparseTUColumn>& cols)
{
	size_t match_iter = 0;
	size_t opp_iter = 0;

	size_t src_iter = 0;

	const size_t src_end_sz = src.size();

	while (src_iter != src_end_sz) {
		while (match_iter != match_sign_dst.size() && (match_sign_dst[match_iter]) < src[src_iter]) {
			match_iter++;
		}
		while (opp_iter != opp_sign_dst.size() && (opp_sign_dst[opp_iter]) < src[src_iter]) {
			opp_iter++;
		}

		bool present_in_opp = (opp_iter != opp_sign_dst.size()) && (opp_sign_dst[opp_iter] == src[src_iter]);
	
		if (present_in_opp) {
			if (opp_sign_dst[opp_iter] != nomodify_col) {
				cols[opp_sign_dst[opp_iter]].remove(dst_row_idx);
			}
			opp_sign_dst.erase(opp_sign_dst.begin() + opp_iter);
		} else {
			match_sign_dst.insert(match_sign_dst.begin() + match_iter, src[src_iter]);
			if (match_sign_dst[match_iter] != nomodify_col) {
				cols[match_sign_dst[match_iter]].insert(dst_row_idx);
			}
		}
		src_iter ++;
	}
}


template<typename list_t>
void add_list(
	list_t& match_sign_dst, 
	list_t& opp_sign_dst, 
	list_t const& src, 
	const size_t dst_row_idx, 
	const size_t nomodify_col,
	std::vector<SparseTUColumn>& cols)
{
	auto match_iter = match_sign_dst.begin();
	auto opp_iter = opp_sign_dst.begin();

	auto src_iter = src.begin();

	while (src_iter != src.end()) {
		while (match_iter != match_sign_dst.end() && (*match_iter) < *src_iter) {
			match_iter++;
		}
		while (opp_iter != opp_sign_dst.end() && (*opp_iter) < *src_iter) {
			opp_iter++;
		}

		bool present_in_opp = (opp_iter != opp_sign_dst.end()) && ((*opp_iter) == *src_iter);
	
		if (present_in_opp) {
			if (*opp_iter != nomodify_col) {
				cols[*opp_iter].remove(dst_row_idx);
			}
			opp_iter = opp_sign_dst.erase(opp_iter);
		} else {
			match_iter = match_sign_dst.insert(match_iter, *src_iter);
			if (*match_iter != nomodify_col) {
				cols[*match_iter].insert(dst_row_idx);
			}
		}
		src_iter ++;
	}
}

void 
SparseTURow::add(SparseTURow const& other_row, const size_t this_row_idx, int8_t coeff, const size_t nomodify_col, std::vector<SparseTUColumn>& cols) {

	if (coeff > 0) {
		add_list(pos, neg, other_row.pos, this_row_idx, nomodify_col, cols);
		add_list(neg, pos, other_row.neg, this_row_idx, nomodify_col, cols);
	} else {
		add_list(pos, neg, other_row.neg, this_row_idx, nomodify_col, cols);
		add_list(neg, pos, other_row.pos, this_row_idx, nomodify_col, cols);
	}
	value += other_row.value * coeff;
}


bool find(std::set<uint16_t> const& set, uint16_t idx) {
	return set.find(idx) != set.end();
}

bool find(std::vector<uint16_t> const& vec, uint16_t idx) {

	if (vec.empty()) { return false; }

	size_t low = 0, high = vec.size() - 1;

	size_t mid = (low + high) / 2;
	while (low != high) {
		if (idx == vec[mid]) {
			return true;
		}
		if (idx > vec[mid]) {
			low = mid + 1;
		} else {
			high = mid;
		}
		mid = (low + high) / 2;
	}
	return vec[mid] == idx;
}

int8_t 
SparseTURow::operator[](uint16_t idx) const {
	if (find(pos, idx)) {
		return 1;
	}
	if (find(neg, idx)) {
		return -1;
	}
	return 0;
} 

void insert_to_list(std::forward_list<uint16_t>& list, size_t idx) {

	forward_list_iter it(list);
	while (!it.at_end()) {
		if (idx < *it) {
			it.insert(idx);
			return;
		}
		it++;
	}
	it.insert(idx);
}

void insert_to_list(std::set<uint16_t>& set, size_t idx) {
	set.insert(idx);
}

void insert_to_list(std::vector<uint16_t>& list, size_t idx) {

	auto iter = list.begin();
	while (iter != list.end()) {
		if (idx < *iter) {
			list.insert(iter, idx);
			return;
		}
		iter++;
	}
	list.insert(iter, idx);
}

void 
SparseTURow::set(size_t idx, int8_t value) {
	if (value > 0) {
		insert_to_list(pos, idx);
	} else {
		insert_to_list(neg, idx);
	}
}

void 
SignedTURow::set(size_t idx, int8_t value) {
	bool is_pos = ((value > 0) && (!negations[negation_idx]))
		|| ((value < 0) && (negations[negation_idx]));

	if (is_pos) {
		insert_to_list(pos, idx);
	} else {
		insert_to_list(neg, idx);
	}
}


void
SparseTUColumn::insert(uint16_t row) {
	auto iter = nonzeros.begin();
	auto back_iter = nonzeros.before_begin();
	while (iter != nonzeros.end()) {
		if (*iter < row) {
			nonzeros.insert_after(back_iter, row);
			return;
		}
		iter++;
		back_iter++;
	}
	nonzeros.insert_after(back_iter, row);
}

void 
SignedTUColumn::insert_pos(uint16_t row) {
	if (negated[row]) {
		insert_to_list(neg, row);
	} else {
		insert_to_list(pos, row);
	}
}
void 
SignedTUColumn::insert_neg(uint16_t row) {
	if (negated[row]) {
		insert_to_list(pos, row);
	} else {
		insert_to_list(neg, row);
	}
}

void remove_from_list(std::forward_list<uint16_t>& list, uint16_t idx) {
	forward_list_iter it(list);
	while (true) {
		if (idx == *it) {
			it.erase();
			return;
		}
		it++;
	}
}

void
SignedTUColumn::remove_pos(uint16_t row) {
	if (negated[row]) {
		remove_from_list(neg, row);
	} else {
		remove_from_list(pos, row);
	}
}
void 
SignedTUColumn::remove_neg(uint16_t row) {
	if (negated[row]) {
		remove_from_list(pos, row);
	} else {
		remove_from_list(neg, row);
	}
}

void
SparseTUColumn::insert_maybe(uint16_t row) {
	auto iter = nonzeros.begin();
	auto back_iter = nonzeros.before_begin();
	while (iter != nonzeros.end()) {
		if (*iter == row) {
			return;
		}
		if (*iter < row) {
			nonzeros.insert_after(back_iter, row);
			return;
		}
		iter++;
		back_iter++;
	}
	nonzeros.insert_after(back_iter, row);
}

void
SparseTUColumn::remove(uint16_t row) {

	auto iter = nonzeros.begin();
	auto back_iter = nonzeros.before_begin();

	while (true) {
		if (iter == nonzeros.end()) {
			std::printf("attempt to remove %u from list without it\n", row);
			print_list(nonzeros);
			throw std::runtime_error("nnz accounting");
		}
		if (*iter == row) {
			nonzeros.erase_after(back_iter);
			return;
		}
		iter++;
		back_iter++;
	}
/*	for (size_t idx = 0; idx < nonzeros.size(); idx++) {
		if (nonzeros[idx] == row) {
			nonzeros.erase(nonzeros.begin() + idx);
			return;
		}
	} */
	throw std::runtime_error("nnz accounting error");
}

void insert_to_iterator(forward_list_iter<uint16_t>& it, uint16_t idx) {
	while (!it.at_end()) {
		if (idx < *it) {
			it.insert(idx);
			return;
		}
		it++;
	}
	it.insert(idx);	
}

bool try_erase_from_iterator(forward_list_iter<uint16_t>& it, uint16_t value) {
	while (!it.at_end()) {
	//	std::printf("*it=%u value=%u\n", *it, value);
		if (*it == value) {
			it.erase();
			return true;
		}
		if (*it > value) {
			return false;
		}
		it++;
	}
	return false;
}

void 
SignedTURow::iterator::insert_pos(uint16_t idx) {
	if (negated) {
		insert_to_iterator(neg_it, idx);
	} else {
		insert_to_iterator(pos_it, idx);
	}
}
void 
SignedTURow::iterator::insert_neg(uint16_t idx) {
	if (negated) {
		insert_to_iterator(pos_it, idx);
	} else {
		insert_to_iterator(neg_it, idx);
	}
}

bool 
SignedTURow::iterator::try_erase_pos(uint16_t idx) {
	//std::printf("try_erase_pos on %u\n", idx);
	if (negated) {
	//	std::printf("negated: from neg list\n");
		return try_erase_from_iterator(neg_it, idx);
	} else {
	//	std::printf("from pos list\n");
		return try_erase_from_iterator(pos_it, idx);
	}
}
bool 
SignedTURow::iterator::try_erase_neg(uint16_t idx) {
	if (negated) {
		return try_erase_from_iterator(pos_it, idx);
	} else {
		return try_erase_from_iterator(neg_it, idx);
	}
}

// Requires that tableau[row][col] = 1
void 
SparseTableau::do_pivot(uint16_t pivot_row, uint16_t pivot_col) {
	auto& row = rows[pivot_row];
	auto& col = cols[pivot_col];

	//std::printf("\n\nprior to pivot on (%u, %u)\n", pivot_row, pivot_col);
	//print("pivot prior");

	auto pos_row_it = row.pos.begin();
	auto neg_row_it = row.neg.begin();

	auto advance = [&pos_row_it, &neg_row_it, &row, &pivot_row, this] () -> std::pair<uint16_t, int8_t> {
		int8_t out = 0;
		uint16_t idx = UINT16_MAX;
		if (pos_row_it != row.pos.end()) {
			idx = *pos_row_it;
			out = 1;
		}
		if (neg_row_it != row.neg.end()) {
			uint16_t idx_candidate = *neg_row_it;
			if (out == 1) {
				if (idx_candidate < idx) {
					idx = idx_candidate;
					out = -1;
				}
			} else {
				idx = idx_candidate;
				out = -1;
			}
		}
		if (out > 0) {
			pos_row_it++;
		}
		if (out < 0) {
			neg_row_it++;
		}

		if (row.is_negated()) {
			out *= -1;
		}
		return {idx, out};
	};

	//Iterators for the rows with nonzero elts in the pivot col.
	// divided by pos/neg, including accounting for negations
	std::vector<SignedTURow::iterator> row_iters;

	std::vector<uint16_t> negated_rows;
	std::vector<uint16_t> touched_rows;

	for (auto pos_nz : col.pos) {
		//std::printf("found %u\n", pos_nz);
		if (pos_nz != pivot_row) {

			if (!negations[pos_nz]) {
				rows[pos_nz].negate();
				negated_rows.push_back(pos_nz);
			}
			touched_rows.push_back(pos_nz);
			row_iters.push_back(rows[pos_nz].begin_insert(pos_nz));


		}
	}

	for (auto neg_nz : col.neg) {

		//std::printf("found %u\n", neg_nz);

		if (neg_nz != pivot_row) {


			if (negations[neg_nz]) {
				rows[neg_nz].negate();
				negated_rows.push_back(neg_nz);
			}

			touched_rows.push_back(neg_nz);

			row_iters.push_back(rows[neg_nz].begin_insert(neg_nz));

		}
	}

	//print("post negations");
	//integrity_check();

	while(true) {
		auto [next_col_idx, coeff] = advance();

		if (coeff == 0) {
			col.set_single_pos(pivot_row);

			for (auto touched_row : touched_rows) {
				rows[touched_row].set_value(rows[touched_row].get_value() + row.get_value());
			}

			for (auto negated_row : negated_rows) {
				rows[negated_row].negate();
			}

//			print("post pivot");
			//done
			return;
		}
		//if (next_col_idx == pivot_col) {
		//	continue;
		//}

		//std::printf("action on col %u, value %d\n", next_col_idx, coeff);

		auto &mod_col = cols[next_col_idx];

		for (auto& row_it : row_iters) {
			if (coeff > 0) {
				row_it.guarded_insert_pos(next_col_idx, mod_col);
			} else {
				row_it.guarded_insert_neg(next_col_idx, mod_col);
			}
		//	integrity_check();
		}
	}
	throw std::runtime_error("beyond here is dead code");
}



uint16_t 
SparseTableau::get_pivot_row(uint16_t col_idx) const {
	auto const& col = cols[col_idx];

	std::optional<size_t> row_out = std::nullopt;

	int128_t value = 0;

	//std::printf("pivot row query on col %u\n", col_idx);

	for (auto row_idx : col.pos) {
		int128_t const& constraint_value = rows[row_idx].get_value();

		//std::printf("pos row %u (negated: %d) has value %lf\n", row_idx, rows[row_idx].is_negated(), (double) rows[row_idx].get_value());
		if (!rows[row_idx].is_negated()) {
			if ((!row_out) || value > constraint_value) {
				row_out = row_idx;
				value = constraint_value;
			}
		}
	}

	for(auto row_idx : col.neg) {
		int128_t const& constraint_value = rows[row_idx].get_value();


		//std::printf("neg row %u (negated: %d) has value %lf\n", row_idx, rows[row_idx].is_negated(), (double) rows[row_idx].get_value());

		if (rows[row_idx].is_negated()) {
			if ((!row_out) || value > constraint_value) {
				row_out = row_idx;
				value = constraint_value;
			}
		}
	}

	if (row_out) {
		return *row_out;
	}

	throw std::runtime_error("failed to find pivot row");
}

void 
SparseTableau::set(uint16_t row, uint16_t col, int8_t value) {
	rows[row].set(col, value);
	cols[col].set(row, value);
}

int8_t
SparseTableau::get(uint16_t row, uint16_t col) const {
	int8_t row_val = rows[row][col];
	int8_t col_val = cols[col][row];
	if (row_val != col_val) {
		std::printf("got %d vs %d at (%u, %u)\n", row_val, col_val, row, col);
		throw std::runtime_error("mismatch!");
	}
	return row_val;
}

void
SparseTableau::integrity_check(bool print_warning) const {
	if (print_warning) {
		std::printf("performing (expensive) integrity check\n");
	}
	for (size_t row = 0; row < rows.size(); row++) {
		for (size_t col = 0; col < cols.size(); col++) {

			get(row, col);
		}
		check_incr_list(rows[row].pos);
		check_incr_list(rows[row].neg);
	}

	for (size_t i = 0; i < cols.size(); i++) {
		check_incr_list(cols[i].pos);
		check_incr_list(cols[i].neg);
	}
}

void
SparseTableau::print_row(uint16_t row_idx) const {
	auto const& row = rows[row_idx];
	for (size_t i = 0u; i < cols.size(); i++) {
		if (row[i] != -1) {
			std::printf(" ");
		}
		std::printf("%d ", row[i]);
	}
	std::printf("%lf\n", (double) row.get_value());
}

void 
SparseTableau::print(std::string s) const {
	std::printf("=== start tableau (%s) ===\n", s.c_str());
	const size_t rows_sz = rows.size();
	for (uint16_t row_idx = 0; row_idx < rows_sz; row_idx++) {
		print_row(row_idx);
	}
}




} /* speedex */
