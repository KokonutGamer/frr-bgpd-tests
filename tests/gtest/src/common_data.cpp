#include "common_data.h"

#include <charconv>
#include <utility>
#include <variant>

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

  nlri.prefix.pref = {.family = AF_INET6};
  std::from_chars(pref.c_str() + pos + 1, pref.c_str() + pref.size(),
                  nlri.prefix.pref.prefixlen);

  inet_pton(AF_INET6, addr.c_str(), &nlri.prefix.pref.u.prefix6);

  // must set valid flags
  SET_FLAG(nlri.local.tlvs,
           std::to_underlying(LinkState::NodeDescTLV::IGP_ROUTER_BIT));
  SET_FLAG(nlri.prefix.tlvs,
           std::to_underlying(LinkState::PrefixDescTLV::IP_REACH_BIT));

  return nlri;
}

BgpLsLinkNlri::operator BApiLinkStateUpdate<LinkStateAttributes>() const {
  uint8_t level = static_cast<uint8_t>(LinkState::Protocol::ISIS_L1);

  BApiLinkStateUpdate<LinkStateAttributes> message{
      .event = BEvent::UPDATE,
      .data = {.local = this->link.interface,
               .remote = this->link.neighbor,
               .adv_node = {.iso_sys_id = this->source.igp_router_id,
                            .level = level},
               .remote_node = {.iso_sys_id = this->destination.igp_router_id,
                               .level = level}}};
  return message;
}

BgpLsPrefixNlri::operator BApiLinkStateUpdate<LinkStatePrefix>() const {
  BApiLinkStateUpdate<LinkStatePrefix> message{
      .event = BEvent::UPDATE,
      .data = {
          .adv = {.iso_sys_id = this->local_node.igp_router_id,
                  .level = static_cast<uint8_t>(LinkState::Protocol::ISIS_L1)},
          .prefix = this->prefix.prefix}};
  return message;
}

}  // namespace Model
