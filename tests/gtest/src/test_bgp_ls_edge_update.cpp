#include "test_bgp_ls_edge_update.h"

#include <arpa/inet.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>

#include "frr_bridge.h"
#include "lib/stream.h"
#include "lib/zclient.h"
#include "sbuf.h"
#include "utils.hpp"

namespace Model {

void EdgeTest::SetUp() {
  // TODO perhaps do some checks on bgpd before testing
  // think of a "liveness" check - to see if bgpd is working

  // in addition, for each value-paramterized test, we should ensure the
  // LS TED and RIB is empty
}

void EdgeTest::TearDown() { bridge_clear_bgp_ls_ted(); }

void EdgeTest::NodeIdToFrr(const LinkStateNodeId& nodeId,
                           ls_node_id& frrNodeId) const {
  // for now, we default to IS-IS level 1
  frrNodeId = {.origin = ls_origin::ISIS_L1, .id = {.iso = {.level = 1}}};
  int ret = SysIdToBuffer(frrNodeId.id.iso.sys_id, nodeId.iso_sys_id.c_str());
  ASSERT_EQ(ISIS_SYS_ID_LEN, ret)
      << "[sys_id]: node ID must be a valid ISO system identifier.";
}

void EdgeTest::AttributesToFrr(const LinkStateAttributes& attr,
                               const ls_node_id& adv,
                               ls_attributes*& frrAttr) const {
  in6_addr local{};
  int ret = inet_pton(AF_INET6, attr.local.c_str(), (void*)&local);

  ASSERT_EQ(1, ret) << "[ipv6]: address must be a valid IPv6 address.";
  frrAttr = ls_attributes_new(adv, in_addr{}, local, 0);

  if (!attr.name.empty()) {
    strncpy(frrAttr->name, attr.name.c_str(), MAX_NAME_LENGTH);
    SET_FLAG(frrAttr->flags, LS_ATTR_NAME);
  }

  frrAttr->metric = attr.metric;
  SET_FLAG(frrAttr->flags, LS_ATTR_METRIC);

  in6_addr remote{};
  ret = inet_pton(AF_INET6, attr.remote.c_str(), (void*)&remote);
  ASSERT_EQ(1, ret) << "[ipv6]: remote address must be a valid IPv6 address.";

  frrAttr->standard.remote6 = remote;
  SET_FLAG(frrAttr->flags, LS_ATTR_NEIGH_ADDR6);
}

void EdgeTest::SendNodeMessage(const ls_node& node, BEvent event) const {
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

void EdgeTest::SendAttributesMessage(const ls_attributes& attr,
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

  stream_putl(s, attr.metric);

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

void EdgeTest::SendUpdateMessage(const BApiLinkStateUpdate& apiMessage,
                                 ls_attributes*& attr) const {
  // TODO model may have IS-IS level as a free variable to test for IS-IS
  // interoperability between level 1 and level 2 nodes (specifically 1/2
  // nodes); check this again in the future
  ls_node_id remote_node_id{};
  NodeIdToFrr(apiMessage.remote, remote_node_id);

  // TODO same as remote node - see above
  ls_node_id adv_node_id{};
  NodeIdToFrr(apiMessage.data.adv, adv_node_id);

  AttributesToFrr(apiMessage.data, adv_node_id, attr);

  struct ls_node* remote_node =
      ls_node_new(remote_node_id, in_addr{}, attr->standard.remote6);
  SendNodeMessage(*remote_node, apiMessage.event);

  struct ls_node* adv_node =
      ls_node_new(adv_node_id, in_addr{}, attr->standard.local6);
  SendNodeMessage(*adv_node, apiMessage.event);

  SendAttributesMessage(*attr, remote_node_id, apiMessage.event);  // forward
  SendAttributesMessage(*attr, remote_node_id, apiMessage.event,
                        true);  // reverse

  ls_node_del(adv_node);
  ls_node_del(remote_node);
}

TEST_P(EdgeTest, ValidateEdgeUpdate) {
  // Arrange
  TestCase tc = GetParam();

  if (IsSysIdUnspecified(tc.api_param.data.adv.iso_sys_id.c_str()) ||
      IsSysIdUnspecified(tc.api_param.remote.iso_sys_id.c_str()) ||
      IsIpv6Unspecified(tc.api_param.data.local.c_str()) ||
      IsIpv6Unspecified(tc.api_param.data.remote.c_str())) {
    GTEST_SKIP() << "[ls_attr]: test " << tc.test_id
                 << " provides no meaningful input.";
  }

  // Note that for arrange, we also want to place TED entries before the one
  // we actually want to test
  for (const auto& edge : tc.initial_state.ted) {
    ls_attributes* tempAttr;
    BApiLinkStateUpdate message = static_cast<BApiLinkStateUpdate>(edge);
    SendUpdateMessage(message, tempAttr);
    ls_attributes_del(tempAttr);
  }

  // Act
  ls_attributes* attr;
  SendUpdateMessage(tc.api_param, attr);

  // Debug
  struct sbuf sbuf;
  sbuf_init(&sbuf, NULL, 0);

  bridge_show_ted(&sbuf);
  std::cout << sbuf_buf(&sbuf) << std::endl;

  // Assert
  ASSERT_TRUE(bridge_edge_exists_ted(attr))
      << "[ls_attributes]: edge does not exist within TED.";

  // Clean
  sbuf_free(&sbuf);
  ls_attributes_del(attr);
}

// supplies a custom ID generator based on the TestId field in JSON
INSTANTIATE_TEST_SUITE_P(
    CrossHairCoverageTestCases, EdgeTest, ::testing::ValuesIn(testCases),
    [](const ::testing::TestParamInfo<EdgeTest::ParamType>& info) {
      return std::to_string(info.param.test_id);
    });

}  // namespace Model
