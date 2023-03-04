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

#include "xdr/experiments.h"

#include <cstdint>
#include <memory>

#include <xdrpp/types.h>
#include <xdrpp/marshal.h>

namespace speedex {

struct DataBuffer {
	size_t num_txs;
	std::shared_ptr<xdr::opaque_vec<>> data;
	size_t buffer_number;
	bool finished;
};

class DataStream {

public:

	virtual DataBuffer
	load_txs_unparsed() = 0;
};

class MockDataStream : public DataStream {
	size_t count;

public:

	MockDataStream()
		: count(0)
		{}

	DataBuffer
	load_txs_unparsed() override final {
		ExperimentBlock block;

		size_t sz = 100'000;

		block.resize(sz);

		std::shared_ptr<xdr::opaque_vec<>> v = std::make_shared<xdr::opaque_vec<>>();

		*v = xdr::xdr_to_opaque(block);
		count++;

		return DataBuffer{sz, v, count, false};
	}

};

} /* speedex */
