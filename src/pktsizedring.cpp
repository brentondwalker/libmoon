#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_rwlock.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include "pktsizedring.hpp"

// DPDK SPSC bounded ring buffer
/*
 * This wraps the DPDK SPSC bounded ring buffer into a structure whose capacity
 * limits the number of packets/frames it can hold.
 * In the plain implementation the ring size must be a power of 2.
 */

struct ps_ring* create_psring(uint32_t capacity, int32_t socket) {
	static volatile uint32_t ring_cnt = 0;
	if (capacity > PS_RING_SIZE_LIMIT) {
		printf("WARNING: requested capacity of %d is too large.  Allocating ring of size %d.\n",capacity,PS_RING_SIZE_LIMIT);
		capacity = PS_RING_SIZE_LIMIT;
	}
	uint32_t count = 1;

	// DPDK ring buffers come with sizes of 2^n, but actual storage limit is (2^n - 1).
	// Therefore if someone reqests a pktsized ring of size 8, for example, we need to
	// allocate one of size 16.
	while (count < (capacity+1)) {
		count *= 2;
	}
	char ring_name[32];
	struct ps_ring* psr = (struct ps_ring*)malloc(sizeof(struct ps_ring));
	psr->capacity = capacity;
	sprintf(ring_name, "mbuf_ps_ring%d", __sync_fetch_and_add(&ring_cnt, 1));
	psr->ring = rte_ring_create(ring_name, count, socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (! psr->ring) {
		free(psr);
		return NULL;
	}
	return psr;
}

int psring_enqueue_bulk(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n) {
	if ((rte_ring_count(psr->ring) + n) < psr->capacity) {
		return rte_ring_sp_enqueue_bulk(psr->ring, (void**)obj, n, NULL);
	}
	return 0;
}

int psring_enqueue_burst(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n) {
	uint32_t count = rte_ring_count(psr->ring);
	if (count > psr->capacity) {
		// pktsized ring is over capacity
		return 0;
	}
	uint32_t num_to_add = ((count + n) > psr->capacity) ? (psr->capacity - count) : n;
	int num_added = rte_ring_sp_enqueue_burst(psr->ring, (void**)obj, num_to_add, NULL);

	// free the remaining mbufs that didn't make it in.
	for (uint32_t i=num_added; i<n; i++) {
		rte_pktmbuf_free(obj[i]);
		obj[i] = NULL;
	}
	
	return num_added;
}

int psring_enqueue(struct ps_ring* psr, struct rte_mbuf* obj) {
	if ((rte_ring_count(psr->ring) + 1) <= psr->capacity) {
		if (rte_ring_sp_enqueue(psr->ring, obj) == 0) {
			return 1;
		}
	}
	rte_pktmbuf_free(obj);
	return 0;
}

int psring_dequeue_bulk(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n) {
	return rte_ring_sc_dequeue_bulk(psr->ring, (void**)obj, n, NULL);
}

int psring_dequeue_burst(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n) {
	return rte_ring_sc_dequeue_burst(psr->ring, (void**)obj, n, NULL);
}

int psring_dequeue(struct ps_ring* psr, struct rte_mbuf** obj) {
	return (rte_ring_sc_dequeue(psr->ring, (void**)obj) == 0);
}

int psring_count(struct ps_ring* psr) {
	return rte_ring_count(psr->ring);
}

int psring_capacity(struct ps_ring* psr) {
	return psr->capacity;
}

