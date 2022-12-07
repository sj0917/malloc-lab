#include "memlib.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"

static char *mem_start_brk; /* points to first byte of heap */
static char *mem_brk;       /* points to last byte of heap */
static char *mem_max_addr;  /* largest legal heap address */

void mem_init(void) {
  if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
    fprintf(stderr, "mem_init_vm: malloc error\n");
    exit(1);
  }

  mem_max_addr = mem_start_brk + MAX_HEAP; /* max legal heap address */
  mem_brk = mem_start_brk;                 /* heap is empty initially */
}

void mem_deinit(void) { free(mem_start_brk); }

void mem_reset_brk() { mem_brk = mem_start_brk; }

void *mem_sbrk(int incr) {
  char *old_brk = mem_brk;

  if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
    errno = ENOMEM;
    fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
    return (void *)-1;
  }
  mem_brk += incr;
  return (void *)old_brk;
}

void *mem_mmap(size_t size)
{
    if (size <= 0) { return NULL; }
    void *ptr = mmap(mem_brk, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) { return NULL; }

    return ptr;
}

void *mem_heap_lo() { return (void *)mem_start_brk; }

void *mem_heap_hi() { return (void *)(mem_brk - 1); }

size_t mem_heapsize() { return (size_t)(mem_brk - mem_start_brk); }

size_t mem_pagesize() { return (size_t)getpagesize(); }