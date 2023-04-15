# SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized EXchange

This repository contains the standalone implementation of SPEEDEX.  For a detailed description
 of the design,
please refer to our NSDI'23 paper (https://arxiv.org/pdf/2111.02719.pdf).

An alternative implementation of SPEEDEX, prototyped as a component of the Stellar blockchain,
can be found at (https://github.com/gramseyer/stellar-core).  

## Repository Layout

The core algorithmic challenge of this work lies in efficiently computing
equilibria in linear Arrow-Debreu exchange markets.
Our algorithms are implemented under `price_computation/`

System performance relies heavily on an efficient Merkle-Patricia trie implementation,
implemented in `lib/merkle_trie_toolkit/` and used (to maintain orderbooks) under `orderbook/`
and (to maintain account state) under `memory_database/`.

Systems for assembling and validating blocks of transactions are under `block_processing/`.

# License
GPLv3 dependency follows from the linear programming library (GLPK).
Future versions will be Apache2.
