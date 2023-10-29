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

# Compilation Instructions

SPEEDEX depends on several libraries, included here as git submodules
(i.e. clone the repository with `git clone --recurse-submodules`).

SPEEDEX requires a c++20-capable compiler. 
I have built it on an Intel Mac (MacOS 14.1) and
on Ubuntu 22 with GCC 13.2.
Please let me know if you encounter problems on other systems.

This project uses Autotools for its build system.
That is,
`./autogen.sh && ./configure && make`

The configure script should identify any missing dependencies, *with one exception*.
SPEEDEX uses the GNU Linear Programming Kit (GLPK), which does not include a pkg-config (.pc)
file in some (perhaps all) package managers.  The software is available at (https://www.gnu.org/software/glpk/),
and this project works (at least) on version 5.0.
This must be installed before building SPEEDEX.

`make test` builds the tests.

