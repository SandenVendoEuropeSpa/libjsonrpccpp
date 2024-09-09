ACLOCAL_AMFLAGS=-I m4

SUBDIRS=src test

if INSTALL_DOCUMENTATION
    SUBDIRS+=doc
endif

if INSTALL_EXAMPLES
    SUBDIRS+=examples
endif

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = jsonrpccpp.pc