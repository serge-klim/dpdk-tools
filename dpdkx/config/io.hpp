#pragma once
#include "device.hpp"
#include <boost/describe.hpp>
#include <iosfwd>
#include <string>

namespace dpdkx { inline namespace v0 { namespace config {

std::ostream& operator << (std::ostream& out, device const& dev);
std::string to_string(device const& dev);
}

} /*inline namespace v0 */ } // namespace dpdkx

//BOOST_DESCRIBE_STRUCT(rte_eth_dev_portconf, (),
//        (
//        burst_size, /**< Device-preferred burst size */
//        ring_size,  /**< Device-preferred size of queue rings */
//        nb_queues   /**< Device-preferred number of queues */
//        ));

        
BOOST_DESCRIBE_STRUCT( rte_eth_dev_portconf, (),
(
    burst_size, /**< Device-preferred burst size */
    ring_size,  /**< Device-preferred size of queue rings */
    nb_queues   /**< Device-preferred number of queues */
)
)

BOOST_DESCRIBE_STRUCT(rte_eth_rxconf, (),
(
	rx_thresh, /**< Rx ring threshold registers. */
	rx_free_thresh, /**< Drives the freeing of Rx descriptors. */
	rx_drop_en, /**< Drop packets if no descriptors are available. */
	rx_deferred_start, /**< Do not start queue with rte_eth_dev_start(). */
	rx_nseg, /**< Number of descriptions in rx_seg array. */
	/**
	 * Share group index in Rx domain and switch domain.
	 * Non-zero value to enable Rx queue share, zero value disable share.
	 * PMD is responsible for Rx queue consistency checks to avoid member
	 * port's configuration contradict to each other.
	 */
	share_group,
	share_qid, /**< Shared Rx queue ID in group */
	/**
	 * Per-queue Rx offloads to be set using RTE_ETH_RX_OFFLOAD_* flags.
	 * Only offloads set on rx_queue_offload_capa or rx_offload_capa
	 * fields on rte_eth_dev_info structure are allowed to be set.
	 */
	offloads,
	/**
	 * Points to the array of segment descriptions for an entire packet.
	 * Array elements are properties for consecutive Rx segments.
	 *
	 * The supported capabilities of receiving segmentation is reported
	 * in rte_eth_dev_info.rx_seg_capa field.
	 */
	//rte_eth_rxseg* rx_seg,
	/**
	 * Array of mempools to allocate Rx buffers from.
	 *
	 * This provides support for multiple mbuf pools per Rx queue.
	 * The capability is reported in device info via positive
	 * max_rx_mempools.
	 *
	 * It could be useful for more efficient usage of memory when an
	 * application creates different mempools to steer the specific
	 * size of the packet.
	 *
	 * If many mempools are specified, packets received using Rx
	 * burst may belong to any provided mempool. From ethdev user point
	 * of view it is undefined how PMD/NIC chooses mempool for a packet.
	 *
	 * If Rx scatter is enabled, a packet may be delivered using a chain
	 * of mbufs obtained from single mempool or multiple mempools based
	 * on the NIC implementation.
	 */
	//struct rte_mempool** rx_mempools,
	rx_nmempool /** < Number of Rx mempools */
)
)

BOOST_DESCRIBE_STRUCT(rte_eth_thresh, (),
  (
    pthresh, /**< Ring prefetch threshold. */
    hthresh, /**< Ring host threshold. */
    wthresh /**< Ring writeback threshold. */
)
)

BOOST_DESCRIBE_STRUCT( rte_eth_txconf, (),
   (
	tx_thresh, /**< Tx ring threshold registers. */
	tx_rs_thresh, /**< Drives the setting of RS bit on TXDs. */
	tx_free_thresh, /**< Start freeing Tx buffers if there are
				      less free descriptors than this value. */

	tx_deferred_start, /**< Do not start queue with rte_eth_dev_start(). */
	/**
	 * Per-queue Tx offloads to be set  using RTE_ETH_TX_OFFLOAD_* flags.
	 * Only offloads set on tx_queue_offload_capa or tx_offload_capa
	 * fields on rte_eth_dev_info structure are allowed to be set.
	 */
	offloads
)
)

