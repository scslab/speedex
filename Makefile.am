ACLOCAL_AMFLAGS = -I m4

SUBDIRS = $(PKGCONFIG_SUBDIRS)

CLEANFILES = *~ .*~

X_FILES = xdr/types.x \
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

all-local: xdrpy_module
