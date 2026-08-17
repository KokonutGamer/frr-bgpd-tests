#include "test_bgp_ls_linkstate_update.h"

#include <arpa/inet.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <variant>

#include "frr_bridge.h"
#include "lib/stream.h"
#include "lib/zclient.h"
#include "linkstate_data.h"
#include "sbuf.h"
#include "utils.hpp"

namespace Model {

void LinkStateTest::SetUp() {
  ASSERT_TRUE(bridge_check_bgpd_running())
      << "[Fixture SetUp]: bgpd is not running.";
  ASSERT_TRUE(bridge_check_ls_clear())
      << "[Fixture SetUp]: BGP-LS TED is not empty.";
}

void LinkStateTest::TearDown() { bridge_clear_bgp_ls_ted(); }

void LinkStateTest::SetDebugMode(bool debug) {
  LinkStateTest::DebugMode = debug;
}

void LinkStateTest::ArrangeLinkTest(const TestCase& tc) const {
  const LinkStateAttributes* attr =
      std::get_if<LinkStateAttributes>(&tc.api_param.data);

  ASSERT_FALSE(attr == nullptr)
      << "[DataVar]: tc.api_param.data is not type LinkStateAttributes.";

  if (IsSysIdUnspecified(attr->adv.iso_sys_id.c_str()) ||
      IsSysIdUnspecified(tc.api_param.remote.iso_sys_id.c_str()) ||
      IsIpv6Unspecified(attr->local.c_str()) ||
      IsIpv6Unspecified(attr->remote.c_str())) {
    GTEST_SKIP() << "[ls_attr]: test " << tc.test_id
                 << " provides no meaningful input.";
  }

  // Note that for arrange, we also want to place TED entries before the one
  // we actually want to test
  for (const TedVar& var : tc.initial_state.ted) {
    if (const LinkStateEdge* edge = std::get_if<LinkStateEdge>(&var)) {
      ls_attributes* tempAttr;
      BApiLinkStateUpdate msg = static_cast<BApiLinkStateUpdate>(*edge);
      SendLinkUpdateMessage(msg, tempAttr);
      ls_attributes_del(tempAttr);
    } else if (const LinkStateSubnet* subnet =
                   std::get_if<LinkStateSubnet>(&var)) {
      // TODO
    }
    // ignore other alternatives
  }
}

void LinkStateTest::NodeIdToFrr(const LinkStateNodeId& nodeId,
                                ls_node_id& frrNodeId) const {
  // for now, we default to IS-IS level 1
  frrNodeId = {.origin = ls_origin::ISIS_L1, .id = {.iso = {.level = 1}}};
  int ret = SysIdToBuffer(frrNodeId.id.iso.sys_id, nodeId.iso_sys_id.c_str());
  ASSERT_EQ(ISIS_SYS_ID_LEN, ret)
      << "[sys_id]: node ID is not a valid ISO system identifier.";
}

void LinkStateTest::AttributesToFrr(const LinkStateAttributes& attr,
                                    const ls_node_id& adv,
                                    ls_attributes*& frrAttr) const {
  in6_addr local{};
  int ret = inet_pton(AF_INET6, attr.local.c_str(), (void*)&local);

  ASSERT_EQ(1, ret) << "[ipv6]: address is not a valid IPv6 address.";
  frrAttr = ls_attributes_new(adv, in_addr{}, local, 0);

  in6_addr remote{};
  ret = inet_pton(AF_INET6, attr.remote.c_str(), (void*)&remote);
  ASSERT_EQ(1, ret) << "[ipv6]: remote address is not a valid IPv6 address.";

  frrAttr->standard.remote6 = remote;
  SET_FLAG(frrAttr->flags, LS_ATTR_NEIGH_ADDR6);
}

void LinkStateTest::SendNodeMessage(const ls_node& node, BEvent event) const {
  stream* s = stream_new(ZEBRA_MAX_PACKET_SIZ);

  // from ls_format_msg (lib/link_state.c, lines 1771, 1794)
  stream_putc(s, static_cast<uint8_t>(event));
  stream_putc(s, LS_MSG_TYPE_NODE);

  // from ls_format_node (lib/link_state.c, lines 1532-1580)
  stream_put(s, &node.adv, sizeof(struct ls_node_id));
  stream_putw(s, node.flags);

  stream_put(s, &node.router_id6, IPV6_MAX_BYTELEN);

  bridge_send_message(s, zapi_opaque_registry::LINK_STATE_UPDATE);
  stream_free(s);
}

void LinkStateTest::SendAttributesMessage(const ls_attributes& attr,
                                          const ls_node_id& remoteNodeId,
                                          BEvent event, bool reverse) const {
  stream* s = stream_new(ZEBRA_MAX_PACKET_SIZ);

  stream_putc(s, static_cast<uint8_t>(event));
  stream_putc(s, LS_MSG_TYPE_ATTRIBUTES);

  if (reverse) {
    stream_put(s, (void*)&attr.adv, sizeof(ls_node_id));
    stream_put(s, (void*)&remoteNodeId, sizeof(ls_node_id));
  } else {
    stream_put(s, (void*)&remoteNodeId, sizeof(ls_node_id));
    stream_put(s, (void*)&attr.adv, sizeof(ls_node_id));
  }

  stream_putl(s, attr.flags);

  if (CHECK_FLAG(attr.flags, LS_ATTR_NAME)) {
    size_t len = strlen(attr.name);
    stream_putc(s, len + 1);
    stream_put(s, (void*)attr.name, len);
    stream_putc(s, '\0');
  }

  if (reverse) {
    stream_put(s, (void*)&attr.standard.remote6, IPV6_MAX_BYTELEN);
    stream_put(s, (void*)&attr.standard.local6, IPV6_MAX_BYTELEN);
  } else {
    stream_put(s, (void*)&attr.standard.local6, IPV6_MAX_BYTELEN);
    stream_put(s, (void*)&attr.standard.remote6, IPV6_MAX_BYTELEN);
  }

  bridge_send_message(s, zapi_opaque_registry::LINK_STATE_UPDATE);
  stream_free(s);
}

void LinkStateTest::SendLinkUpdateMessage(const BApiLinkStateUpdate& apiMessage,
                                          ls_attributes*& frrAttr) const {
  ASSERT_TRUE(std::holds_alternative<LinkStateAttributes>(apiMessage.data))
      << "[DataVar]: apiMessage.data is not type LinkStateAttributes.";

  const LinkStateAttributes attr =
      std::get<LinkStateAttributes>(apiMessage.data);

  // TODO model may have IS-IS level as a free variable to test for IS-IS
  // interoperability between level 1 and level 2 nodes (specifically 1/2
  // nodes); check this again in the future
  ls_node_id remote_node_id{};
  NodeIdToFrr(apiMessage.remote, remote_node_id);

  // TODO same as remote node - see above
  ls_node_id adv_node_id{};
  NodeIdToFrr(attr.adv, adv_node_id);

  AttributesToFrr(attr, adv_node_id, frrAttr);

  struct ls_node* remote_node =
      ls_node_new(remote_node_id, in_addr{}, frrAttr->standard.remote6);
  SendNodeMessage(*remote_node, apiMessage.event);

  struct ls_node* adv_node =
      ls_node_new(adv_node_id, in_addr{}, frrAttr->standard.local6);
  SendNodeMessage(*adv_node, apiMessage.event);

  SendAttributesMessage(*frrAttr, remote_node_id, apiMessage.event);  // forward
  SendAttributesMessage(*frrAttr, remote_node_id, apiMessage.event,
                        true);  // reverse

  ls_node_del(adv_node);
  ls_node_del(remote_node);
}

void LinkStateTest::VerifyNlri(const std::vector<Model::RibVar>& rib) const {
  for (const Model::RibVar& var : rib) {
    if (const BgpLsLinkNlri* entry = std::get_if<BgpLsLinkNlri>(&var)) {
      LinkState::LinkNlri nlri = static_cast<LinkState::LinkNlri>(*entry);
      LinkState::ParentNlri p{.type = LinkState::NlriType::LINK,
                              .data = {.link = nlri}};
      // WARNING: unsafe cast; this is being used to bypass include errors with
      // the FRR bgpd library
      bgp_ls_nlri* nlriCast = reinterpret_cast<bgp_ls_nlri*>(&p);
      ASSERT_TRUE(bridge_link_exists_nlri(nlriCast))
          << "[bgp_ls_nlri]: NLRI does not exist within the RIB.";
    } else if (const BgpLsPrefixNlri* entry =
                   std::get_if<BgpLsPrefixNlri>(&var)) {
      // TODO
    }
  }
}

TEST_P(LinkStateTest, ValidateEdgeUpdate) {
  // Arrange
  TestCase tc = GetParam();

  if (std::holds_alternative<LinkStateAttributes>(tc.api_param.data)) {
    ArrangeLinkTest(tc);
  } else if (std::holds_alternative<LinkStatePrefix>(tc.api_param.data)) {
    // TODO
  } else {
    FAIL() << "[DataVar]: unknown alternative found in tc.api_param.data.";
  }

  // Act
  ls_attributes* attr;
  SendLinkUpdateMessage(tc.api_param, attr);

  // Debug
  if (LinkStateTest::DebugMode) {
    struct sbuf sbuf;
    sbuf_init(&sbuf, NULL, 0);

    bridge_show_ted(&sbuf);
    bridge_show_table(&sbuf);
    std::cout << sbuf_buf(&sbuf) << std::endl;
    sbuf_free(&sbuf);
  }

  // Assert
  ASSERT_TRUE(bridge_edge_exists_ted(attr))
      << "[ls_attributes]: edge does not exist within TED.";
  VerifyNlri(tc.final_state.rib);

  // Clean
  ls_attributes_del(attr);
}

// supplies a custom ID generator based on the TestId field in JSON
INSTANTIATE_TEST_SUITE_P(
    CrossHairCoverageTestCases, LinkStateTest, ::testing::ValuesIn(testCases),
    [](const ::testing::TestParamInfo<LinkStateTest::ParamType>& info) {
      return std::to_string(info.param.test_id);
    });

}  // namespace Model
