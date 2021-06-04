#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_xdr cimport *
%from types_includes cimport *
%from trie_proof_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from trie_proof_includes cimport *
#endif
namespace edce {
	
	struct ProofNode {
		opaque prefix_length_and_bv[4];
		Hash hashes<16>;
	};

	struct Proof {
		ProofNode nodes<>;
		opaque prefix<>;

		uint32 trie_size;
		Hash root_node_hash;
		
		opaque value_bytes<>;
		uint32 membership_flag;
	};
}