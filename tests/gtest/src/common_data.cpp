#include "common_data.h"

#include <charconv>
#include <utility>

#include "lib/prefix.h"
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
  SET_FLAG(nlri.local.tlvs,
           std::to_underlying(LinkState::NodeDescTLV::IGP_ROUTER_BIT));
  SET_FLAG(nlri.remote.tlvs,
           std::to_underlying(LinkState::NodeDescTLV::IGP_ROUTER_BIT));
  SET_FLAG(nlri.link.tlvs,
           std::to_underlying(LinkState::LinkDescTLV::IPV6_INTF_BIT));
  SET_FLAG(nlri.link.tlvs,
           std::to_underlying(LinkState::LinkDescTLV::IPV6_NEIGH_BIT));

  return nlri;
}

BgpLsPrefixNlri::operator LinkState::PrefixNlri() const {
  LinkState::PrefixNlri nlri{.proto = LinkState::Protocol::ISIS_L1};

  nlri.local.igpRouterIdLen = SysIdToBuffer(
      nlri.local.igpRouterId.sysid, this->local_node.igp_router_id.c_str());

  auto& pref = this->prefix.prefix;

  std::size_t pos = pref.find("/");
  std::string addr = pref.substr(0, pos);

  nlri.prefix.prefix = {.family = AF_INET6};
  std::from_chars(pref.c_str() + pos + 1, pref.c_str() + pref.size(),
                  nlri.prefix.prefix.prefixlen);

  inet_pton(AF_INET6, addr.c_str(), &nlri.prefix.prefix.u.prefix6);

  // TODO verify this is what FRR generates
  nlri.prefix.bgpRT = LinkState::BgpRouteType::LOCAL;

  // must set valid flags
  SET_FLAG(nlri.local.tlvs,
           std::to_underlying(LinkState::NodeDescTLV::IGP_ROUTER_BIT));
  SET_FLAG(nlri.prefix.tlvs,
           std::to_underlying(LinkState::PrefixDescTLV::IP_REACH_BIT));
  SET_FLAG(nlri.prefix.tlvs,
           std::to_underlying(LinkState::PrefixDescTLV::BGP_ROUTE_TYPE_BIT));

  return nlri;
}

LinkStateEdge::operator BApiLinkStateUpdate() const {
  BApiLinkStateUpdate message{.event = BEvent::UPDATE,
                              .remote = this->destination_node,
                              .data = {.adv = this->source_node,
                                       .local = this->source,
                                       .remote = this->destination}};
  return message;
}

}  // namespace Model
