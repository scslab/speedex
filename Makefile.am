ACLOCAL_AMFLAGS = -I m4

SUBDIRS = $(PKGCONFIG_SUBDIRS)

CLEANFILES = *~ .*~

X_FILES = \
	xdr/types.x \
	xdr/transaction.x \
	xdr/ledger.x \
	xdr/block.x \
	xdr/database_commitments.x \
	xdr/trie_proof.x \
	xdr/iblt_wire.x \
	xdr/consensus_api.x \
	xdr/cryptocoin_experiment.x \
	xdr/header_summary.x \
	xdr/experiments.x \
	xdr/hotstuff.x

XH_FILES = $(X_FILES:.x=.h)

# main srcs

BLOCK_PROCESSING_SRCS = \
	block_processing/block_producer.cc \
	block_processing/block_validator.cc \
	block_processing/serial_transaction_processor.cc

CRYPTO_SRCS = \
	crypto/crypto_utils.cc

EXPERIMENTS_SRCS = \
	experiments/producer_node.cc \
	experiments/validator_node.cc \
	experiments/singlenode_init.cc

HEADER_HASH_SRCS = \
	header_hash/block_header_hash_map.cc

HOTSTUFF_SRCS = \
	hotstuff/block.cc \
	hotstuff/block_storage/io_utils.cc \
	hotstuff/block_storage/block_fetch_server.cc \
	hotstuff/block_storage/block_fetch_manager.cc \
	hotstuff/block_storage/block_fetch_worker.cc \
	hotstuff/block_storage/block_store.cc \
	hotstuff/consensus.cc \
	hotstuff/crypto.cc \
	hotstuff/event.cc \
	hotstuff/event_queue.cc \
	hotstuff/hotstuff.cc \
	hotstuff/network_event.cc \
	hotstuff/network_event_queue.cc \
	hotstuff/protocol/hotstuff_protocol_client.cc \
	hotstuff/protocol/hotstuff_protocol_manager.cc \
	hotstuff/protocol/hotstuff_server.cc \
	hotstuff/replica_config.cc

HOTSTUFF_TEST_SRCS = \
	hotstuff/vm/tests/test_height_map_gadget.h

LMDB_SRCS = \
	lmdb/lmdb_types.cc \
	lmdb/lmdb_wrapper.cc

MEMORY_DATABASE_SRCS = \
	memory_database/memory_database.cc \
	memory_database/memory_database_view.cc \
	memory_database/user_account.cc

MEMPOOL_SRCS = \
	mempool/mempool.cc \
	mempool/mempool_cleaner.cc

MODLOG_SRCS = \
	modlog/account_modification_log.cc \
	modlog/file_prealloc_worker.cc \
	modlog/log_entry_fns.cc \
	modlog/log_merge_worker.cc

ORDERBOOK_SRCS = \
	orderbook/commitment_checker.cc \
	orderbook/lmdb.cc \
	orderbook/offer_clearing_params.cc \
	orderbook/orderbook.cc \
	orderbook/orderbook_manager.cc

PRICE_COMPUTATION_SRCS = \
	price_computation/lp_solver.cc \
	price_computation/normalization_rolling_average.cc \
	price_computation/tatonnement_oracle.cc

RPC_SRCS = \
	rpc/block_forwarder.cc \
	rpc/consensus_api.cc \
	rpc/consensus_api_server.cc

SPEEDEX_SRCS = \
	speedex/speedex_measurements.cc \
	speedex/speedex_node.cc \
	speedex/speedex_operation.cc \
	speedex/speedex_options.cc \
	speedex/speedex_persistence.cc

SYNTHETIC_DATA_GEN_SRCS = \
	synthetic_data_generator/synthetic_data_gen.cc \
	synthetic_data_generator/synthetic_data_gen_options.cc

TRIE_TEST_SRCS = \
	trie/tests/test_prefix.h

UTILS_SRCS = \
	utils/cleanup.cc \
	utils/header_persistence.cc

