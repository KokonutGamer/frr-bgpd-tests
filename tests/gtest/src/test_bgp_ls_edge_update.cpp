#include <gtest/gtest.h>

#include "test_bgp_ls_linkstate_update.h"

/**
 * Hacky solution to compiling with C++ (keyword delete cannot be used as a
 * variable name); see https://stackoverflow.com/a/25647229
 */
#define delete to_delete
#include "lib/link_state.h"
#undef delete

namespace Model {

class EdgeTest : public LinkStateTest<LinkStateAttributes, ls_attributes> {};

TEST_P(EdgeTest, ValidateEdgeUpdate) {
  // Arrange
  TestCase tc = GetParam();

  if (IsSysIdUnspecified(tc.api_param.data.adv_node.iso_sys_id.c_str()) ||
      IsSysIdUnspecified(tc.api_param.data.remote_node.iso_sys_id.c_str()) ||
      IsIpv6Unspecified(tc.api_param.data.local.c_str()) ||
      IsIpv6Unspecified(tc.api_param.data.remote.c_str())) {
    GTEST_SKIP() << "[ls_attr]: test " << tc.test_id
                 << " provides no meaningful input.";
  }

  // Note that for arrange, we also want to place TED entries before the one
  // we actually want to test
  ArrangeInitialState(tc.initial_state.rib);
  VerifyNlri(tc.initial_state.rib);

  // Act
  SendUpdateMessage(tc.api_param);

  // Debug
  if (TestConfig::DebugMode) {
    DebugBgpd();
  }

  // Assert
  VerifyNlri(tc.final_state.rib);
}

// supplies a custom ID generator based on the TestId field in JSON
INSTANTIATE_TEST_SUITE_P(
    CrossHairCoverageTestCases, EdgeTest, ::testing::ValuesIn(linkTestCases),
    [](const ::testing::TestParamInfo<EdgeTest::ParamType>& info) {
      return std::to_string(info.param.test_id);
    });

}  // namespace Model
