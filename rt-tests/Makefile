VERSION = 1.0
PREFIX ?= riscv64-linux-musl
CC := $(PREFIX)-gcc -static
AR := $(PREFIX)-ar

OBJDIR = bld

sources = cyclictest.c \
	  hackbench.c \
	  pip_stress.c \
	  pi_stress.c \
	  pmqtest.c \
	  ptsematest.c \
	  rt-migrate-test.c \
	  sendme.c \
	  signaltest.c \
	  sigwaittest.c \
	  svsematest.c

TARGETS = $(sources:.c=)
LIBS	= -lrt -lpthread
RTTESTLIB = -lrttest -L$(OBJDIR)
EXTRA_LIBS ?= -ldl	# for get_cpu
DESTDIR	?=
prefix  ?= /usr/local
bindir  ?= $(prefix)/bin
mandir	?= $(prefix)/share/man
srcdir	?= $(prefix)/src

CFLAGS ?= -Wall -Wno-nonnull 
CPPFLAGS += -D_GNU_SOURCE -Isrc/include
LDFLAGS ?=

CFLAGS	+= -fPIC

PYLIB  ?= $(shell python -c 'import distutils.sysconfig;  print distutils.sysconfig.get_python_lib()')

ifndef DEBUG
	CFLAGS	+= -O2
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

# The default is to assume you have libnuma installed, which is fine to do
# even on non-numa machines. If you don't want to install the numa libs, for
# example, they might not be available in an embedded environment, then
# compile with
# make NUMA=0
ifneq ($(filter x86_64 i386 ia64 mips powerpc,$(machinetype)),)
NUMA 	:= 1
endif

# The default is to assume that you have numa_parse_cpustring_all
# If you have an older version of libnuma that only has numa_parse_cpustring
# then compile with
# make HAVE_PARSE_CPUSTRING_ALL=0
HAVE_PARSE_CPUSTRING_ALL?=1
ifeq ($(NUMA),1)
	CFLAGS += -DNUMA
	NUMA_LIBS = -lnuma
	ifeq ($(HAVE_PARSE_CPUSTRING_ALL),1)
		CFLAGS += -DHAVE_PARSE_CPUSTRING_ALL
	endif
endif

include src/arch/android/Makefile

VPATH	= src/cyclictest:
VPATH	+= src/signaltest:
VPATH	+= src/pi_tests:
VPATH	+= src/rt-migrate-test:
VPATH	+= src/ptsematest:
VPATH	+= src/sigwaittest:
VPATH	+= src/svsematest:
VPATH	+= src/pmqtest:
VPATH	+= src/backfire:
VPATH	+= src/lib:
VPATH	+= src/hackbench:

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) -D VERSION=$(VERSION) -c $< $(CFLAGS) $(CPPFLAGS) -o $@

# Pattern rule to generate dependency files from .c files
$(OBJDIR)/%.d: %.c | $(OBJDIR)
	@$(CC) -MM $(CFLAGS) $(CPPFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@ || rm -f $@

.PHONY: all
all: $(TARGETS) hwlatdetect | $(OBJDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

# Include dependency files, automatically generate them if needed.
-include $(addprefix $(OBJDIR)/,$(sources:.c=.d))

cyclictest: $(OBJDIR)/cyclictest.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(NUMA_LIBS)

signaltest: $(OBJDIR)/signaltest.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

pi_stress: $(OBJDIR)/pi_stress.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

hwlatdetect:  src/hwlatdetect/hwlatdetect.py
	chmod +x src/hwlatdetect/hwlatdetect.py
	ln -s src/hwlatdetect/hwlatdetect.py hwlatdetect

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

sendme: $(OBJDIR)/sendme.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB) $(EXTRA_LIBS)

pip_stress: $(OBJDIR)/pip_stress.o $(OBJDIR)/librttest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) $(RTTESTLIB)

hackbench: $(OBJDIR)/hackbench.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

LIBOBJS =$(addprefix $(OBJDIR)/,error.o rt-get_cpu.o rt-sched.o rt-utils.o)
$(OBJDIR)/librttest.a: $(LIBOBJS)
	$(AR) rcs $@ $^

CLEANUP  = $(TARGETS) *.o .depend *.*~ *.orig *.rej *.d *.a
CLEANUP += $(if $(wildcard .git), ChangeLog)

.PHONY: clean
clean:
	for F in $(CLEANUP); do find -type f -name $$F | xargs rm -f; done
	rm -f rt-tests-*.tar
	rm -f hwlatdetect
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
install: all install_hwlatdetect
	mkdir -p "$(DESTDIR)$(bindir)" "$(DESTDIR)$(mandir)/man4"
	mkdir -p "$(DESTDIR)$(srcdir)" "$(DESTDIR)$(mandir)/man8"
	cp $(TARGETS) "$(DESTDIR)$(bindir)"
	install -D -m 644 src/backfire/backfire.c "$(DESTDIR)$(srcdir)/backfire/backfire.c"
	install -m 644 src/backfire/Makefile "$(DESTDIR)$(srcdir)/backfire/Makefile"
	gzip -c src/backfire/backfire.4 >"$(DESTDIR)$(mandir)/man4/backfire.4.gz"
	gzip -c src/cyclictest/cyclictest.8 >"$(DESTDIR)$(mandir)/man8/cyclictest.8.gz"
	gzip -c src/pi_tests/pi_stress.8 >"$(DESTDIR)$(mandir)/man8/pi_stress.8.gz"
	gzip -c src/ptsematest/ptsematest.8 >"$(DESTDIR)$(mandir)/man8/ptsematest.8.gz"
	gzip -c src/rt-migrate-test/rt-migrate-test.8 >"$(DESTDIR)$(mandir)/man8/rt-migrate-test.8.gz"
	gzip -c src/sigwaittest/sigwaittest.8 >"$(DESTDIR)$(mandir)/man8/sigwaittest.8.gz"
	gzip -c src/svsematest/svsematest.8 >"$(DESTDIR)$(mandir)/man8/svsematest.8.gz"
	gzip -c src/pmqtest/pmqtest.8 >"$(DESTDIR)$(mandir)/man8/pmqtest.8.gz"
	gzip -c src/backfire/sendme.8 >"$(DESTDIR)$(mandir)/man8/sendme.8.gz"
	gzip -c src/hackbench/hackbench.8 >"$(DESTDIR)$(mandir)/man8/hackbench.8.gz"
	gzip -c src/signaltest/signaltest.8 >"$(DESTDIR)$(mandir)/man8/signaltest.8.gz"

.PHONY: install_hwlatdetect
install_hwlatdetect: hwlatdetect
	if test -n "$(PYLIB)" ; then \
		mkdir -p "$(DESTDIR)$(bindir)" "$(DESTDIR)$(mandir)/man8" ; \
		install -D -m 755 src/hwlatdetect/hwlatdetect.py $(DESTDIR)$(PYLIB)/hwlatdetect.py ; \
		rm -f "$(DESTDIR)$(bindir)/hwlatdetect" ; \
		ln -s $(PYLIB)/hwlatdetect.py "$(DESTDIR)$(bindir)/hwlatdetect" ; \
		gzip -c src/hwlatdetect/hwlatdetect.8 >"$(DESTDIR)$(mandir)/man8/hwlatdetect.8.gz" ; \
	fi

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

.PHONY: tags
tags:
	ctags -R --extra=+f --c-kinds=+p --exclude=tmp --exclude=BUILD *
