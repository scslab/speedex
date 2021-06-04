#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_includes cimport *
%from types_xdr cimport *
%from iblt_wire_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from iblt_wire_includes cimport *
#endif

namespace edce {

struct IBLTWireFormat {
	opaque rawData<>;
	uint32 cellCounts<>;
};

}