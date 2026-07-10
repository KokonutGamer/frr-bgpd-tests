#ifndef COMMON_DATA_H
#define COMMON_DATA_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using sys_id_t = std::string;
using prefix_t = std::string;
using addr_t = std::string;

namespace Model {
struct BgpLsNode {
  uint32_t asn;
  addr_t igp_router_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsNode, asn, igp_router_id)

struct BgpLsLink {
  addr_t interface;
  addr_t neighbor;
  uint32_t remote_asn;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsLink, interface, neighbor, remote_asn)

struct BgpLsLinkNlri {
  BgpLsNode source;
  BgpLsNode destination;
  BgpLsLink link;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsLinkNlri, source, destination, link)

struct LinkStateNodeId {
  sys_id_t iso_sys_id;
  uint8_t level;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkStateNodeId, iso_sys_id, level)

struct LinkStateEdge {
  uint32_t asn;
  LinkStateNodeId source_node;
  LinkStateNodeId dest_node;
  addr_t source;
  addr_t destination;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkStateEdge, asn, source_node, dest_node,
                                   source, destination)

struct LinkStateAttributes {
  bool valid;
  LinkStateNodeId adv;
  std::string name;
  uint32_t metric;
  addr_t local;
  addr_t remote;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkStateAttributes, valid, adv, name,
                                   metric, local, remote)

struct BgpRibEntry {
  prefix_t synth_prefix;
  BgpLsLinkNlri ls_nlri;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpRibEntry, synth_prefix, ls_nlri)

enum class BEvent : uint8_t { UNDEF = 0, SYNC, ADD, UPDATE, DELETE };

struct BApiLinkStateUpdate {
  BEvent event;
  LinkStateNodeId remote;
  LinkStateAttributes data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BApiLinkStateUpdate, event, remote, data)

struct BgpLsLinkState {
  uint32_t asn;
  std::vector<LinkStateEdge> ted;
  std::vector<BgpRibEntry> rib;
  prefix_t next_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsLinkState, asn, ted, rib, next_id)

struct TestCase {
  int test_id;
  std::string op;  // currently always "api_bgp_ls_edge_update"
  BgpLsLinkState initial_state;
  BApiLinkStateUpdate api_param;
  BgpLsLinkState final_state;
  int response;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TestCase, test_id, op, initial_state,
                                   api_param, final_state, response)

}  // namespace Model

#endif  // COMMON_DATA_H