BOOST_DESCRIBE_STRUCT(rte_eth_desc_lim, (),
   (
	    nb_max,   /**< Max allowed number of descriptors. */
		nb_min,   /**< Min allowed number of descriptors. */
		nb_align, /**< Number of descriptors should be aligned to. */

		/**
			* Max allowed number of segments per whole packet.
			*
			* - For TSO packet this is the total number of data descriptors allowed
			*   by device.
			*
			* @see nb_mtu_seg_max
			*/
		nb_seg_max,

		/**
			* Max number of segments per one MTU.
			*
			* - For non-TSO packet, this is the maximum allowed number of segments
			*   in a single transmit packet.
			*
			* - For TSO packet each segment within the TSO may span up to this
			*   value.
			*
			* @see nb_seg_max
			*/
		nb_mtu_seg_max
)
)

BOOST_DESCRIBE_STRUCT(rte_eth_dev_info, (),
    ( 
        min_mtu,   /**< Minimum MTU allowed */
        max_mtu,   /**< Maximum MTU allowed */
        min_rx_bufsize,
        /**
         * Maximum Rx buffer size per descriptor supported by HW.
         * The value is not enforced, information only to application to
         * optimize mbuf size.
         * Its value is UINT32_MAX when not specified by the driver.
         */
        max_rx_bufsize,
        max_rx_pktlen, /**< Maximum configurable length of Rx pkt. */
        /** Maximum configurable size of LRO aggregated packet. */
        max_lro_pkt_size,
        max_rx_queues, /**< Maximum number of Rx queues. */
        max_tx_queues, /**< Maximum number of Tx queues. */
        max_mac_addrs, /**< Maximum number of MAC addresses. */
        /** Maximum number of hash MAC addresses for MTA and UTA. */
        max_hash_mac_addrs,
        max_vfs, /**< Maximum number of VFs. */
        max_vmdq_pools, /**< Maximum number of VMDq pools. */
        rx_offload_capa,
        tx_offload_capa,
        rx_queue_offload_capa,
        tx_queue_offload_capa,
        /** Device redirection table size, the total number of entries. */
        reta_size,
        //	    hash_key_size, /**< Hash key size in bytes */
        rss_algo_capa, /** RSS hash algorithms capabilities */
        /** Bit mask of RSS offloads, the bit offset also means flow type */
        flow_type_rss_offloads,
        default_rxconf, /**< Default Rx configuration */
        default_txconf, /**< Default Tx configuration */
        vmdq_queue_base, /**< First queue ID for VMDq pools. */
        vmdq_queue_num,  /**< Queue number for VMDq pools. */
        vmdq_pool_base,  /**< First ID of VMDq pools. */
		rx_desc_lim,  /**< Rx descriptors limits */
		tx_desc_lim,  /**< Tx descriptors limits */
        speed_capa,  /**< Supported speeds bitmap (RTE_ETH_LINK_SPEED_). */
        nb_rx_queues,
        nb_tx_queues,
        default_rxportconf,
        default_txportconf,
        dev_capa
    )
)


namespace dpdkx { inline namespace v0 { namespace config {
     
 BOOST_DESCRIBE_STRUCT(device::effective_offloads, (),
	 (
		 rx,
		 tx
	 )
 )

 BOOST_DESCRIBE_STRUCT(device, (),
	 (
		 info,
		 effective_offload,
		 svc_tx_n,
		 next_src_port,
		 rx_reconfig_hint,
         clock_hz,
		 rx_meta_features,
	     features
	 )
 )



}/*namespace dpdkx*/ }/*::v0*/ } // namespace config
