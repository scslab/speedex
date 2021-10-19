#include "speedex/speedex_vm_block_id.h"

using xdr::operator==;

namespace speedex {

bool
SpeedexVMBlockID::operator== (const SpeedexVMBlockID& other) const {
	if ((!value) && (!other.value)) {
		return true;
	}
	if (((bool)value) && ((bool) other.value)) {
		return (*value).hash == (*(other.value)).hash;
	}
	return false;
}

} /* speedex */