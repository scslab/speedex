
#if !(defined(XDRC_PXDI) || defined(XDRC_PXD) || defined(XDRC_PYX))

%#include "xdr/types.h"
%#include "xdr/block.h"

namespace hotstuff {

typedef speedex::uint32 ReplicaIDBitMap;

struct PartialCertificateWire {
	speedex::Hash hash;
	speedex::Signature sig;
};

struct QuorumCertificateWire {
	speedex::Hash justify;
	ReplicaIDBitMap bmp;
	speedex::Signature sigs<speedex::MAX_REPLICAS>;
};

struct HotstuffBlockHeader {
	speedex::Hash parent_hash;
	QuorumCertificateWire qc; // qc.justify need not be the direct parent
	// body_hash included for convenience.  We can hash a "block" by just hashing the header.
	speedex::Hash body_hash; // hash of HotstuffBlockHeader::body
};

struct HotstuffBlockWire {
	HotstuffBlockHeader header;
	
	opaque body<>; // serialized vm block (for speedex, HashedBlockTransactionListPair)
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

struct VoteMessage {
	PartialCertificateWire vote;
	speedex::ReplicaID voter;
};

struct ProposeMessage {
	HotstuffBlockWire proposal;
	speedex::ReplicaID proposer;
};

program HotstuffProtocol {
	version HotstuffProtocolV1 {
		void vote(VoteMessage) = 1;
		void propose(ProposeMessage) = 2;
	} = 1;
} = 0x11111115;

} /* hotstuff */

#endif