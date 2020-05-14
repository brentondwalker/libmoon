#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include "bytesizedring.hpp"

// DPDK SPSC bounded ring buffer
/*
 * This wraps the DPDK SPSC bounded ring buffer into a structure whose capacity
 * limits the number of bytes it can hold.
 */

struct bs_ring* create_bsring(uint32_t capacity, int32_t socket) {
	static volatile uint32_t ring_cnt = 0;
	int count_min = 1 + capacity/60;
	int count = 1;
	
	// DPDK ring buffers come with sizes of 2^n, but the actual storage limit
	// is (2^n - 1).  Therefore always request a ring that will hold our
	// desired size plus 1.
	while ((count-1) < count_min && count <= BS_RING_SIZE_LIMIT) {
		count *= 2;
	}
	if (count > BS_RING_SIZE_LIMIT) {
		printf("WARNING: create_bsring(): could not allocate a large enough ring.\n");
		count /= 2;
	}
	char ring_name[32];
	struct bs_ring* bsr = (struct bs_ring*)malloc(sizeof(struct bs_ring));
	bsr->capacity = capacity;
	bsr->ring_locked = false;
	sprintf(ring_name, "mbuf_bs_ring%d", __sync_fetch_and_add(&ring_cnt, 1));
	bsr->ring = rte_ring_create(ring_name, count, socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
	bsr->bytes_used = 0;

	if (! bsr->ring) {
		free(bsr);
		return NULL;
	}
	return bsr;
}

/**
 * XXX review this function.
 */
int bsring_enqueue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n) {
	uint32_t num_added = 0;

	// in bulk mode we either add all or nothing.
	// check if there is space available.
	uint32_t i = 0;
	uint32_t total_size = 0;
	for (i=0; i<n; i++) {
		total_size += obj[i]->pkt_len + FRAME_OVERHEAD;
	}
	if ((bsr->bytes_used + total_size) > bsr->capacity) {
		// the mbufs will be dropped.  Free them.
		for (uint32_t i=0; i<n; i++) {
			rte_pktmbuf_free(obj[i]);
			obj[i] = NULL;
		}
		return 0;
	}

	// there should be space available.  Do a bulk enqueue.
	num_added = rte_ring_sp_enqueue_bulk(bsr->ring, (void**)obj, n, NULL);

	if (num_added < n) {
		// this should not happen
		printf("WARNING: bsring_enqueue_bulk(): some mbufs failed to enqueue\n");

		// free any remaining mbufs that didn't make it in.
		for (uint32_t i=num_added; i<n; i++) {
			rte_pktmbuf_free(obj[i]);
			obj[i] = NULL;
		}
	}


	// adjust the bsring's usage values
	total_size = 0;
	for (i=0; i<num_added; i++) {
		// we now know this is problematic
		total_size += obj[i]->pkt_len + FRAME_OVERHEAD;
	}

	bsr->bytes_used += total_size;
	return num_added;
}

/**
 * Add up to n mbufs to the ring.
 * Returns the number of mbufs actually added.
 * mbufs not added are freed by this function.
 * This function is safe for a single producer, but expect performance issues
 * with multiple producers.
 */
