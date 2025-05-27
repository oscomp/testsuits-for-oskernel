# SPDX-License-Identifier: GPL-2.0-or-later
VERSION = 2.7
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
HOST=$(patsubst %-gcc,%,$(CC))
OBJDIR = bld

sources = cyclictest.c \
	  hackbench.c \
	  pmqtest.c \
	  ptsematest.c \
	  rt-migrate-test.c \
	  signaltest.c \
	  sigwaittest.c \
	  svsematest.c  \
	  cyclicdeadline.c \
	  deadline_test.c \
	  queuelat.c \
	  ssdd.c \
	  oslat.c

TARGETS = $(sources:.c=)
LIBS	= -lrt -lpthread
RTTESTLIB = -lrttest -L$(OBJDIR)
EXTRA_LIBS ?= -ldl	# for get_cpu

ifeq ($(CROSS_COMPILE), riscv64-buildroot-linux-musl-)
LDFLAGS1 +=  -latomic -static
endif

ifeq ($(CROSS_COMPILE), riscv64-linux-gnu-)
LDFLAGS1 +=  -latomic -static
endif

RTTESTNUMA = -lrttestnuma  ./numactl-2.0.14/.libs/libnuma.a $(LDFLAGS1)
DESTDIR	?=
prefix  ?= /usr/local
bindir  ?= $(prefix)/bin
mandir	?= $(prefix)/share/man

CFLAGS ?= -Wall -Wno-nonnull -Wextra -Wno-sign-compare  -I./numactl-2.0.14
ifeq ($(CROSS_COMPILE), loongarch64-linux-musl-)
    CFLAGS += -DLOONGARCH_MUSL
endif

ifeq ($(CROSS_COMPILE), riscv64-buildroot-linux-musl-)
    CFLAGS1 += -I/opt/riscv64-linux-musl-cross/riscv64-linux-musl/include/
    CFLAGS += -DLOONGARCH_MUSL
endif
CPPFLAGS += -D_GNU_SOURCE -Isrc/include
LDFLAGS ?=


PYLIB ?= $(shell python3 -m get_pylib)

# Check for errors, such as python3 not available
ifeq (${PYLIB},)
	undefine PYLIB
endif

MANPAGES = src/cyclictest/cyclictest.8 \
	   src/pi_tests/pi_stress.8 \
	   src/ptsematest/ptsematest.8 \
	   src/rt-migrate-test/rt-migrate-test.8 \
	   src/sigwaittest/sigwaittest.8 \
	   src/svsematest/svsematest.8 \
	   src/pmqtest/pmqtest.8 \
	   src/hackbench/hackbench.8 \
	   src/signaltest/signaltest.8 \
	   src/pi_tests/pip_stress.8 \
	   src/queuelat/queuelat.8 \
	   src/queuelat/determine_maximum_mpps.8 \
	   src/sched_deadline/deadline_test.8 \
	   src/ssdd/ssdd.8 \
	   src/sched_deadline/cyclicdeadline.8 \
	   src/oslat/oslat.8

ifdef PYLIB
	MANPAGES += src/cyclictest/get_cyclictest_snapshot.8 \
	src/hwlatdetect/hwlatdetect.8
endif

ifeq ($(MAN_COMPRESSION),gzip)
	MANPAGES := $(MANPAGES:.8=.8.gz)
else ifeq ($(MAN_COMPRESSION),bzip2)
	MANPAGES := $(MANPAGES:.8=.8.bz2)
endif

ifndef DEBUG
	CFLAGS	+= -O2 -g
else
	CFLAGS	+= -O0 -g
endif

# We make some gueses on how to compile rt-tests based on the machine type
# and the ostype. These can often be overridden.
dumpmachine := $(shell $(CC) -dumpmachine)

# The ostype is typically something like linux or android
ostype := $(lastword $(subst -, ,$(dumpmachine)))

