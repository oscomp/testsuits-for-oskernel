/* Modified from glibc: io/tst-copy_file_range.c for oscomp.

   Tests for copy_file_range.
   Copyright (C) 2017-2023 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* array_length (VAR) is the number of elements in the array VAR.  VAR
   must evaluate to an array, not a pointer.  */
#define array_length(var)                                                   \
  (sizeof(var) / sizeof((var)[0]) +                                         \
   0 * sizeof(struct {                                                      \
     _Static_assert(                                                        \
         !__builtin_types_compatible_p(__typeof(var), __typeof(&(var)[0])), \
         "argument must be an array");                                      \
   }))

/* array_end (VAR) is a pointer one past the end of the array VAR.
   VAR must evaluate to an array, not a pointer.  */
#define array_end(var) (&(var)[array_length(var)])

void fail_exit(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

#define die(fmt_, ...) \
  fail_exit("%s:%d: " fmt_ "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define TEST_COMPARE(a_, b_) \
  {                          \
    if (a_ != b_) {          \
      die("compare failed"); \
    }                        \
  }

#define TEST_VERIFY(a_)     \
  {                         \
    if (!a_) {              \
      die("verify failed"); \
    }                       \
  }

void xwrite(int fd, const void* buffer, size_t length) {
  const char* p = buffer;
  const char* end = p + length;
  while (p < end) {
    ssize_t ret = write(fd, p, end - p);
    if (ret < 0)
      die("write of %zu bytes failed after %td", length,
          p - (const char*)buffer);
    if (ret == 0)
      die("write return 0 after writing %td bytes of %zu",
          p - (const char*)buffer, length);
    p += ret;
  }
}

long long xlseek(int fd, long long offset, int whence) {
  long long result = lseek(fd, offset, whence);
  if (result < 0)
    die("lseek(%d, %lld, %d)", fd, offset, whence);
  return result;
}

void* xmalloc(size_t n) {
  void* p;
  p = malloc(n);
  if (p == NULL)
    die("malloc %zu", n);
  return p;
}

int xopen(const char* path, int flags, mode_t mode) {
  int ret = open(path, flags, mode);
  if (ret < 0)
    die("open (\"%s\", 0x%x, 0%o)", path, flags, mode);
  return ret;
}

void xfstat(int fd, struct stat* result) {
  if (fstat(fd, result) != 0)
    die("fstat (%d)", fd);
}

void xftruncate(int fd, long long length) {
  if (ftruncate(fd, length) != 0)
    die("ftruncate (%d, %lld)", fd, length);
}

#define xclose close
#define test_verbose 0

#ifdef O_LARGEFILE
#undef O_LARGEFILE
#endif
#define O_LARGEFILE 0

int create_temp_file(const char* prefix, char** name) {
  *name = xmalloc(strlen(prefix) + 6);
  strcpy(*name, prefix);
  strcat(*name, "XXXXXX");
  int fd = mkstemp(*name);
  if (fd < 0)
    die("mkstemp: %d", fd);
  printf("created: %s\n", *name);
  return fd;
}

/* Boolean flags which indicate whether to use pointers with explicit
   output flags.  */
static int do_inoff;
static int do_outoff;

/* Name and descriptors of the input files.  Files are truncated and
   reopened (with O_RDWR) between tests.  */
static char* infile;
static int infd;
static char* outfile;
static int outfd;

/* Input and output offsets.  Set according to do_inoff and do_outoff
   before the test.  The offsets themselves are always set to
   zero.  */
static off_t inoff;
static off_t* pinoff;
static off_t outoff;
static off_t* poutoff;

/* These are a collection of copy sizes used in tests.    */
enum { maximum_size = 99999 };
static const int typical_sizes[] = {0,    1,    2,    3,    1024,        2048,
                                    4096, 8191, 8192, 8193, maximum_size};

/* The random contents of this array can be used as a pattern to check
   for correct write operations.  */
static unsigned char random_data[maximum_size];

/* The size chosen by the test harness.  */
static int current_size;

/* Perform a copy of a file.  */
static void simple_file_copy(void) {
  xwrite(infd, random_data, current_size);

  int length;
  int in_skipped; /* Expected skipped bytes in input.  */
  if (do_inoff) {
    xlseek(infd, 1, SEEK_SET);
    inoff = 2;
    length = current_size - 3;
    in_skipped = 2;
  } else {
    xlseek(infd, 3, SEEK_SET);
    length = current_size - 5;
    in_skipped = 3;
  }
  int out_skipped; /* Expected skipped bytes before the written data.  */
  if (do_outoff) {
    xlseek(outfd, 4, SEEK_SET);
    outoff = 5;
    out_skipped = 5;
  } else {
    xlseek(outfd, 6, SEEK_SET);
    length = current_size - 6;
    out_skipped = 6;
  }
  if (length < 0)
    length = 0;

  TEST_COMPARE(copy_file_range(infd, pinoff, outfd, poutoff, length, 0),
               length);
  if (do_inoff) {
    TEST_COMPARE(inoff, 2 + length);
    TEST_COMPARE(xlseek(infd, 0, SEEK_CUR), 1);
  } else
    TEST_COMPARE(xlseek(infd, 0, SEEK_CUR), 3 + length);
  if (do_outoff) {
    TEST_COMPARE(outoff, 5 + length);
    TEST_COMPARE(xlseek(outfd, 0, SEEK_CUR), 4);
  } else
    TEST_COMPARE(xlseek(outfd, 0, SEEK_CUR), 6 + length);

  struct stat st;
  xfstat(outfd, &st);
  if (length > 0) {
    TEST_COMPARE(st.st_size, out_skipped + length);
  } else {
    /* If we did not write anything, we also did not add any
       padding.  */
    TEST_COMPARE(st.st_size, 0);
    return;
  }

  xlseek(outfd, 0, SEEK_SET);
  char* bytes = xmalloc(st.st_size);
  TEST_COMPARE(read(outfd, bytes, st.st_size), st.st_size);
  for (int i = 0; i < out_skipped; ++i)
    TEST_COMPARE(bytes[i], 0);
  TEST_VERIFY(memcmp(bytes + out_skipped, random_data + in_skipped, length) ==
              0);
  free(bytes);
}

/* Test that a short input file results in a shortened copy.  */
static void short_copy(void) {
  if (current_size == 0)
    /* Nothing to shorten.  */
    return;

  /* Two subtests, one with offset 0 and current_size - 1 bytes, and
     another one with current_size bytes, but offset 1.  */
  for (int shift = 0; shift < 2; ++shift) {
    if (test_verbose > 0)
      printf("info:   shift=%d\n", shift);
    xftruncate(infd, 0);
    xlseek(infd, 0, SEEK_SET);
    xwrite(infd, random_data, current_size - !shift);

    if (do_inoff) {
      inoff = shift;
      xlseek(infd, 2, SEEK_SET);
    } else {
      inoff = 3;
      xlseek(infd, shift, SEEK_SET);
    }
    xftruncate(outfd, 0);
    xlseek(outfd, 0, SEEK_SET);
    outoff = 0;

    /* First call copies current_size - 1 bytes.  */
    TEST_COMPARE(copy_file_range(infd, pinoff, outfd, poutoff, current_size, 0),
                 current_size - 1);
    char* buffer = xmalloc(current_size);
    TEST_COMPARE(pread(outfd, buffer, current_size, 0), current_size - 1);
    TEST_VERIFY(memcmp(buffer, random_data + shift, current_size - 1) == 0);
    free(buffer);

    if (do_inoff) {
      TEST_COMPARE(inoff, current_size - 1 + shift);
      TEST_COMPARE(xlseek(infd, 0, SEEK_CUR), 2);
    } else
      TEST_COMPARE(xlseek(infd, 0, SEEK_CUR), current_size - 1 + shift);
    if (do_outoff) {
      TEST_COMPARE(outoff, current_size - 1);
      TEST_COMPARE(xlseek(outfd, 0, SEEK_CUR), 0);
    } else
      TEST_COMPARE(xlseek(outfd, 0, SEEK_CUR), current_size - 1);

    /* First call copies zero bytes.  */
    TEST_COMPARE(copy_file_range(infd, pinoff, outfd, poutoff, current_size, 0),
                 0);
    /* And the offsets are unchanged.  */
    if (do_inoff) {
      TEST_COMPARE(inoff, current_size - 1 + shift);
      TEST_COMPARE(xlseek(infd, 0, SEEK_CUR), 2);
    } else
      TEST_COMPARE(xlseek(infd, 0, SEEK_CUR), current_size - 1 + shift);
    if (do_outoff) {
      TEST_COMPARE(outoff, current_size - 1);
      TEST_COMPARE(xlseek(outfd, 0, SEEK_CUR), 0);
    } else
      TEST_COMPARE(xlseek(outfd, 0, SEEK_CUR), current_size - 1);
  }
}

/* A named test function.  */
struct test_case {
  const char* name;
  void (*func)(void);
  bool sizes; /* If true, call the test with different current_size values.  */
};

/* The available test cases.  */
static struct test_case tests[] = {
    {"simple_file_copy", simple_file_copy, .sizes = true},
    {"short_copy", short_copy, .sizes = true},
};

void do_test(int n_inoff, int n_outoff, int n_test) {
  for (unsigned char* p = random_data; p < array_end(random_data); ++p)
    *p = rand() >> 24;

  infd = create_temp_file("tst-copy_file_range-in-", &infile);
  outfd = create_temp_file("tst-copy_file_range-out-", &outfile);
  {
    ssize_t ret = copy_file_range(infd, NULL, outfd, NULL, 0, 0);
    if (ret != 0) {
      die("copy_file_range probing call: %d", ret);
    }
  }
  xclose(infd);
  xclose(outfd);

  for (do_inoff = 0; do_inoff < n_inoff; ++do_inoff)
    for (do_outoff = 0; do_outoff < n_outoff; ++do_outoff)
      for (struct test_case* test = tests; test < tests + n_test; ++test)
        for (const int* size = typical_sizes; size < array_end(typical_sizes);
             ++size) {
          current_size = *size;
          if (test_verbose > 0)
            printf("info: %s do_inoff=%d do_outoff=%d current_size=%d\n",
                   test->name, do_inoff, do_outoff, current_size);

          inoff = 0;
          if (do_inoff)
            pinoff = &inoff;
          else
            pinoff = NULL;
          outoff = 0;
          if (do_outoff)
            poutoff = &outoff;
          else
            poutoff = NULL;

          infd = xopen(infile, O_RDWR | O_LARGEFILE, 0);
          xftruncate(infd, 0);
          outfd = xopen(outfile, O_RDWR | O_LARGEFILE, 0);
          xftruncate(outfd, 0);

          test->func();

          xclose(infd);
          xclose(outfd);

          if (!test->sizes)
            /* Skip the other sizes unless they have been
               requested.  */
            break;
        }

  remove(infile);
  remove(outfile);
  free(infile);
  free(outfile);
}

int main(void) {
#if CASE == 1
  do_test(1, 1, 1);
#elif CASE == 2
  do_test(1, 1, 2);
#elif CASE == 3
  do_test(2, 2, 1);
#else
  do_test(2, 2, 2);
#endif
  printf("copy-file-range-test: passed case %d\n", CASE);
  return 0;
}
