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

#include "synthetic_data_generator/synthetic_data_stream.h"

#include <xdrpp/marshal.h>

namespace speedex {

std::string 
tx_filename(std::string const& root, uint64_t block_number) {
	return root + std::to_string(block_number) + std::string(".txs");
}

DataBuffer
SyntheticDataStream::load_txs_unparsed() {
	std::shared_ptr<xdr::opaque_vec<>> data = std::make_shared<xdr::opaque_vec<>>();
	auto filename = tx_filename(folder, cur_block_number);

	if(load_xdr_from_file_fast(*data, filename.c_str(), buffer, BUFFER_SIZE) != 0) {
		finished = true;
		return {0, nullptr, cur_block_number, finished};
	}

	cur_block_number ++;

	uint32_t num_txs;

	if (data -> size() < 4) {
		finished = true;
		return {0, nullptr, cur_block_number - 1, finished};
	}

	xdr::xdr_get g(data->data(), data -> data() + 4);

	xdr::xdr_argpack_archive(g, num_txs);

	return DataBuffer{num_txs, data, cur_block_number - 1, finished};
}


} /* speedex */
