#ifndef MG_BYTESIZEDRING_H
#define MG_BYTESIZEDRING_H

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

#define BS_RING_SIZE_LIMIT 268435455
#define FRAME_OVERHEAD 24
#define BS_RING_MEMPOOL_BUF_SIZE RTE_MBUF_DEFAULT_BUF_SIZE /* 2048 */
#define BS_RING_MEMPOOL_CACHE_SIZE 512
#define BS_RING_MEMPOOL_MIN_SIZE 1024


struct bs_ring
{
	struct rte_ring* ring;
	uint32_t capacity;
	struct rte_mempool *pktmbuf_pool;  // null unless copy_mbufs is true
        bool copy_mbufs;

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
	std::atomic<bool> ring_locked;
};

  struct bs_ring* create_bsring(uint32_t capacity, int32_t socket, bool copy_mbufs);

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


