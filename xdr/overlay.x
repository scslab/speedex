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


#if !(defined(XDRC_PXDI) || defined(XDRC_PXD) || defined(XDRC_PYX))

%#include "xdr/types.h"
%#include "xdr/block.h"

namespace speedex {

typedef opaque ForwardingTxs<>;

program Overlay {
	version OverlayV1 {
		uint64 mempool_size(void) = 1;
		void forward_txs(ForwardingTxs, uint32, ReplicaID) = 2;
	} = 1;
} = 0x11111120;

} /* speedex */

#endif