#pragma once

#include "xdr/block.h"

namespace speedex {

struct SpeedexVMBlockID {

	std::optional<HashedBlock> value;

	SpeedexVMBlockID()
		: value(std::nullopt)
		{}

	SpeedexVMBlockID(HashedBlock const& block)
		: value(std::make_optional<HashedBlock>(block))
		{}

	bool operator==(const SpeedexVMBlockID& other) const;
};

} /* speedex */
