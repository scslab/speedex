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

		return DataBuffer{sz, v, count};
	}

};

} /* speedex */
