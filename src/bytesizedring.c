#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_rwlock.h>
#include <stdio.h>
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
	printf("requested ring capacity: %d\tactual ring capacity: %d\tring size: %d\n",count,rte_ring_get_capacity(bsr->ring),rte_ring_get_size(bsr->ring));
	bsr->used = 0;
	rte_rwlock_init(&(bsr->used_lock));
	if (! bsr->ring) {
		free(bsr);
		return NULL;
	}
	printf("returning bsr\n");
	return bsr;
}

int bsring_enqueue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, int n) {
	int num_added = 0;

	// in bulk mode we either add all or nothing.
	// check if there is space available.
	int i = 0;
	int total_size = 0;
	for (i=0; i<n; i++) {
		total_size += obj[i]->pkt_len;
	}
	if ((bsr->used + total_size) > bsr->capacity) {
		return 0;
	}

	// there should be space available.  Do a bulk enqueue.
	num_added = rte_ring_sp_enqueue_bulk(bsr, obj, n, NULL);

	// adjust the bsring's usage values
	total_size = 0;
	for (i=0; i<num_added; i++) {
		total_size += obj[i]->pkt_len;
	}
	rte_rwlock_write_lock(&(bsr->used_lock));
	bsr->used += total_size;
	rte_rwlock_write_unlock(&(bsr->used_lock));

	return num_added;
}

int bsring_enqueue_burst(struct bs_ring* bsr, struct rte_mbuf** obj, int n) {
	int num_to_add = 0;
	int num_added = 0;
	int bytes_added = 0;
	
	// in burst mode we add as many packets as will fit.
	// count how many packets we can add from the start of this batch.
	int i = 0;
	int bytes_remaining = bsr->capacity - bsr->used;
	while ((i < n) && (bytes_remaining > 0)) {
		bytes_remaining -= (obj[i]->pkt_len);
		num_to_add++;
	}
	
	num_added = rte_ring_sp_enqueue_burst(bsr, obj, num_to_add, NULL);
	for (i=0; i<num_added; i++) {
		bytes_added += (obj[i]->pkt_len);
	}

	// it's possible that some of the remaining packets are small enough
	// to fit into the remaining space.  Try them iteratively.
	if (num_added < n) {
		bytes_remaining = bsr->capacity - bsr->used - bytes_added;
		i = num_to_add;
		while ((i < n) && (bytes_remaining >= 60)) {
			if (obj[i]->pkt_len < bytes_remaining) {
				if (rte_ring_sp_enqueue(bsr, obj[i])) {
					num_added++;
					bytes_added += (obj[i]->pkt_len);
					bytes_remaining -= (obj[i]->pkt_len);
				}
			}
			i++;
		}
	}
	
	rte_rwlock_write_lock(&(bsr->used_lock));
	bsr->used += bytes_added;
	rte_rwlock_write_unlock(&(bsr->used_lock));

	return num_added;
}


int bsring_enqueue(struct bs_ring* bsr, struct rte_mbuf* obj) {
	if ((bsr->used + obj->pkt_len) < bsr->capacity) {
		if (rte_ring_sp_enqueue(bsr->ring, obj) == 0) {
			rte_rwlock_write_lock(&(bsr->used_lock));
			bsr->used += (obj->pkt_len);
			rte_rwlock_write_unlock(&(bsr->used_lock));
			return 1;
		} else {
			printf("bsring_enqueue(): rte_ring_sp_enqueue failed");
		}
	} else {
		printf("bsring_enqueue(): cannot add, ring is full with %d frames \t%d\t%d\n",rte_ring_count(bsr->ring),bsr->used,bsr->capacity);
	}
	return 0;
}

int bsring_dequeue_burst(struct bs_ring* bsr, struct rte_mbuf** obj, int n) {
	int num_dequeued = rte_ring_sc_dequeue_burst(bsr->ring, (void**)obj, n, NULL);
	int i = 0;
	if (num_dequeued > 0) {
		rte_rwlock_write_lock(&(bsr->used_lock));
		for (i=0; i<num_dequeued; i++) {
			bsr->used -= (obj[i]->pkt_len);
		}
		rte_rwlock_write_unlock(&(bsr->used_lock));
	}
	return num_dequeued;
}

int bsring_dequeue_bulk(struct bs_ring* bsr, struct rte_mbuf** obj, int n) {
	int num_dequeued = rte_ring_sc_dequeue_bulk(bsr->ring, (void**)obj, n, NULL);
	int i = 0;
	if (num_dequeued > 0) {
		rte_rwlock_write_lock(&(bsr->used_lock));
		for (i=0; i<num_dequeued; i++) {
			bsr->used -= (obj[i]->pkt_len);
		}
		rte_rwlock_write_unlock(&(bsr->used_lock));
	}
	return num_dequeued;
}

int bsring_dequeue(struct bs_ring* bsr, struct rte_mbuf** obj) {
	if (rte_ring_sc_dequeue(bsr->ring, (void**)obj) == 0) {
		rte_rwlock_write_lock(&(bsr->used_lock));
		bsr->used -= (obj[0]->pkt_len);
		rte_rwlock_write_unlock(&(bsr->used_lock));
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
	return bsr->used;
}

