#ifndef MG_BYTESIZEDTXRING_H
#define MG_BYTESIZEDTXRING_H

#include <cstdint>
#include <atomic>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
//#include <rte_rwlock.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSTX_RING_SIZE_LIMIT 268435455

struct bstx_ring
{
	struct rte_ring* ring;
	uint32_t capacity;

	/*
	 * Keep track of the number of bytes in the ring buffer.
	 * This value is an atomic because it will be modified by
	 * multiple threads.  It is used in a simple way to
	 * maintain better performance.  Therefore it is possible to
	 * read this value in a transient state where its value
	 * is briefly out of sync with the contents of the buffer.
	 * If there are multiple threads feeding the buffer, this
	 * could result in filling the buffer above its intended
	 * capacity.
	 *
	 * TODO:
	 * We could provide better protection by locking over the
	 * entire enqueue/dequeue functions, but expect performance
	 * to suffer.
	 */
	std::atomic<uint32_t> bytes_used;
};

struct bstx_ring* create_bstxring(uint32_t capacity, int32_t socket, uint16_t port);
static uint16_t bstxring_decrement_callback(uint8_t port __rte_unused, uint16_t qidx __rte_unused, struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused);
/**
 * The difference between bulk and burst is when n>1.  In those
 * cases bulk mode will only en/dequeue a full batch.  In burst
 * mode it will enqueue whatever there is space for, or dequeue
 * as many as are available, up to n.
 */
int bstxring_enqueue_bulk(struct bstx_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bstxring_enqueue_burst(struct bstx_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bstxring_enqueue(struct bstx_ring* bsr, struct rte_mbuf* obj);
int bstxring_dequeue_bulk(struct bstx_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bstxring_dequeue_burst(struct bstx_ring* bsr, struct rte_mbuf** obj, uint32_t n);
int bstxring_dequeue(struct bstx_ring* bsr, struct rte_mbuf** obj);
int bstxring_count(struct bstx_ring* bsr);
int bstxring_capacity(struct bstx_ring* bsr);
int bstxring_bytesused(struct bstx_ring* bsr);


#ifdef __cplusplus
}
#endif

#endif


