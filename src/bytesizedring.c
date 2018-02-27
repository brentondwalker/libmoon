#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include "bytesizedring.h"

// DPDK SPSC bounded ring buffer
/*
 * This wraps the DPDK SPSC bounded ring buffer into a structure whose capacity
 * limits the number of bytes it can hold.
 */

struct bs_ring* create_bsring(uint32_t capacity, int32_t socket) {
	static volatile uint32_t ring_cnt = 0;
	int count_min = capacity/60;
	int count = 1;
	while (count < count_min && count <= RING_SIZE_LIMIT) {
		count *= 2;
	}
	char ring_name[32];
	struct bs_ring* bsr = (struct bs_ring*)malloc(sizeof(struct bs_ring*));
	printf("create_bsring(%d,%d)\n",capacity,socket);
	bsr->capacity = capacity;
	sprintf(ring_name, "mbuf_bs_ring%d", __sync_fetch_and_add(&ring_cnt, 1));
	bsr->ring = rte_ring_create(ring_name, count, socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
	bsr->used = 0;
	if (! bsr->ring) {
		free(bsr);
		return NULL;
	}
	printf("returning bsr\n");
	return bsr;
}

int bsring_enqueue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, int n) {
	int num_added = 0;
	int i = 0;
	for (i=0; i<n; i++) {
		num_added += bsring_enqueue(bsr, obj[i]);
	}
	return num_added;
}

int bsring_enqueue(struct bs_ring* bsr, struct rte_mbuf* obj) {
	if (bsr->used < bsr->capacity) {
		if (rte_ring_sp_enqueue(bsr->ring, obj) == 0) {
			bsr->used += obj->pkt_len;
			return 1;
		}
	}
	return 0;
}

int bsring_dequeue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, int n) {
	int num_dequeued = rte_ring_sc_dequeue_bulk(bsr->ring, (void**)obj, n, NULL);
	int i = 0;
	for (i=0; i<num_dequeued; i++) {
		bsr->used -= obj[i]->pkt_len;
	}
	return num_dequeued;
}

int bsring_count(struct bs_ring* bsr) {
	return rte_ring_count(bsr->ring);
}

int bsring_capacity(struct bs_ring* bsr) {
	return bsr->capacity;
}

int bsring_bytesused(struct bs_ring* bsr) {
	return bsr->used;
}

