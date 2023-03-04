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

#pragma once

#include "synthetic_data_generator/data_stream.h"

#include "utils/save_load_xdr.h"

#include <cstdint>
#include <memory>

#include <xdrpp/types.h>

namespace speedex {

/*
//TODO future: rotating system of buffers to avoid excessive copying.
class BufferManager {

	using buffer_t = std::shared_ptr<std::vector<unsigned char>>;

};*/

class SyntheticDataStream : public DataStream {
	constexpr static size_t BUFFER_SIZE = 100'000'000;

	std::string folder;
	uint64_t cur_block_number;

	bool finished;

	unsigned char * const buffer;

public:

	SyntheticDataStream(std::string root_folder)
		: DataStream()
		, folder(root_folder)
		, cur_block_number{1}
		, finished(false)
		, buffer(new unsigned char[BUFFER_SIZE])
		{}

	~SyntheticDataStream()
	{
		delete buffer;
	}

	DataBuffer 
	load_txs_unparsed() override final;

};

} /* speedex */
