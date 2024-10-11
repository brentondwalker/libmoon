#include <rte_config.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_rwlock.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <stdio.h>
#include "pktsizedring.hpp"
#include "mbuf_utils.hpp"

// DPDK SPSC bounded ring buffer
/*
 * This wraps the DPDK SPSC bounded ring buffer into a structure whose capacity
 * limits the number of packets/frames it can hold.
 * In the plain implementation the ring size must be a power of 2.
 */

struct ps_ring* create_psring(uint32_t capacity, int32_t socket, bool copy_mbufs) {
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

	psr->copy_mbufs = copy_mbufs;
	psr->pktmbuf_pool = NULL;
	if (copy_mbufs) {
	  char pool_name[32];
	  sprintf(pool_name, "psring_pool%d", __sync_fetch_and_add(&ring_cnt, 1));
	  // mem pools are supposed to be of size (2^n - 1)
	  // they also seem to have a minimum size of 2^10 -1, which I don't find documented anywhere.
	  int pool_size = (count < PS_RING_MEMPOOL_MIN_SIZE) ? PS_RING_MEMPOOL_MIN_SIZE : count;
	  psr->pktmbuf_pool = rte_pktmbuf_pool_create(pool_name, (pool_size-1),
						      PS_RING_MEMPOOL_CACHE_SIZE, 0,
						      PS_RING_MEMPOOL_BUF_SIZE,
						      SOCKET_ID_ANY);
	  if (psr->pktmbuf_pool == NULL) {
	    rte_exit(EXIT_FAILURE, "Cannot init mbuf pool: %s\n", rte_strerror(rte_errno));
	  } else {
	    printf("Allocated mbuf pool:\n");
	    printf("pool=%s\tpool_size=%d\tpool_populated_size=%d\tpool_elt_size=%d\n",
		   psr->pktmbuf_pool->name, psr->pktmbuf_pool->size,
		   psr->pktmbuf_pool->populated_size, psr->pktmbuf_pool->elt_size);
	    printf("memzone_len=%lu\tmemzone_hpsz=%lu\tmemzone_name=%s\n",
		   psr->pktmbuf_pool->mz->len, psr->pktmbuf_pool->mz->hugepage_sz,
		   psr->pktmbuf_pool->mz->name);
	  }
	}
	
	return psr;
}

int psring_enqueue_bulk(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n) {
	if ((rte_ring_count(psr->ring) + n) < psr->capacity) {
		return psring_enqueue_burst(psr, obj, n);
	}
	return 0;
}

int psring_enqueue_burst(struct ps_ring* psr, struct rte_mbuf** obj, uint32_t n) {
	uint32_t count = rte_ring_count(psr->ring);
	//printf("\tpsring count is %d\n", count);

	int num_added = 0;
	if (count < psr->capacity) {
	  uint32_t num_to_add = ((count + n) > psr->capacity) ? (psr->capacity - count) : n;

	  if (psr->copy_mbufs) {
	    static struct rte_mbuf* mbf = NULL;
	    for (uint32_t i=0; i<num_to_add; i++) {
	      mbf = diy_mbuf_copy(psr->pktmbuf_pool, obj[i]);
	      if (mbf == NULL) {
		printf("ERROR: failed to copy mbuf %d.\n", i);
		break;
	      } else {
		if (rte_ring_sp_enqueue(psr->ring, mbf) == 0) {
		  num_added++;
		} else {
		  printf("failed to enqueue the copied mbuf!\n");
		  rte_pktmbuf_free(mbf);
		}
	      }
	      rte_pktmbuf_free(obj[i]);
	    }
	  } else {
	    // if we aren't copying the mbufs, use the native enqueue_burst() funcion
	    num_added = rte_ring_sp_enqueue_burst(psr->ring, (void**)obj, num_to_add, NULL);
	  }
	} else {
	  // psring is full
	  //printf("psring isa full!!\n");
	}

	// free the remaining mbufs that didn't make it in.
	for (uint32_t i=num_added; i<n; i++) {
	  rte_pktmbuf_free(obj[i]);
	  obj[i] = NULL;
	}
	
	return num_added;
}

int psring_enqueue(struct ps_ring* psr, struct rte_mbuf* obj) {
	if ((rte_ring_count(psr->ring) + 1) <= psr->capacity) {
	  if (psr->copy_mbufs) {
	    static struct rte_mbuf* mbf = NULL;
	    mbf = diy_mbuf_copy(psr->pktmbuf_pool, obj);
	    if (mbf == NULL) {
	      printf("ERROR: failed to copy mbuf.\n");
	      return 0;
	    }
	    rte_pktmbuf_free(obj);
	    obj = mbf;
	  }
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
	//uint32_t count = rte_ring_count(psr->ring);
	//if (count > 0) printf("\tTXTX psring count is %d\n", count);
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