machinetype := $(shell echo $(dumpmachine)| \
    sed -e 's/-.*//' -e 's/i.86/i386/' -e 's/mips.*/mips/' -e 's/ppc.*/powerpc/')

include src/arch/android/Makefile

VPATH	= src/cyclictest:
VPATH	+= src/signaltest:
#VPATH	+= src/pi_tests:
VPATH	+= src/rt-migrate-test:
VPATH	+= src/ptsematest:
VPATH	+= src/sigwaittest:
VPATH	+= src/svsematest:
VPATH	+= src/pmqtest:
VPATH	+= src/lib:
VPATH	+= src/hackbench:
VPATH	+= src/sched_deadline:
VPATH	+= src/queuelat:	
VPATH	+= src/ssdd:
VPATH	+= src/oslat:


$(OBJDIR)/%.o: %.c | $(OBJDIR) 
	$(CC) -D VERSION=$(VERSION) -c $< $(CFLAGS) $(CFLAGS1) $(CPPFLAGS) -o $@

# Pattern rule to generate dependency files from .c files
$(OBJDIR)/%.d: %.c | $(OBJDIR)
	@$(CC) -MM $(CFLAGS) $(CPPFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@ || rm -f $@

.PHONY: all
all: extract_numactl $(TARGETS) hwlatdetect get_cyclictest_snapshot | $(OBJDIR)

.PHONY: extract_numactl
extract_numactl:
	rm -fr numactl-2.0.14
	tar zxf numactl-2.0.14.tar.gz
	echo $(HOST)
	cd numactl-2.0.14 && ./configure --host=$(HOST) && CC=$(CC) make -j 10

$(OBJDIR):
	mkdir $(OBJDIR)

# Include dependency files, automatically generate them if needed.
-include $(addprefix $(OBJDIR)/,$(sources:.c=.d))

cyclictest: $(OBJDIR)/cyclictest.o $(OBJDIR)/librttest.a $(OBJDIR)/librttestnuma.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(RTTESTNUMA)

cyclicdeadline: $(OBJDIR)/cyclicdeadline.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

deadline_test: $(OBJDIR)/deadline_test.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

signaltest: $(OBJDIR)/signaltest.o $(OBJDIR)/librttest.a $(OBJDIR)/librttestnuma.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(RTTESTNUMA)

pi_stress: $(OBJDIR)/pi_stress.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

hwlatdetect:  src/hwlatdetect/hwlatdetect.py
	chmod +x src/hwlatdetect/hwlatdetect.py
	ln -s src/hwlatdetect/hwlatdetect.py hwlatdetect

get_cyclictest_snapshot: src/cyclictest/get_cyclictest_snapshot.py
	chmod +x src/cyclictest/get_cyclictest_snapshot.py
	ln -s src/cyclictest/get_cyclictest_snapshot.py get_cyclictest_snapshot

rt-migrate-test: $(OBJDIR)/rt-migrate-test.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

ptsematest: $(OBJDIR)/ptsematest.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(EXTRA_LIBS)

sigwaittest: $(OBJDIR)/sigwaittest.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(EXTRA_LIBS)

svsematest: $(OBJDIR)/svsematest.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(EXTRA_LIBS)

pmqtest: $(OBJDIR)/pmqtest.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(EXTRA_LIBS)


hackbench: $(OBJDIR)/hackbench.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

queuelat: $(OBJDIR)/queuelat.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

ssdd: $(OBJDIR)/ssdd.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

oslat: $(OBJDIR)/oslat.o $(OBJDIR)/librttest.a $(OBJDIR)/librttestnuma.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(RTTESTNUMA)

%.8.gz: %.8
	gzip -nc $< > $@

%.8.bz2: %.8
	bzip2 -c $< > $@

LIBOBJS =$(addprefix $(OBJDIR)/,rt-error.o rt-get_cpu.o rt-sched.o rt-utils.o \
	histogram.o)
$(OBJDIR)/librttest.a: $(LIBOBJS)
	$(AR) rcs $@ $^

LIBNUMAOBJS =$(addprefix $(OBJDIR)/,rt-numa.o)
$(OBJDIR)/librttestnuma.a: $(LIBNUMAOBJS)
	pwd
	$(AR) rcs $@ $^

CLEANUP  = $(TARGETS) *.o .depend *.*~ *.orig *.rej *.d *.a *.8.gz *.8.bz2
CLEANUP += $(if $(wildcard .git), ChangeLog)

.PHONY: clean
clean:
	for F in $(CLEANUP); do find -type f -name $$F | xargs rm -f; done
	rm -f rt-tests-*.tar
	rm -f hwlatdetect
	rm -f get_cyclictest_snapshot
	rm -f tags

RPMDIRS = BUILD BUILDROOT RPMS SRPMS SPECS
.PHONY: distclean
distclean: clean
	rm -rf $(RPMDIRS) releases *.tar.gz *.tar.asc tmp

.PHONY: rebuild
rebuild:
	$(MAKE) clean
	$(MAKE) all

.PHONY: install
install: all install_manpages install_hwlatdetect install_get_cyclictest_snapshot
	mkdir -p "$(DESTDIR)$(bindir)"
	cp $(TARGETS) "$(DESTDIR)$(bindir)"
	install src/queuelat/determine_maximum_mpps.sh "${DESTDIR}${bindir}"

.PHONY: install_hwlatdetect
install_hwlatdetect: hwlatdetect
	if test -n "$(PYLIB)" ; then \
		mkdir -p "$(DESTDIR)$(bindir)" ; \
		install -D -m 755 src/hwlatdetect/hwlatdetect.py $(DESTDIR)$(PYLIB)/hwlatdetect.py ; \
		rm -f "$(DESTDIR)$(bindir)/hwlatdetect" ; \
		ln -s $(PYLIB)/hwlatdetect.py "$(DESTDIR)$(bindir)/hwlatdetect" ; \
	fi

.PHONY: install_get_cyclictest_snapshot
install_get_cyclictest_snapshot: get_cyclictest_snapshot
	if test -n "${PYLIB}" ; then \
		mkdir -p "${DESTDIR}${bindir}" ; \
		install -D -m 755 src/cyclictest/get_cyclictest_snapshot.py ${DESTDIR}${PYLIB}/get_cyclictest_snapshot.py ; \
		rm -f "${DESTDIR}${bindir}/get_cyclictest_snapshot" ; \
		ln -s ${PYLIB}/get_cyclictest_snapshot.py "${DESTDIR}${bindir}/get_cyclictest_snapshot" ; \
	fi

.PHONY: install_manpages
install_manpages: $(MANPAGES)
	mkdir -p "$(DESTDIR)$(mandir)/man8"
	cp $(MANPAGES) "$(DESTDIR)$(mandir)/man8"

.PHONY: tarball
tarball:
	git archive --worktree-attributes --prefix=rt-tests-${VERSION}/ -o rt-tests-${VERSION}.tar v${VERSION}

.PHONY: help
help:
	@echo ""
	@echo " rt-tests useful Makefile targets:"
	@echo ""
	@echo "    all       :  build all tests (default"
	@echo "    install   :  install tests to local filesystem"
	@echo "    clean     :  remove object files"
	@echo "    distclean :  remove all generated files"
	@echo "    tarball   :  make a rt-tests tarball suitable for release"
	@echo "    help      :  print this message"

# Universal Ctags warns about the backward compatible option '--extra' and
# wants it to be called '--extras'.
CTAGS_BIN = ctags
CTAGS_EXTRA := $(shell $(CTAGS_BIN) --version 2>&1 | grep -iq universal && echo extras || echo extra)

.PHONY: tags
tags:
	$(CTAGS_BIN) -R --$(CTAGS_EXTRA)=+f --c-kinds=+p --exclude=tmp --exclude=BUILD *
