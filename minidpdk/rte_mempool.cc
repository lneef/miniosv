#include <minidpdk/rte_mempool.h>

#include <minidpdk/internal/mem_pool.hh>
#include <malloc.h>

void rte_mempool_free(rte_mempool *pool) {
    pool->~mem_pool();
    free(pool);
}
