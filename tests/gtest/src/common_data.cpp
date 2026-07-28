#include "common_data.h"

#include "lib/zebra.h"
#include "linkstate_data.h"
#include "utils.hpp"

namespace Model {

BgpLsLinkNlri::operator LinkState::LinkNlri() const {
  // IMPORTANT NOTE
  // When converting a `BgpLsLinkNlri` to a `LinkNlri`, we REVERSE the direction
  // of the edge. This may be because advertising the forward edge first does
  // not actually install the entry until the reverse edge is processed in the
  // Google Test implementation. Will need to check this again in the future.
  LinkState::LinkNlri nlri{.proto = LinkState::Protocol::ISIS_L1};

  nlri.local.igpRouterIdLen = SysIdToBuffer(nlri.remote.igpRouterId.sysid,
                                            this->source.igp_router_id.c_str());
  nlri.remote.igpRouterIdLen = SysIdToBuffer(
      nlri.local.igpRouterId.sysid, this->destination.igp_router_id.c_str());

  inet_pton(AF_INET6, this->link.interface.c_str(), &nlri.link.ipv6Neigh);
  inet_pton(AF_INET6, this->link.neighbor.c_str(), &nlri.link.ipv6Intf);

  // must set valid flags
  SET_FLAG(nlri.local.tlvs, LinkState::NodeDescTLV::IGP_ROUTER_BIT);
  SET_FLAG(nlri.remote.tlvs, LinkState::NodeDescTLV::IGP_ROUTER_BIT);
  SET_FLAG(nlri.link.tlvs, LinkState::LinkDescTLV::IPV6_INTF_BIT);
  SET_FLAG(nlri.link.tlvs, LinkState::LinkDescTLV::IPV6_NEIGH_BIT);

  return nlri;
}

LinkStateEdge::operator BApiLinkStateUpdate() const {
  BApiLinkStateUpdate message{.event = BEvent::UPDATE,
                              .remote = this->destination_node,
                              .data = {.adv = this->source_node,
                                       .metric = 0,
                                       .local = this->source,
                                       .remote = this->destination}};
  return message;
}

}  // namespace Model
