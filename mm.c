#include "mm.h"
#include "memlib.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <assert.h>


team_t team = { 
                "ateam",
                "Harry Bovik",
                "bovik@cs.cmu.edu",
                "",
                ""
              };


#define ALIGN8(n) (((n) + 8 - 1) & ~(8 - 1))

struct node
{
    struct node *next; // next node in memory order.
    struct node *prev; // previous node in memory order.
    size_t size;       // Does not include size of node.
    bool is_free;
};

struct body
{
    struct node *next_free; // Set if free, not in memory order.
};

static struct body *node_get_body(struct node *node)
{
    return (struct body *)((char *)node + sizeof(struct node));
}

static struct node *node_get_next_free(struct node *node)
{
    return node_get_body(node)->next_free;
}

static void node_set_next_free(struct node *node, struct node *next_free)
{
    node_get_body(node)->next_free = next_free;
}

static void node_insert_after(struct node *left, struct node *right)
{
    right->next = left->next;
    right->prev = left;
    left->prev = right;
}

static struct node *node_from_ptr(void *ptr)
{
    return (struct node *)((char *)ptr - sizeof(struct node));
}

static void *node_get_ptr(struct node *node)
{
    return (void *)((char *)node + sizeof(struct node));
}

static struct pool
{
    void *start;
    size_t size;
    struct node *first;
    struct node *first_free;
} g_pool = {NULL, 0};

static void my_log(const char *msg, size_t value)
{
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s(%zu)\n", msg, value);
    write(STDERR_FILENO, buffer, strlen(buffer));
}

int mm_init(void) 
{
    const size_t pool_size = ((size_t)20) * (1 << 20); // 20 MB
    g_pool.start = mem_mmap(pool_size);
    g_pool.size = pool_size;
    g_pool.first = (struct node *)g_pool.start;
    g_pool.first->next = NULL;
    g_pool.first->prev = NULL;
    g_pool.first->size = g_pool.size - sizeof(struct node);
    g_pool.first->is_free = true;
    node_set_next_free(g_pool.first, NULL);
    g_pool.first_free = g_pool.first;

    write(STDERR_FILENO, "heap initialized\n", 17);
    return 0;
}

void *mm_malloc(size_t size)
{
    my_log("mm_malloc", size);
    if (g_pool.start == NULL)
    {
        mm_init();
    }
    if (size <= 0)
    {
        return NULL;
    }
    struct node *prev_free = NULL;
    struct node *current = g_pool.first_free;
    while (current != NULL)
    {
        assert(current->is_free);
        if (current->size >= size)
        {
            static_assert(sizeof(struct node) == ALIGN8(sizeof(struct node)), "node must a multiple of 8-bytes");

            const size_t aligned_size = ALIGN8(size);
            const size_t kMinimumFreeNodeSize = sizeof(struct node) + sizeof(struct body);

            if (current->size > aligned_size + kMinimumFreeNodeSize)
            {
                struct node *new_node = (struct node *)((char *)current + aligned_size);
                node_insert_after(current, new_node);
                new_node->size = current->size - aligned_size - sizeof(struct node);
                current->size = aligned_size;
                new_node->is_free = true;

                node_set_next_free(new_node, node_get_next_free(current));
                if (prev_free != NULL) { node_set_next_free(prev_free, new_node); }
                else { g_pool.first_free = new_node; }
            }
            else
            {
                struct node *next_free = node_get_next_free(current);
                if (prev_free != NULL) { node_set_next_free(prev_free, next_free); }
                else { g_pool.first_free = next_free; }
            }

            current->is_free = false;
            return node_get_ptr(current);
        }
        current = current->next;
    }
    return NULL;
}

void mm_free(void *ptr)
{
    if (ptr == NULL) { return; }

    struct node *node = node_from_ptr(ptr);
    my_log("free", node->size);

    node->is_free = true;
    if (node->prev && node->prev->is_free)
    {
        struct node *prev = node->prev;
        prev->size += node->size + sizeof(struct node);
        prev->next = node->next;

        if (node->next) { node->next->prev = prev; }
        node = prev;
    }
    else
    {
        node_set_next_free(node, g_pool.first_free);
        g_pool.first_free = node;
    }
}

void *mm_realloc(void *ptr, size_t size)
{
    my_log("realloc", size);

    void *new_ptr = mm_malloc(size);
    memcpy(new_ptr, ptr, size);
    mm_free(ptr);
    return new_ptr;
}

void *mm_calloc(size_t number_of_members, size_t size)
{
    const size_t number_of_bytes = number_of_members * size;
    my_log("calloc", number_of_bytes);

    void *ptr = mm_malloc(number_of_bytes);
    memset(ptr, 0, number_of_bytes);
    return ptr;
}

void dump_heap(void)
{
    struct node *current = g_pool.first;
    while (current != NULL)
    {
        printf("Node: %p, size: %zu, is_free: %d\n", current, current->size, current->is_free);
        current = current->next;
    }
}

