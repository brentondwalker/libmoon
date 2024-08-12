#ifndef MG_PKTSIZEDRING_H
#define MG_PKTSIZEDRING_H

#include <cstdint>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_RING_SIZE_LIMIT 268435455
#define PS_RING_MEMPOOL_BUF_SIZE RTE_MBUF_DEFAULT_BUF_SIZE /* 2048 */
#define PS_RING_MEMPOOL_CACHE_SIZE 512
  
/**
 * If we put mbufs directly into a large rte_ring, the mempool of the producer
 * they are coming from may fill up.  This seems to be the case with the receive
 * device.  Once it runs out of space for mbufs, calls to recv() block.
 *
 * In cases with large rte_ring, it makes sense to allocate our own mempool, 
 * and enqueue copies of the mbufs, so we can free the ones owned by the producer.
 */
  
struct ps_ring
{
	struct rte_ring* ring;
	uint32_t capacity;
	struct rte_mempool *pktmbuf_pool;  // null unless copy_mbufs is true
	bool copy_mbufs;
};

struct ps_ring* create_psring(uint32_t capacity, int32_t socket, bool copy_mbufs);

/**
 * The difference between bulk and burst is when n>1.  In those
 * cases bulk mode will only en/dequeue a full batch.  In burst
 * mode it will enqueue whatever there is space for, or dequeue
 * as many as are available, up to n.
 */
int psring_enqueue_bulk(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n);
int psring_enqueue_burst(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n);
int psring_enqueue(struct ps_ring* psr, struct rte_mbuf* obj);
int psring_dequeue_bulk(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n);
int psring_dequeue_burst(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n);
int psring_dequeue(struct ps_ring* psr, struct rte_mbuf** obj);
int psring_count(struct ps_ring* psr);
int psring_capacity(struct ps_ring* psr);


#ifdef __cplusplus
}
#endif

#endif


