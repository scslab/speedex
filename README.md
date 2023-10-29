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

# Running Experiments

`blockstm_comparison` runs the payment experiments comparing SPEEDEX to Block-STM (https://arxiv.org/abs/2203.06871),
as in Figure 7 of our paper.

Let `X<=65535` be a a reasonable upper bound on the number of transactions per account in one block (i.e., for a
block with 2 accounts and 100'000 accounts per block, use to 65'535)
The binary should be built after running 
`./configure -D_MAX_SEQ_NUMS_PER_BLOCK=X -D_DISABLE_PRICE_COMPUTATION -D_DISABLE_TATONNEMENT_TIMEOUT -D_NUM_ACCOUNT_DB_SHARDS=1`

The binary will print its own configuration data before running.
Figure 7 was produced with the following configuration.

```
========== trie configs ==========
TRIE_LOG_HASH_RECORDS         = 0
========== static configs ==========
USE_TATONNEMENT_TIMEOUT_THREAD = 0
DISABLE_PRICE_COMPUTATION      = 1
DISABLE_LMDB                   = 0
DETAILED_MOD_LOGGING           = 1
PREALLOC_BLOCK_FILES           = 1
ACCOUNT_DB_SYNC_IMMEDIATELY    = 0
MAX_SEQ_NUMS_PER_BLOCK         = 60000
LOG_TRANSFERS                  = 0
NUM_ACCOUNT_DB_SHARDS          = 1
====================================
initialized sodium
```







