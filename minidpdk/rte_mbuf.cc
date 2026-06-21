#include <minidpdk/rte_mbuf.h>

#include <minidpdk/internal/mem_pool.hh>
#include <cassert>
#include <cerrno>
#include <malloc.h>
#include <new>

static inline void free_internal(rte_mbuf *buf) {
    if (buf->ol_flags & RTE_MBUF_F_EXTERNAL || buf->shinfo) {
        assert(buf->shinfo->refcnt > 0);
        --buf->shinfo->refcnt;
        if (!buf->shinfo->refcnt)
            buf->shinfo->free_cb(buf->buf_addr, buf->shinfo->fcb_opaque);
    }
    minidpdk::mbuf_free(buf);
}

void rte_pktmbuf_free(rte_mbuf *mbuf) {
    free_internal(mbuf);
}

void rte_mbuf_raw_free(rte_mbuf *mbuf) {
    free_internal(mbuf);
}

int rte_pktmbuf_alloc_bulk(rte_pktmbuf_pool *pool, rte_mbuf **pkts, uint16_t size) {
    unsigned ret = pool->alloc_bulk(reinterpret_cast<void **>(pkts), size);
    if (!ret)
        return -ENOENT;
    return 0;
}

void rte_pktmbuf_free_bulk(rte_mbuf **pkts, uint16_t size) {
    for (auto i = 0u; i < size; ++i)
        free_internal(pkts[i]);
}

const void *rte_pktmbuf_read(rte_mbuf *m, uint32_t off, uint32_t len, uint8_t *buf) {
    return m->read(off, len, buf);
}

rte_pktmbuf_pool *rte_pktmbuf_pool_create(const char *name, unsigned n,
                                          unsigned cache_size, uint16_t priv_size,
                                          uint16_t data_room_size, int socket_id) {
    (void)name;
    (void)cache_size;
    (void)priv_size;
    (void)socket_id;
    assert(data_room_size);
    auto *slab = malloc(sizeof(minidpdk::mem_pool));
    return new (slab) minidpdk::mem_pool(n, data_room_size);
}
