#pragma once

#include "xdr/block.h"

#include <cstdint>
#include <optional>

namespace speedex {

struct SpeedexVMBlockID {

	std::optional<HashedBlock> value;

	SpeedexVMBlockID()
		: value(std::nullopt)
		{}

	SpeedexVMBlockID(HashedBlock const& block)
		: value(std::make_optional<HashedBlock>(block))
		{}

	SpeedexVMBlockID(std::vector<uint8_t> const& bytes);

	bool operator==(const SpeedexVMBlockID& other) const;

	std::vector<uint8_t> serialize() const;

	operator bool() const {
		return value.has_value();
	}
};

} /* speedex */
