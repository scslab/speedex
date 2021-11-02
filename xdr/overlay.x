
#if !(defined(XDRC_PXDI) || defined(XDRC_PXD) || defined(XDRC_PYX))

%#include "xdr/types.h"
%#include "xdr/block.h"

namespace speedex {

typedef opaque ForwardingTxs<>;

program Overlay {
	version OverlayV1 {
		uint64 mempool_size(void) = 1;
		void forward_txs(ForwardingTxs) = 2;
	} = 1;
} = 0x11111120;

} /* speedex */

#endif