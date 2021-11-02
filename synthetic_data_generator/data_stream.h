#pragma once

#include "xdr/experiments.h"

#include <cstdint>
#include <memory>

#include <xdrpp/types.h>
#include <xdrpp/marshal.h>

namespace speedex {

class DataStream {

public:

	virtual std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>> 
	load_txs_unparsed() = 0;
};

class MockDataStream : public DataStream {

public:

	std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>> 
	load_txs_unparsed() override final {
		ExperimentBlock block;

		size_t sz = 100'000;

		block.resize(sz);

		std::shared_ptr<xdr::opaque_vec<>> v = std::make_shared<xdr::opaque_vec<>>();

		*v = xdr::xdr_to_opaque(block);

		return {sz, v};
	}

};

} /* speedex */
