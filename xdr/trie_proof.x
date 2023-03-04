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
namespace speedex {
	
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