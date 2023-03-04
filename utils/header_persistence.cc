/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils/header_persistence.h"
#include "utils/save_load_xdr.h"

#include "config.h"

namespace speedex {

inline std::string 
header_filename(const uint64_t round_number) {
	return std::string(ROOT_DB_DIRECTORY) + std::string(HEADER_DB) + std::to_string(round_number) + std::string(".header");
}

HashedBlock
load_header(const uint64_t round_number) {
	HashedBlock out;
	auto filename = header_filename(round_number);
	auto res = load_xdr_from_file(out, filename.c_str());
	if (res != 0) {
		throw std::runtime_error((std::string("can't find header file ") + filename).c_str());
	}
	return out;
}

bool check_if_header_exists(const uint64_t round_number) {
	auto filename = header_filename(round_number);
	if (FILE* file = fopen(filename.c_str(), "r")) {
		fclose(file);
		return true;
	}
	return false;
}

void save_header(const HashedBlock& header) {
	auto filename = header_filename(header.block.blockNumber);
	if (save_xdr_to_file(header, filename.c_str())) {
		throw std::runtime_error("could not save header file!");
	}
}

}
