#pragma once
#include "rte_byteorder.h"
#include "rte_common.h"
#include <cstring>
#include <cstdint>


namespace dpdkx {

struct __rte_aligned(2) __rte_packed_begin igmpv3_mem_rep {
    //rte_be32_t          ip_header_options /*= 0x040400*/
    enum igmp_type : std::uint8_t {
        membership_report = 0x22
    } type;	                            /* version & type of IGMP message*/
    std::uint8_t		reserved1;	    /* Reserved*/
    rte_be16_t			cksum;	        /* IP-style checksum */
    rte_be16_t			reserved2;	    /* Reserved */
    rte_be16_t			num_records;	/* Number of Group Records (M) */
    /*struct in_addr	sources[1];*/   /* source addresses */
} __rte_packed_end;

struct __rte_aligned(2) __rte_packed_begin igm_record {
    enum igmp_type : std::uint8_t {
        change_to_exclude_mode = 4,
        change_to_include_mode = 3
    } type;	/* version & type of IGMP message*/
    std::uint8_t len;	/*Aux Data Len*/
    rte_be16_t	num_sources;	/* number of sources */
} __rte_packed_end;

} // namespace dpdkx

