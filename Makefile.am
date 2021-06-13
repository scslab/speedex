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
	xdr/experiments.x

XH_FILES = $(X_FILES:.x=.h)

# main srcs

HEADER_HASH_SRCS = \
	header_hash/block_header_hash_map.cc

LMDB_SRCS = \
	lmdb/lmdb_types.cc \
	lmdb/lmdb_wrapper.cc

MEMORY_DATABASE_SRCS = \
	memory_database/memory_database.cc \
	memory_database/memory_database_view.cc \ 
	memory_database/user_account.cc

MODLOG_SRCS = \
	modlog/account_modification_log.cc \
	modlog/file_prealloc_worker.cc \
	modlog/log_entry_fns.cc

ORDERBOOK_SRCS = \
	orderbook/commitment_checker.cc \
	orderbook/lmdb.cc \
	orderbook/orderbook.cc \
	orderbook/orderbook_manager.cc

PRICE_COMPUTATION_SRCS = \
	price_computation/lp_solver.cc \
	price_computation/tatonnement_oracle.cc

UTILS_SRCS = \
	utils/cleanup.cc \
	utils/header_persistence.cc

SRCS = \
	$(HEADER_HASH_SRCS) \
	$(LMDB_SRCS) \
	$(MEMORY_DATABASE_SRCS) \
	$(MODLOG_SRCS) \
	$(ORDERBOOK_SRCS) \
	$(PRICE_COMPUTATION_SRCS) \
	$(UTILS_SRCS)

MAIN_CCS = main/simple.cc \
	main/trie_comparison.cc

AM_CPPFLAGS = $(libcrypto_CFLAGS) $(gsl_CFLAGS) $(xdrpp_CFLAGS) $(libsodium_CFLAGS) $(LIBFYAML_CFLAGS) $(lmdb_CFLAGS)
LDADD = $(libcrypto_LIBS) $(gsl_LIBS) $(xdrpp_LIBS) -ltbb -lglpk $(libsodium_LIBS) $(LIBFYAML_LIBS) $(lmdb_LIBS)

$(SRCS:.cc=.o) : $(XH_FILES)
$(MAIN_CCS:.cc=.o) : $(XH_FILES)

bin_PROGRAMS = simple trie_comparison

simple_SOURCES = $(SRCS) main/simple.cc
trie_comparison_SOURCES = $(SRCS) main/trie_comparison.cc

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
	$(RM) $(PYX_FILES) $(PXD_FILES) $(PXD_INCLUDE_FILES) $(PYXDR_CPP_FILES) $(XH_FILES)

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



