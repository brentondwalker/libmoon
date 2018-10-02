#ifndef MG_BYTESIZEDRING_H
#define MG_BYTESIZEDRING_H

#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_rwlock.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BS_RING_SIZE_LIMIT 268435455

struct bs_ring
{
	struct rte_ring* ring;
	uint32_t capacity;
	uint32_t used;
	rte_rwlock_t used_lock;
};

struct bs_ring* create_bsring(uint32_t capacity, int32_t socket);

/**
 * The difference between bulk and burst is when n>1.  In those
 * cases bulk mode will only en/dequeue a full batch.  In burst
 * mode it will enqueue whatever there is space for, or dequeue
 * as many as are available, up to n.
 */
int bsring_enqueue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bsring_enqueue_burst(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bsring_enqueue(struct bs_ring* bsr, struct rte_mbuf* obj);
int bsring_dequeue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bsring_dequeue_burst(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bsring_dequeue(struct bs_ring* bsr, struct rte_mbuf** obj);
int bsring_count(struct bs_ring* bsr);
int bsring_capacity(struct bs_ring* bsr);
int bsring_bytesused(struct bs_ring* bsr);


#ifdef __cplusplus
}
#endif

#endif