SRCS = \
	$(BLOCK_PROCESSING_SRCS) \
	$(CRYPTO_SRCS) \
	$(EXPERIMENTS_SRCS) \
	$(HEADER_HASH_SRCS) \
	$(HOTSTUFF_SRCS) \
	$(LMDB_SRCS) \
	$(MEMORY_DATABASE_SRCS) \
	$(MEMPOOL_SRCS) \
	$(MODLOG_SRCS) \
	$(ORDERBOOK_SRCS) \
	$(PRICE_COMPUTATION_SRCS) \
	$(RPC_SRCS) \
	$(SPEEDEX_SRCS) \
	$(SYNTHETIC_DATA_GEN_SRCS) \
	$(UTILS_SRCS)

MAIN_CCS = main/simple.cc \
	main/replicated_simulation_experiment_controller.cc \
	main/speedex_producer_experiment.cc \
	main/speedex_validator_experiment.cc \
	main/synthetic_data_gen_from_params.cc \
	main/trie_comparison.cc 

AM_CPPFLAGS = $(libcrypto_CFLAGS) $(gsl_CFLAGS) $(xdrpp_CFLAGS) $(libsodium_CFLAGS) $(LIBFYAML_CFLAGS) $(lmdb_CFLAGS)
LDADD = $(libcrypto_LIBS) $(gsl_LIBS) $(xdrpp_LIBS) -ltbb -lglpk $(libsodium_LIBS) $(LIBFYAML_LIBS) $(lmdb_LIBS)

$(SRCS:.cc=.o) : $(XH_FILES)
$(MAIN_CCS:.cc=.o) : $(XH_FILES)

TEST_SRCS = \
	$(HOTSTUFF_TEST_SRCS) \
	$(TRIE_TEST_SRCS)

test_runner.cc : $(TEST_SRCS) $(XHFILES)
	cxxtestgen --error-printer -o test_runner.cc $(TEST_SRCS)

bin_PROGRAMS = simple \
	replicated_simulation_experiment_controller \
	speedex_producer_experiment \
	speedex_validator_experiment \
	synthetic_data_gen \
	trie_comparison \
	test

simple_SOURCES = $(SRCS) main/simple.cc
replicated_simulation_experiment_controller_SOURCES = $(SRCS) main/replicated_simulation_experiment_controller.cc
speedex_producer_experiment_SOURCES = $(SRCS) main/speedex_producer_experiment.cc
speedex_validator_experiment_SOURCES = $(SRCS) main/speedex_validator_experiment.cc
synthetic_data_gen_SOURCES = $(SRCS) main/synthetic_data_gen_from_params.cc
trie_comparison_SOURCES = $(SRCS) main/trie_comparison.cc

test_SOURCES = $(SRCS) test_runner.cc

#building x files
$(XH_FILES) : $(XDRC)

SUFFIXES = .x .h

.x.h:
	$(XDRC)  -hh --hhpy -o $@ $<

PYX_FILES = $(X_FILES:.x=_xdr.pyx)
PXD_FILES = $(X_FILES:.x=_xdr.pxd)
PXD_INCLUDE_FILES = $(X_FILES:.x=_includes.pxd)
PYXDR_CPP_FILES = $(X_FILES:.x=_xdr.cpp)

clean-local:
	$(RM) $(PYX_FILES) $(PXD_FILES) $(PXD_INCLUDE_FILES) $(PYXDR_CPP_FILES) $(XH_FILES) test_runner.cc

%_xdr.pyx : %.x $(XDRC) $(X_FILES)
	$(XDRC) --pyx $< -o $(basename $@).pyx

%_xdr.pxd : %.x $(XDRC) $(X_FILES)
	$(XDRC) --pxd $< -o $(basename $@).pxd

%_includes.pxd : %.x $(XDRC) $(X_FILES)
	$(XDRC) --pxdi $< -o $(basename $@).pxd

xdrpy_module: $(XDRC) $(PYX_FILES) $(PXD_FILES) $(PXD_INCLUDE_FILES) $(X_FILES:.x=.h)
	python3 setup.py build_ext --inplace --makeflags="$(MAKEFLAGS)" --pyx_files="$(PYX_FILES)"

all-local: xdrpy_module $(SRCS:.cc=.o)

.PHONY: doc
doc:
	doxygen Doxyfile




