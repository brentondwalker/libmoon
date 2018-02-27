#ifndef MG_BYTESIZEDRING_H
#define MG_BYTESIZEDRING_H

#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RING_SIZE_LIMIT 268435455

struct bs_ring
{
	struct rte_ring* ring;
	int capacity;
	int used;
};

struct bs_ring* create_bsring(uint32_t capacity, int32_t socket);
int bsring_enqueue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, int n);
int bsring_enqueue(struct bs_ring* bsr, struct rte_mbuf* obj);
int bsring_dequeue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, int n);
int bsring_count(struct bs_ring* r);
int bsring_capacity(struct bs_ring* bsr);
int bsring_bytesused(struct bs_ring* bsr);


#ifdef __cplusplus
}
#endif

#endif


