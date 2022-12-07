#include "memlib.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"

static char *mem_start_brk = NULL;
static char *mem_brk = NULL;
static char *mem_max_addr = NULL;

void mem_init(void) {
  if ((mem_start_brk = (char *)mem_mmap(MAX_HEAP)) == NULL) {
    fprintf(stderr, "mem_init_vm: malloc error\n");
    exit(1);
  }

  mem_max_addr = mem_start_brk + MAX_HEAP; 
  mem_brk = mem_start_brk;                 
}

void mem_deinit(void) { free(mem_start_brk); }

void mem_reset_brk() { mem_brk = mem_start_brk; }

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