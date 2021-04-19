// See LICENSE for license details.

#include "htif.h"
#include "atomic.h"
#include "mtrap.h"
#include "fdt.h"
#include "syscall.h"
#include <string.h>

struct { uint32_t arr[2]; } volatile tohost __attribute__((section(".htif")));
struct { uint32_t arr[2]; } volatile fromhost __attribute__((section(".htif")));
static spinlock_t htif_lock = SPINLOCK_INIT;
uintptr_t htif;

static inline void htif_send(uint8_t dev, uint8_t cmd, int64_t data)
{
  /* endian neutral encoding with ordered 32-bit writes */
  union { uint32_t arr[2]; uint64_t val; } encode = {
    .val = (uint64_t)dev << 56 | (uint64_t)cmd << 48 | data
  };
  tohost.arr[0] = encode.arr[0];
  tohost.arr[1] = encode.arr[1];
}

static inline void htif_recv(uint8_t *dev, uint8_t *cmd, int64_t *data)
{
  /* endian neutral decoding with ordered 32-bit reads */
  union { uint32_t arr[2]; uint64_t val; } decode;
  decode.arr[0] = fromhost.arr[0];
  decode.arr[1] = fromhost.arr[1];
  *dev = decode.val >> 56;
  *cmd = (decode.val >> 48) & 0xff;
  *data = decode.val << 16 >> 16;
}

static int64_t htif_get_fromhost(uint8_t dev, uint8_t cmd)
{
  /* receive data for specified device and command */
  uint8_t rdev, rcmd;
  int64_t data;
  htif_recv(&rdev, &rcmd, &data);
  return rdev == dev && rcmd == cmd ? data : -1;
}

static void htif_set_tohost(uint8_t dev, uint8_t cmd, int64_t data)
{
  /* send data with specified device and command */
  while (tohost.arr[0]) {
    asm volatile ("" : : "r" (fromhost.arr[0]));
    asm volatile ("" : : "r" (fromhost.arr[1]));
  }
  htif_send(dev, cmd, data);
}

int htif_console_getchar()
{
  int ch;
  spinlock_lock(&htif_lock);
  if ((ch = htif_get_fromhost(1, 0) & 0xff)) {
    htif_set_tohost(1, 0, 0);
  }
  spinlock_unlock(&htif_lock);
  return ch;
}

void htif_console_putchar(uint8_t ch)
{
  spinlock_lock(&htif_lock);
  htif_set_tohost(1, 1, ch);
  spinlock_unlock(&htif_lock);
}

void htif_syscall(uintptr_t arg)
{
  htif_set_tohost(0, 0, arg);
}

void htif_poweroff(int status)
{
  for (;;) {
    htif_set_tohost(0, 0, 1);
  }
}

struct htif_scan
{
  int compat;
};

static void htif_open(const struct fdt_scan_node *node, void *extra)
{
  struct htif_scan *scan = (struct htif_scan *)extra;
  memset(scan, 0, sizeof(*scan));
}

static void htif_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct htif_scan *scan = (struct htif_scan *)extra;
  if (!strcmp(prop->name, "compatible") && !strcmp((const char*)prop->value, "ucb,htif0")) {
    scan->compat = 1;
  }
}

static void htif_done(const struct fdt_scan_node *node, void *extra)
{
  struct htif_scan *scan = (struct htif_scan *)extra;
  if (!scan->compat) return;

  htif = 1;
}

void query_htif(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct htif_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = htif_open;
  cb.prop = htif_prop;
  cb.done = htif_done;
  cb.extra = &scan;

  fdt_scan(fdt, &cb);
}
