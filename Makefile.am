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
	xdr/hotstuff.x \
	xdr/overlay.x

XH_FILES = $(X_FILES:.x=.h)

# main srcs

AUTOMATION_SRCS = \
	automation/experiment_control.cc

BLOCK_PROCESSING_SRCS = \
	block_processing/block_producer.cc \
	block_processing/block_validator.cc \
	block_processing/serial_transaction_processor.cc

CONFIG_SRCS = \
	config/replica_config.cc

CRYPTO_SRCS = \
	crypto/crypto_utils.cc

EXPERIMENTS_SRCS = \
	experiments/producer_node.cc \
	experiments/validator_node.cc \
	experiments/singlenode_init.cc

HEADER_HASH_SRCS = \
	header_hash/block_header_hash_map.cc

HEADER_HASH_TEST_SRCS = \
	header_hash/tests/test_header_hash_map.h

HOTSTUFF_SRCS = \
	hotstuff/block.cc \
	hotstuff/block_storage/io_utils.cc \
	hotstuff/block_storage/block_fetch_server.cc \
	hotstuff/block_storage/block_fetch_manager.cc \
	hotstuff/block_storage/block_fetch_worker.cc \
	hotstuff/block_storage/block_store.cc \
	hotstuff/block_storage/garbage_collector.cc \
	hotstuff/consensus.cc \
	hotstuff/crypto.cc \
	hotstuff/event.cc \
	hotstuff/event_queue.cc \
	hotstuff/hotstuff.cc \
	hotstuff/lmdb.cc \
	hotstuff/network_event.cc \
	hotstuff/network_event_queue.cc \
	hotstuff/protocol/hotstuff_protocol_client.cc \
	hotstuff/protocol/hotstuff_protocol_manager.cc \
	hotstuff/protocol/hotstuff_server.cc \
	hotstuff/vm/counting_vm.cc

HOTSTUFF_TEST_SRCS = \
	hotstuff/vm/tests/test_speculative_exec_gadget.h

LMDB_SRCS = \
	lmdb/lmdb_types.cc \
	lmdb/lmdb_wrapper.cc

MEMORY_DATABASE_SRCS = \
	memory_database/account_vector.cc \
	memory_database/memory_database.cc \
	memory_database/memory_database_view.cc \
	memory_database/user_account.cc

MEMORY_DATABASE_TEST_SRCS = \
	memory_database/tests/test_memory_database_lmdb.h

MEMPOOL_SRCS = \
	mempool/mempool.cc \
	mempool/mempool_cleaner.cc \
	mempool/mempool_transaction_filter.cc

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

OVERLAY_SRCS = \
	overlay/overlay_client.cc \
	overlay/overlay_client_manager.cc \
	overlay/overlay_flooder.cc \
	overlay/overlay_server.cc

PRICE_COMPUTATION_SRCS = \
	price_computation/lp_solver.cc \
	price_computation/normalization_rolling_average.cc \
	price_computation/tatonnement_oracle.cc

RPC_SRCS = \
	rpc/block_forwarder.cc \
	rpc/consensus_api.cc \
	rpc/consensus_api_server.cc

SIMPLEX_SRCS = \
	simplex/allocator.cc \
	simplex/bitcompressed_row.cc \
	simplex/simplex.cc \
	simplex/solver.cc \
	simplex/sparse.cc

SIMPLEX_TEST_SRCS = \
	simplex/tests/test_bitcompressed_row.h \
	simplex/tests/test_feasibility_solver.h \
	simplex/tests/test_simplex.h \
	simplex/tests/test_tableau.h

SPEEDEX_SRCS = \
	speedex/reload_from_hotstuff.cc \
	speedex/speedex_measurements.cc \
	speedex/speedex_operation.cc \
	speedex/speedex_options.cc \
	speedex/speedex_persistence.cc \
	speedex/vm/speedex_vm.cc \
	speedex/vm/speedex_vm_block_id.cc \
	speedex/vm/speedex_vm_init.cc

