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
/*
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
}*/

template<typename forward_list_t>
void insert_to_list(forward_list_t& list, size_t idx) {

	buffered_forward_list_iter<forward_list_t> it(list);
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
		//insert_to_list(pos2, idx);
	} else {
		insert_to_list(neg, idx);
		//insert_to_list(neg2, idx);
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

template<typename buffered_iter_t>
void insert_to_iterator(buffered_iter_t& it, uint16_t idx) {
	//std::printf("start insert_to_iterator\n");
	while (!it.at_end()) {
	//	std::printf("compare against %u\n", *it);
		if (idx < *it) {
			it.insert(idx);
			return;
		}
		it++;
	}
	//std::printf("insert at end\n");
	it.insert(idx);	
}

template<typename forward_list_t>
bool try_erase_from_iterator(buffered_forward_list_iter<forward_list_t>& it, uint16_t value) {
	//std::printf("try erase from buffered_it\n");
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
SignedTUColumn::iterator::insert_pos(uint16_t row) {
	if (negations[row]) {
		insert_to_iterator(neg_it, row);
	} else {
		insert_to_iterator(pos_it, row);
	}
}

void
SignedTUColumn::iterator::insert_neg(uint16_t row) {
	if (negations[row]) {
		insert_to_iterator(pos_it, row);
	} else {
		insert_to_iterator(neg_it, row);
	}
}

void
SignedTUColumn::iterator::remove_pos(uint16_t row) {
	if (negations[row]) {
		if (!try_erase_from_iterator(neg_it, row)) {
			throw std::runtime_error("desync");
		}
	} else {
		if (!try_erase_from_iterator(pos_it, row)) {
			throw std::runtime_error("desync");
		}	
	}
}
void 
SignedTUColumn::iterator::remove_neg(uint16_t row) {
	if (negations[row]) {
		if (!try_erase_from_iterator(pos_it, row)) {
			throw std::runtime_error("desync");
		}
	} else {
		if (!try_erase_from_iterator(neg_it, row)) {
			throw std::runtime_error("desync");
		}	
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

void remove_from_list(buffered_forward_list& list, uint16_t idx) {
	buffered_forward_list_iter it(list);
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

bool try_erase_from_iterator(forward_list_iter<uint16_t>& it, uint16_t value) {
	//std::printf("try erase from regular it\n");
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

void check_(std::forward_list<uint16_t> const& l1, buffered_forward_list const& l2) {
	auto it1 = l1.begin();
	auto it2 = l2.begin();

	std::printf("begin check\n");

	for (auto const& val : l1) {
		std::printf("expected %u\n", val);
	}

	while (it1 != l1.end()) {
		std::printf("checking %u\n", *it1);
		if (*it1 != *it2) {
			std::printf("%u vs %u\n", *it1, *it2);
			throw std::runtime_error("iter mismatch");
		}
		it1++;
		it2++;
	}
	if (it2 != l2.end()) {
		throw std::runtime_error("wtf");
	}
}

/*void SignedTURow::check() const {
	pos2.print_list();
	neg2.print_list();
	check_(pos, pos2);
	check_(neg, neg2);
	std::printf("done row\n");
} */


void 
SignedTURow::iterator::insert_pos(uint16_t idx) {
	if (negated) {
	//	insert_to_iterator(neg_it2, idx);
		insert_to_iterator(neg_it, idx);
	} else {
		//insert_to_iterator(pos_it2, idx);
		insert_to_iterator(pos_it, idx);
	}
}
void 
SignedTURow::iterator::insert_neg(uint16_t idx) {
	if (negated) {
	//	std::printf("within insert_neg, insert to pos (bc negation)\n");
	//	insert_to_iterator(pos_it2, idx);
		insert_to_iterator(pos_it, idx);
	} else {
	//	std::printf("within insert_neg, insert to neg (bc no negation)\n");
	//	insert_to_iterator(neg_it2, idx);
		insert_to_iterator(neg_it, idx);
	}
}

bool 
SignedTURow::iterator::try_erase_pos(uint16_t idx) {
	//std::printf("try_erase_pos on %u\n", idx);
	if (negated) {
	//	std::printf("negated: from neg list\n");
	//	bool res = try_erase_from_iterator(neg_it2, idx);
	//	std::printf("buffered_it res was %d\n", res);
		return try_erase_from_iterator(neg_it, idx);
	} else {
	//    std::printf("from pos list\n");
	//	bool res = try_erase_from_iterator(pos_it2, idx);
	//	std::printf("buffered_it res was %d\n", res);
		return try_erase_from_iterator(pos_it, idx);
	}
}
bool 
SignedTURow::iterator::try_erase_neg(uint16_t idx) {
	if (negated) {
	//	try_erase_from_iterator(pos_it2, idx);
		return try_erase_from_iterator(pos_it, idx);
	} else {
	//	try_erase_from_iterator(neg_it2, idx);
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

//	integrity_check();

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

	auto add_pos_it = [this, &row_iters, &negated_rows, &touched_rows, &pivot_row] (uint16_t row_idx) {
		if (row_idx != pivot_row) {
			if (!negations[row_idx]) {
				rows[row_idx].negate();
				negated_rows.push_back(row_idx);
			}
			touched_rows.push_back(row_idx);
			row_iters.push_back(rows[row_idx].begin_insert(row_idx));
		}
	};

	auto add_neg_it = [this, &row_iters, &negated_rows, &touched_rows, &pivot_row] (uint16_t row_idx) {
		if (row_idx != pivot_row) {
			if (negations[row_idx]) {
				rows[row_idx].negate();
				negated_rows.push_back(row_idx);
			}
			touched_rows.push_back(row_idx);
			row_iters.push_back(rows[row_idx].begin_insert(row_idx));
		}
	};

	auto pos_acc_it = col.pos.begin();
	auto neg_acc_it = col.neg.begin();

	while(true) {
		if (pos_acc_it == col.pos.end()) {
			while(neg_acc_it != col.neg.end()) {
				add_neg_it(*neg_acc_it);
				++neg_acc_it;
			}
			break;
		}
		if (neg_acc_it == col.neg.end()) {
			while(pos_acc_it != col.pos.end()) {
				add_pos_it(*pos_acc_it);
				++pos_acc_it;
			}
			break;
		}

		if (*pos_acc_it < *neg_acc_it) {
			add_pos_it(*pos_acc_it);
			++pos_acc_it;
		} else {
			add_neg_it(*neg_acc_it);
			++neg_acc_it;
		}
	}

	//print("prior to check");
	//integrity_check();

	while(true) {
		auto [next_col_idx, coeff] = advance();

		if (coeff == 0) {
			//integrity_check();

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

		auto &mod_col = cols[next_col_idx];

		auto mod_col_it = mod_col.begin();

		for (auto& row_it : row_iters) {
			if (coeff > 0) {
				row_it.guarded_insert_pos(next_col_idx, mod_col_it);
			} else {
				row_it.guarded_insert_neg(next_col_idx, mod_col_it);
			}
			//print("post insert");
			//integrity_check();
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
	//	rows[row].check();
		check_incr_list(rows[row].pos);
		check_incr_list(rows[row].neg);
	}

	for (size_t i = 0; i < cols.size(); i++) {
		//col.check();
		check_incr_list(cols[i].pos);
		check_incr_list(cols[i].neg);
	}
	if (print_warning) {
		std::printf("done integrity check\n");
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
