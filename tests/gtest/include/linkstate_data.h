/**
 * Replicates BGP-LS primitive data structures required within FRR.
 *
 * These data structures are taken from `bgpd/bgp_ls_nlri.h`. Attempting to
 * include this header within C++ code causes a domino effect: errors ranging
 * from usages of C++ keywords as variable names, returning void pointers when
 * function return type is specified as a non-void pointer, requiring a C++
 * compiler for template definitions, etc. This comes from the fact that FRR's
 * main library, found in the `lib/` directory, was made with C++ in mind.
 * However, daemon-specific libraries, such as `bgpd/` and `isisd/`, are written
 * in pure C.
 */
#ifndef LINKSTATE_DATA_H
#define LINKSTATE_DATA_H

#include <netinet/in.h>

#include <cstdint>

#include "lib/prefix.h"

static constexpr int IGP_ROUTER_ID_ISIS_LEN = 6;
static constexpr int IGP_ROUTER_ID_MAX_SIZE = 16;

using as_t = uint32_t;

namespace Model {
namespace LinkState {

/**
 * @brief Replicates `thash_item` in FRR.
 */
struct HashItem {
  HashItem *next;
  uint32_t val;
};

/**
 * @brief Replicates `bgp_ls_nlri_hash_item` (predeclared) in FRR.
 */
struct NlriHash {
  HashItem item;
};

/**
 * @brief Replicates `bgp_ls_nlri_type` in FRR.
 */
enum class NlriType : int {
  RESERVED = 0,
  NODE,
  LINK,
  IPV4_PREFIX,
  IPV6_PREFIX,
  SRV6_SID = 6
};

/**
 * @brief Replicates `bgp_ls_protocol_id` in FRR.
 */
enum class Protocol : int {
  RESERVED = 0,
  ISIS_L1,
  ISIS_L2,
  OSPFV2,
  DIRECT,
  STATIC,
  OSPFV3,
  BGP
};

/**
 * @brief Replicates `bgp_ls_ospf_route_type` in FRR.
 */
enum class OspfRouteType : int {
  INTRA_AREA = 1,
  INTER_AREA,
  EXTERNAL_1,
  EXTERNAL_2,
  NSSA_1,
  NSSA_2
};

/**
 * @brief Replicates `bgp_ls_bgp_route_type` in FRR.
 */
enum class BgpRouteType : int {
  LOCAL = 1,
  ATTACHED,
  EXTERNAL_BGP,
  INTERNAL_BGP,
  REDISTRIBUTED
};

enum NodeDescTLV : uint16_t {
  AS_BIT = (1ULL << 0),
  BGP_LS_ID_BIT = (1ULL << 1),
  OSPF_AREA_BIT = (1ULL << 2),
  IGP_ROUTER_BIT = (1ULL << 3),
  BGP_ROUTER_ID_BIT = (1ULL << 4)
};

/**
 * @brief Replicates `bgp_ls_node_descriptor` in FRR.
 */
struct NodeDesc {
  uint16_t tlvs;
  as_t asn;
  uint32_t lsId;
  uint32_t ospfAreaId;
  uint8_t igpRouterIdLen;
  union {
    uint8_t sysid[IGP_ROUTER_ID_ISIS_LEN];
    struct {
      uint8_t sysid[IGP_ROUTER_ID_ISIS_LEN];
      uint8_t psn;
    } pseudoIsis;
    in_addr ospf;
    struct {
      in_addr routerId;
      in_addr ifaddr;
    } pseudoOspf;
    in_addr ipv4;
    in6_addr ipv6;
    uint8_t raw[IGP_ROUTER_ID_MAX_SIZE];
  } igpRouterId;
  in_addr bgpRouterId;
};

enum LinkDescTLV : uint32_t {
  LINK_ID_BIT = (1ULL << 0),
  IPV4_INTF_BIT = (1ULL << 1),
  IPV4_NEIGH_BIT = (1ULL << 2),
  IPV6_INTF_BIT = (1ULL << 3),
  IPV6_NEIGH_BIT = (1ULL << 4),
  MT_ID_BIT = (1ULL << 5),
  REMOTE_AS_BIT = (1ULL << 6)
};

/**
 * @brief Replicates `bgp_ls_link_descriptor` in FRR.
 */
struct LinkDesc {
  uint32_t tlvs;
  uint32_t localId;
  uint32_t remoteId;
  in_addr ipv4Intf;
  in_addr ipv4Neigh;
  in6_addr ipv6Intf;
  in6_addr ipv6Neigh;
  as_t remoteAsn;
  uint16_t mtId;
};

/**
 * @brief Replicates `bgp_ls_prefix_descriptor` in FRR.
 */
struct PrefixDesc {
  uint16_t tlvs;
  uint16_t mtId;
  OspfRouteType ospfRT;
  BgpRouteType bgpRT;
  prefix prefix;
};

/**
 * @brief Replicates `bgp_ls_srv6_sid_descriptor` in FRR.
 */
struct Srv6SidDesc {
  uint16_t tlvs;
  in6_addr sid;
  uint16_t mtId;
};

/**
 * @brief Replicates `bgp_ls_node_nlri` in FRR.
 */
struct NodeNlri {
  Protocol proto;
  uint64_t id;
  NodeDesc node;
};

/**
 * @brief Replicates `bgp_ls_link_nlri` in FRR.
 */
struct LinkNlri {
  Protocol proto;
  uint64_t id;
  NodeDesc local;
  NodeDesc remote;
  LinkDesc link;
};

/**
 * @brief Replicates `bgp_ls_prefix_nlri` in FRR.
 */
struct PrefixNlri {
  Protocol proto;
  uint64_t id;
  NodeDesc local;
  PrefixDesc prefix;
};

/**
 * @brief Replicates `bgp_ls_srv6_sid_nlri` in FRR.
 */
struct Srv6Nlri {
  Protocol proto;
  uint64_t id;
  NodeDesc local;
  Srv6SidDesc sid;
};

/**
 * @brief Replicates `bgp_ls_nlri` in FRR.
 */
struct ParentNlri {
  NlriType type;
  union {
    NodeNlri node;
    LinkNlri link;
    PrefixNlri prefix;
    Srv6Nlri srv6;
  } data;
  unsigned long refcnt;
  uint32_t id;
  NlriHash hash;
};

}  // namespace LinkState
}  // namespace Model

#endif  // LINKSTATE_DATA_H