int bsring_enqueue_burst(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n) {
	uint32_t num_to_add = 0;
	uint32_t num_added = 0;
	
	// in burst mode we add as many packets as will fit.
	// count how many packets we can add from the start of this batch.
	// We start by reserving space in the ring by increasing bsr->bytes_used.
	uint32_t i = 0;
	//bool ring_empty = (bsr->bytes_used == 0);
	if (n>0) {
	uint32_t bytes_to_add = obj[i]->pkt_len + FRAME_OVERHEAD;
	while ((bsr->bytes_used == 0) || ((i < n) && ((bsr->bytes_used+bytes_to_add) < bsr->capacity))) {
		//bsr->bytes_used += (obj[i]->pkt_len + FRAME_OVERHEAD);
		bsr->bytes_used += bytes_to_add;
		num_to_add++;
		i++;
		if (i<n) bytes_to_add = obj[i]->pkt_len + FRAME_OVERHEAD;
	}
	}
/*
	if ((!ring_empty) && (bsr->bytes_used > bsr->capacity) && (num_to_add > 0)) {
		//if (i <= 0) {
		//	printf("i=%d  ring_empty=%d  num_to_add=%d  bytes_used=%u  capacity=%u\n", i, ring_empty, num_to_add, unsigned(bsr->bytes_used), unsigned(bsr->capacity));
		//}
		bsr->bytes_used -= (obj[i-1]->pkt_len + FRAME_OVERHEAD);
		num_to_add--;
	}
*/
	
	// try to add the mbufs
	num_added = rte_ring_sp_enqueue_burst(bsr->ring, (void**)obj, num_to_add, NULL);

	// if any of them failed to add, decrement for the space we didn't use
	for (uint32_t i=num_added; i<num_to_add; i++) {
		bsr->bytes_used -= (obj[i]->pkt_len + FRAME_OVERHEAD);
	}

	// free any mbufs that didn't make it in.
	//for (uint32_t i=num_added; i<num_to_add; i++) {
	for (uint32_t i=num_added; i<n; i++) {
		rte_pktmbuf_free(obj[i]);
		obj[i] = NULL;
	}


	// It's possible that some of the remaining frames are small enough
	// to fit into the remaining space.  Try them iteratively.
	// Free any mbufs that don't get added
	/*
	if (num_added < n) {
		bytes_remaining = bsr->capacity - bsr->bytes_used - bytes_added;
		i = num_to_add;
		while ((i < n) && (bytes_remaining >= 60)) {
			if (((int)(obj[i]->pkt_len) <= bytes_remaining)
				&& (rte_ring_sp_enqueue(bsr->ring, obj[i]))) {
				num_added++;
				bytes_added += (obj[i]->pkt_len);
				bytes_remaining -= (obj[i]->pkt_len);
			} else {
				rte_pktmbuf_free(obj[i]);
				obj[i] = NULL;
			}
			i++;
		}
	}
	*/
	// XXX - We could be incrementing bsr->bytes_used as we enqueue
	//       the mbufs instead of adding them all at the end.
	//bsr->bytes_used += bytes_added;
	return num_added;
}


/**
 * Add one mbuf to the ring.
 * Returns the number of mbufs actually added, 0 or 1.
 * mbufs not added are freed by this function.
 * This function is safe for a single producer, but expect performance issues
 * with multiple producers.
 */
int bsring_enqueue(struct bs_ring* bsr, struct rte_mbuf* obj) {
	if (obj == NULL) {
		return 0;
	}

	// remember the pkt size locally.  Once we enqueue it, it can be
	// dequeued, sent, and freed before we can account for it.
	uint32_t pkt_size = obj->pkt_len + FRAME_OVERHEAD;
	if ((bsr->bytes_used == 0) || ((bsr->bytes_used + pkt_size) < bsr->capacity)) {
		if (rte_ring_sp_enqueue(bsr->ring, obj) == 0) {
			bsr->bytes_used += pkt_size;
			return 1;
		} else {
			// this shouldn't happen
			printf("bsring_enqueue(): rte_ring_sp_enqueue failed\n");
		}
	}
	rte_pktmbuf_free(obj);
	return 0;
}

int bsring_dequeue_burst(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n) {
	uint32_t num_dequeued = rte_ring_sc_dequeue_burst(bsr->ring, (void**)obj, n, NULL);
	uint32_t i = 0;
	if (num_dequeued > 0) {
		for (i=0; i<num_dequeued; i++) {
			bsr->bytes_used -= (obj[i]->pkt_len + FRAME_OVERHEAD);
		}
	}
	return num_dequeued;
}

int bsring_dequeue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, uint32_t n) {
	uint32_t num_dequeued = rte_ring_sc_dequeue_bulk(bsr->ring, (void**)obj, n, NULL);
	uint32_t i = 0;
	if (num_dequeued > 0) {
		for (i=0; i<num_dequeued; i++) {
			bsr->bytes_used -= (obj[i]->pkt_len + FRAME_OVERHEAD);
		}
	}
	return num_dequeued;
}

int bsring_dequeue(struct bs_ring* bsr, struct rte_mbuf** obj) {
	if (rte_ring_sc_dequeue(bsr->ring, (void**)obj) == 0) {
		bsr->bytes_used -= (obj[0]->pkt_len + FRAME_OVERHEAD);
		return 1;
	}
	return 0;
}

int bsring_count(struct bs_ring* bsr) {
	return rte_ring_count(bsr->ring);
}

int bsring_capacity(struct bs_ring* bsr) {
	return bsr->capacity;
}

int bsring_bytesused(struct bs_ring* bsr) {
	return bsr->bytes_used;
}

