#include <unistd.h>

void mem_init(void);               
void mem_deinit(void);
void *mem_sbrk(int incr);
void *mem_mmap(size_t size);
void mem_reset_brk(void); 
void *mem_heap_lo(void);
void *mem_heap_hi(void);
void set_mem_heap_lo(char *);
void set_mem_heap_cur(char *);
void set_mem_heap_hi(char *);
size_t mem_heapsize(void);
size_t mem_pagesize(void);

