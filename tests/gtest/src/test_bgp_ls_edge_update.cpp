#include "test_bgp_ls_edge_update.h"

#include "common_data.h"

namespace Model {

ls_attributes* EdgeTest::SendUpdateMessage(
    const BApiLinkStateUpdate<LinkStateAttributes>& apiMessage) const {
  ls_attributes* attr;

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

  return attr;
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
  for (const TedVar& var : tc.initial_state.ted) {
    if (const LinkStateEdge* edge = std::get_if<LinkStateEdge>(&var)) {
      BApiLinkStateUpdate<LinkStateAttributes> msg =
          static_cast<BApiLinkStateUpdate<LinkStateAttributes>>(*edge);
      ls_attributes* tempAttr = SendUpdateMessage(msg);
      ls_attributes_del(tempAttr);
    } else if (const LinkStateSubnet* subnet =
                   std::get_if<LinkStateSubnet>(&var)) {
      // TODO
    }
    // ignore other alternatives
  }

  // Act
  ls_attributes* attr = SendUpdateMessage(tc.api_param);

  // Debug
  if (EdgeTest::DebugMode) {
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
    CrossHairCoverageTestCases, EdgeTest, ::testing::ValuesIn(linkTestCases),
    [](const ::testing::TestParamInfo<EdgeTest::ParamType>& info) {
      return std::to_string(info.param.test_id);
    });

}  // namespace Model
