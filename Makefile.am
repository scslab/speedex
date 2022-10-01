ACLOCAL_AMFLAGS = -I m4

SUBDIRS = $(PKGCONFIG_SUBDIRS)

CLEANFILES = *~ .*~

X_FILES = \
	xdr/types.x \
	xdr/transaction.x \
	xdr/block.x \
	xdr/database_commitments.x \
	xdr/trie_proof.x \
	xdr/consensus_api.x \
	xdr/cryptocoin_experiment.x \
	xdr/experiments.x \
	xdr/overlay.x

XH_FILES = $(X_FILES:.x=.h)

# main srcs

AUTOMATION_SRCS = \
	automation/experiment_control.cc

BLOCK_PROCESSING_SRCS = \
	block_processing/block_producer.cc \
	block_processing/block_validator.cc \
	block_processing/serial_transaction_processor.cc

CDA_SRCS = \
	cda/serial_ob.cc \
	cda/serial_ob_experiment.cc

CONFIG_SRCS = \
	config/replica_config.cc

CRYPTO_SRCS = \
	crypto/crypto_utils.cc

EXPERIMENTS_SRCS = \
	experiments/tatonnement_sim_experiment.cc

FILTERING_SRCS = \
	filtering/account_filter_entry.cc \
	filtering/filter_log.cc
 
HEADER_HASH_SRCS = \
	header_hash/block_header_hash_map.cc

HEADER_HASH_TEST_SRCS = \
	header_hash/tests/test_header_hash_map.cc

LMDB_SRCS = \
	lmdb/lmdb_types.cc \
	lmdb/lmdb_wrapper.cc

MEMORY_DATABASE_SRCS = \
	memory_database/account_lmdb.cc \
	memory_database/account_vector.cc \
	memory_database/memory_database.cc \
	memory_database/memory_database_view.cc \
	memory_database/sequence_tracker.cc \
	memory_database/thunk.cc \
	memory_database/user_account.cc

MEMORY_DATABASE_TEST_SRCS = \
	memory_database/tests/test_memory_database_lmdb.cc \
	memory_database/tests/test_seqno_gadget.cc

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
	orderbook/orderbook_manager.cc \
	orderbook/orderbook_manager_view.cc

ORDERBOOK_TEST_SRCS = \
	orderbook/tests/test_demand_calc.cc

OVERLAY_SRCS = \
	overlay/overlay_client.cc \
	overlay/overlay_client_manager.cc \
	overlay/overlay_flooder.cc \
	overlay/overlay_server.cc

PRICE_COMPUTATION_SRCS = \
	price_computation/lp_solver.cc \
	price_computation/normalization_rolling_average.cc \
	price_computation/tatonnement_oracle.cc

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
	speedex/autorollback_validation_structures.cc \
	speedex/reload_from_hotstuff.cc \
	speedex/speedex_management_structures.cc \
	speedex/speedex_measurements.cc \
	speedex/speedex_operation.cc \
	speedex/speedex_options.cc \
	speedex/speedex_persistence.cc \
	speedex/vm/speedex_vm.cc \
	speedex/vm/speedex_vm_init.cc

SYNTHETIC_DATA_GEN_SRCS = \
	synthetic_data_generator/synthetic_data_gen.cc \
	synthetic_data_generator/synthetic_data_gen_options.cc \
	synthetic_data_generator/synthetic_data_stream.cc

UTILS_SRCS = \
	utils/cleanup.cc \
	utils/header_persistence.cc \
	utils/manage_data_dirs.cc \
	utils/save_load_xdr.cc

SRCS = \
	$(AUTOMATION_SRCS) \
	$(BLOCK_PROCESSING_SRCS) \
	$(CDA_SRCS) \
	$(CONFIG_SRCS) \
	$(CRYPTO_SRCS) \
	$(EXPERIMENTS_SRCS) \
	$(FILTERING_SRCS) \
	$(HEADER_HASH_SRCS) \
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

GENERIC_CCS = \
	generic/counting_vm.cc

MAIN_CCS = \
	main/blockstm_comparison.cc \
	main/cda_experiment.cc \
	main/clean_lmdbs.cc \
	main/counting_vm_hotstuff.cc \
	main/cryptocoin_dataset_gen.cc \
	main/exchange_data_experiment.cc \
	main/experiment_controller.cc \
	main/filtering_experiment.cc \
	main/filtering_experiment_gen.cc \
	main/overlay_sim.cc \
	main/solver_comparison.cc \
	main/speedex_vm_hotstuff.cc \
	main/synthetic_data_gen_from_params.cc \
	main/tatonnement_experiment_data_gen_from_params.cc \
	main/tatonnement_mega_graph.cc \
	main/test.cc \
	main/trie_comparison.cc 

