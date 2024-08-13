#ifndef MG_MBUF_UTILS_H
#define MG_MBUF_UTILS_H

#include <rte_mbuf.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Newer version of DPDK have a function for this.  Our version does not,
   * so I implement a basic deep copy function.
   */
  inline rte_mbuf* diy_mbuf_copy(struct rte_mempool *pktmbuf_pool, struct rte_mbuf* src_mbf) {

    if (src_mbf->nb_segs > 1) {
      printf("ERROR: diy_mbuf_copy(): cannot copy an mbuf with more than one segment!\n");
    }

    static struct rte_mbuf* mbf = NULL;
    mbf = rte_pktmbuf_alloc(pktmbuf_pool);
    //printf("diy allocated mbuf: %p\n", mbf);

    if (mbf == NULL) {
      printf("ERROR: diy_mbuf_copy(): failed to allocate mbuf.\n");
    } else {
      // our packets are simple, right?  Just copy the basic values?
      mbf->pkt_len = src_mbf->pkt_len;
      mbf->data_len = src_mbf->data_len;
      mbf->data_off = src_mbf->data_off;
      mbf->port = src_mbf->port;
      mbf->ol_flags = src_mbf->ol_flags;
    
      //mbf->rx_descriptor_fields1 = src_mbf->rx_descriptor_fields1;  // just a marker
      mbf->packet_type = src_mbf->packet_type;
      mbf->vlan_tci = src_mbf->vlan_tci;
      mbf->vlan_tci_outer = src_mbf->vlan_tci_outer;
      mbf->timestamp = src_mbf->timestamp;
      mbf->udata64 = src_mbf->udata64;
      //printf("copied udata64 = %016lX\n", mbf->udata64);
      mbf->timesync = src_mbf->timesync;
      mbf->seqn = src_mbf->seqn;
      rte_memcpy(mbf->buf_addr, src_mbf->buf_addr, src_mbf->buf_len);
      //printf("allocated new mbuf: %p\tbuf_addr: %p\n", mbf, mbf->buf_addr);
    }
    
    return mbf;
  }
  
#ifdef __cplusplus
  }
#endif

#endif

