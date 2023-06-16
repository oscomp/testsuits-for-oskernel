
# Android use Bionic for libc, and this does not have
# - pthread barriers
# - pthread_[gs]etaffinity
#
# This is all handled by bionic.h based on flags we set here.

# Typically see something like "aarch64-linux-android". However, in some
# buildsystems, it will be a variation of -androidabe.
ost=$(findstring android, $(ostype))
ifeq (android,$(ost))
	USE_BIONIC := 1
	CFLAGS += -DPTHREAD_BIONIC

	LDFLAGS += -pie
# -lrt and -lpthread is in standard bionic library, no standalone library
	LIBS := $(filter-out -lrt,$(LIBS))
	LIBS := $(filter-out -lpthread,$(LIBS))

# Currently, only these binaries will compile and link properly for android
# - cyclictest
# - hackbench
	sources := cyclictest.c hackbench.c
endif
