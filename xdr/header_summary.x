#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_includes cimport *
%from types_xdr cimport *
%from header_summary_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from header_summary_includes cimport *
#endif

namespace edce
{

struct HeaderSummary {
	float prices<>;
	float activated_supplies<>;
	uint32 tat_timeout; // 1 if yes, 0 if no
	uint64 last_mempool_block;
};

struct ExperimentSummary {
	HeaderSummary headers<>;
};

} /* edce */