SYNTHETIC_DATA_GEN_SRCS = \
	synthetic_data_generator/synthetic_data_gen.cc \
	synthetic_data_generator/synthetic_data_gen_options.cc \
	synthetic_data_generator/synthetic_data_stream.cc

TRIE_TEST_SRCS = \
	trie/tests/test_prefix.h \
	trie/tests/test_merkle_trie.h \
	trie/tests/test_merkle_trie_metadata.h \
	trie/tests/test_recycling_trie.h

UTILS_SRCS = \
	utils/cleanup.cc \
	utils/header_persistence.cc \
	utils/manage_data_dirs.cc

SRCS = \
	$(AUTOMATION_SRCS) \
	$(BLOCK_PROCESSING_SRCS) \
	$(CONFIG_SRCS) \
	$(CRYPTO_SRCS) \
	$(HEADER_HASH_SRCS) \
	$(HOTSTUFF_SRCS) \
	$(LMDB_SRCS) \
	$(MEMORY_DATABASE_SRCS) \
	$(MEMPOOL_SRCS) \
	$(MODLOG_SRCS) \
	$(ORDERBOOK_SRCS) \
	$(OVERLAY_SRCS) \
	$(PRICE_COMPUTATION_SRCS) \
	$(SIMPLEX_SRCS) \
	$(SPEEDEX_SRCS) \
	$(SYNTHETIC_DATA_GEN_SRCS) \
	$(UTILS_SRCS)

MAIN_CCS = \
	main/clean_lmdbs.cc \
	main/counting_vm_hotstuff.cc \
	main/experiment_controller.cc \
	main/overlay_sim.cc \
	main/simple.cc \
	main/replicated_simulation_experiment_controller.cc \
	main/solver_comparison.cc \
	main/speedex_vm_hotstuff.cc \
	main/synthetic_data_gen_from_params.cc \
	main/trie_comparison.cc 

AM_CPPFLAGS = $(libcrypto_CFLAGS) $(gsl_CFLAGS) $(xdrpp_CFLAGS) $(libsodium_CFLAGS) $(LIBFYAML_CFLAGS) $(lmdb_CFLAGS)
LDADD = $(libcrypto_LIBS) $(gsl_LIBS) $(xdrpp_LIBS) -ltbb -lglpk $(libsodium_LIBS) $(LIBFYAML_LIBS) $(lmdb_LIBS)

$(SRCS:.cc=.o) : $(XH_FILES)
$(MAIN_CCS:.cc=.o) : $(XH_FILES)

TEST_SRCS = \
	$(HEADER_HASH_TEST_SRCS) \
	$(HOTSTUFF_TEST_SRCS) \
	$(MEMORY_DATABASE_TEST_SRCS) \
	$(SIMPLEX_TEST_SRCS) \
	$(TRIE_TEST_SRCS)

test_runner.cc : $(TEST_SRCS) $(XHFILES)
	cxxtestgen --error-printer -o test_runner.cc $(TEST_SRCS)

bin_PROGRAMS = \
	clean_lmdbs \
	counting_vm_hotstuff \
	experiment_controller \
	overlay_sim \
	simple \
	replicated_simulation_experiment_controller \
	solver_comparison \
	speedex_vm_hotstuff \
	synthetic_data_gen \
	trie_comparison \
	test

clean_lmdbs_SOURCES = $(SRCS) main/clean_lmdbs.cc
counting_vm_hotstuff_SOURCES = $(SRCS) main/counting_vm_hotstuff.cc
experiment_controller_SOURCES = $(SRCS) main/experiment_controller.cc
overlay_sim_SOURCES = $(SRCS) main/overlay_sim.cc
simple_SOURCES = $(SRCS) main/simple.cc
replicated_simulation_experiment_controller_SOURCES = $(SRCS) main/replicated_simulation_experiment_controller.cc
solver_comparison_SOURCES = $(SRCS) main/solver_comparison.cc
speedex_vm_hotstuff_SOURCES = $(SRCS) main/speedex_vm_hotstuff.cc
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

README: README.md

.PHONY: doc
doc:
	doxygen Doxyfile




