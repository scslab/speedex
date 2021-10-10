
#if !(defined(XDRC_PXDI) || defined(XDRC_PXD) || defined(XDRC_PYX))

%#include "xdr/types.h"
%#include "xdr/block.h"

namespace hotstuff {

typedef speedex::uint32 ReplicaID;
typedef speedex::uint32 ReplicaIDBitMap;

const MAX_REPLICAS = 32; // max length of bitmap.  More replicas => longer bitmap

struct PartialCertificateWire {
	speedex::Hash hash;
	speedex::Signature sig;
};

struct QuorumCertificateWire {
	speedex::Hash justify;
	ReplicaIDBitMap bmp;
	speedex::Signature sigs<MAX_REPLICAS>;
};

struct HeaderDataPair {
	speedex::HashedBlock header;
	speedex::TransactionData body;
};

struct HotstuffBlockHeader {
	speedex::Hash parent_hash;
	QuorumCertificateWire qc; // qc.justify need not be the direct parent
	// body_hash included for convenience.  We can hash a "block" by just hashing the header.
	speedex::Hash body_hash; // hash of HotstuffBlockHeader::body
};

struct HotstuffBlockWire {
	HotstuffBlockHeader header;
	
	opaque body<>; // serialized HeaderDataPair
};

struct BlockFetchRequest {
	speedex::Hash reqs<>;
};

struct BlockFetchResponse {
	HotstuffBlockWire responses<>;
};

program FetchBlocks {
	version FetchBlocksV1 {
		 BlockFetchResponse fetch(BlockFetchRequest) = 1;
	} = 1;
} = 0x11111114;

} /* hotstuff */

#endif