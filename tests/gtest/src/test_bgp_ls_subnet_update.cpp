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

class SubnetTest : public LinkStateTest<LinkStatePrefix, ls_prefix> {};

TEST_P(SubnetTest, ValidateSubnetUpdate) {
  // Arrange
  TestCase tc = GetParam();

  std::size_t delim = tc.api_param.data.prefix.find("/");
  std::string addr = tc.api_param.data.prefix.substr(0, delim);

  if (IsSysIdUnspecified(tc.api_param.data.adv.iso_sys_id.c_str()) ||
      IsIpv6Unspecified(addr.c_str())) {
    GTEST_SKIP() << "[ls_pref]: test " << tc.test_id
                 << " provides no meaningful input.";
  }

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
    CrossHairCoverageTestCases, SubnetTest,
    ::testing::ValuesIn(prefixTestCases),
    [](const ::testing::TestParamInfo<SubnetTest::ParamType>& info) {
      return std::to_string(info.param.test_id);
    });

}  // namespace Model
