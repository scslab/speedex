#include "speedex/speedex_vm_block_id.h"

#include <xdrpp/marshal.h>


namespace speedex {

using xdr::operator==;

bool
SpeedexVMBlockID::operator== (const SpeedexVMBlockID& other) const {
	if ((!value) && (!other.value)) {
		return true;
	}
	if (((bool)value) && ((bool) other.value)) {
		return (*value) == (*(other.value));
	}
	return false;
}

std::vector<uint8_t>
SpeedexVMBlockID::serialize() const {
	if (!value) {
		return {};
	}
	return xdr::xdr_to_opaque(*value);
}

SpeedexVMBlockID::SpeedexVMBlockID(std::vector<uint8_t> const& bytes)
	: value(std::nullopt)
{
	if (bytes.size() > 0) {
		value = std::make_optional<HashedBlock>();
		xdr::xdr_from_opaque(bytes, *value);
	}
}

} /* speedex */