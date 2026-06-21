#include <cstdint>
#include <minidpdk/rte_mempool.h>

#include <minidpdk/internal/mem_pool.hh>
#include <malloc.h>

void rte_mempool_free(rte_mempool *pool) {
    pool->~mem_pool();
    free(pool);
}

uint16_t rte_pktmbuf_data_room_size(rte_mempool *mp){
    return mp->get_data_size();
}