AM_CPPFLAGS = \
	$(libcrypto_CFLAGS) \
	$(gsl_CFLAGS) \
	$(xdrpp_CFLAGS) \
	$(libsodium_CFLAGS) \
	$(LIBFYAML_CFLAGS) \
	$(tbb_CFLAGS) \
	$(lmdb_CFLAGS) \
	$(lemon_CFLAGS) \
	$(merkle_trie_toolkit_CFLAGS) \
	$(hotstuff_CFLAGS)

LDADD = \
	$(libcrypto_LIBS) \
	$(gsl_LIBS) \
	$(xdrpp_LIBS) \
	$(libsodium_LIBS) \
	$(LIBFYAML_LIBS) \
	$(tbb_LIBS) \
	$(lmdb_LIBS) \
	$(lemon_LIBS) \
	$(merkle_trie_toolkit_LIBS) \
	$(hotstuff_LIBS) \
	-lglpk

CATCH_TEST_CCS = \
	filtering/tests/test_filter_entry.cc \
	$(HEADER_HASH_TEST_SRCS) \
	$(MEMORY_DATABASE_TEST_SRCS) \
	$(ORDERBOOK_TEST_SRCS)


$(CATCH_TEST_SRCS:.cc=.o) : CXXFLAGS += $(Catch2_CFLAGS)
main/test.o : CXXFLAGS += $(Catch2_CFLAGS)

$(SRCS:.cc=.o) : $(XH_FILES)
$(MAIN_CCS:.cc=.o) : $(XH_FILES)
$(GENERIC_CCS:.cc=.o) : $(XH_FILES)
$(CATCH_TEST_CCS:.cc=.o) : $(XH_FILES)

TEST_SRCS = \
	$(SIMPLEX_TEST_SRCS)

test_runner.cc : $(TEST_SRCS) $(XHFILES)
	cxxtestgen --error-printer -o test_runner.cc $(TEST_SRCS)

bin_PROGRAMS = \
	blockstm_comparison \
	cda_experiment \
	clean_lmdbs \
	counting_vm_hotstuff \
	cryptocoin_dataset_gen \
	exchange_data_experiment \
	experiment_controller \
	filtering_experiment \
	filtering_experiment_gen \
	overlay_sim \
	solver_comparison \
	speedex_vm_hotstuff \
	synthetic_data_gen \
	tatonnement_experiment_data_gen \
	tatonnement_mega_graph \
	trie_comparison \
	test \
	ctest

blockstm_comparison_SOURCES = $(SRCS) main/blockstm_comparison.cc
cda_experiment_SOURCES = $(SRCS) main/cda_experiment.cc
clean_lmdbs_SOURCES = $(SRCS) main/clean_lmdbs.cc
counting_vm_hotstuff_SOURCES = $(SRCS) $(GENERIC_CCS) main/counting_vm_hotstuff.cc
cryptocoin_dataset_gen_SOURCES = $(SRCS) main/cryptocoin_dataset_gen.cc
exchange_data_experiment_SOURCES = $(SRCS) main/exchange_data_experiment.cc
experiment_controller_SOURCES = $(SRCS) main/experiment_controller.cc
filtering_experiment_SOURCES = $(SRCS) main/filtering_experiment.cc
filtering_experiment_gen_SOURCES = $(SRCS) main/filtering_experiment_gen.cc
overlay_sim_SOURCES = $(SRCS) main/overlay_sim.cc
solver_comparison_SOURCES = $(SRCS) main/solver_comparison.cc
speedex_vm_hotstuff_SOURCES = $(SRCS) main/speedex_vm_hotstuff.cc
synthetic_data_gen_SOURCES = $(SRCS) main/synthetic_data_gen_from_params.cc
tatonnement_experiment_data_gen_SOURCES = $(SRCS) main/tatonnement_experiment_data_gen_from_params.cc
tatonnement_mega_graph_SOURCES = $(SRCS) main/tatonnement_mega_graph.cc
trie_comparison_SOURCES = $(SRCS) main/trie_comparison.cc

test_SOURCES = $(SRCS) test_runner.cc
ctest_SOURCES = $(SRCS) $(CATCH_TEST_CCS) main/test.cc

ctest_LDADD = $(LDADD) $(Catch2_LIBS)

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




