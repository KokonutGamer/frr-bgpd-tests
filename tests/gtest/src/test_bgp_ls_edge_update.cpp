#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "common_data.h"

TEST(HelloTest, BasicAssertions) {
  EXPECT_STRNE("hello", "world");
  EXPECT_EQ(7 * 6, 42);
}

namespace Model {

class EdgeTest : public testing::TestWithParam<TestCase> {
 protected:
  void SetUp() override {
    // TODO perhaps do some checks on bgpd before testing
    // think of a "liveness" check - to see if bgpd is working

    // in addition, for each value-paramterized test, we should ensure the
    // LS TED and RIB is empty
  }

  void TearDown() override {
    // TODO check bgpd
    // perhaps have each test clean up the LS TED and RIB
  }
};

TEST_P(EdgeTest, ValidateEdgeUpdate) {
  // Arrange
  TestCase tc = GetParam();

  // Note that for arrange, we also want to place TED entries before the one
  // we actually want to test

  // Act
  // TODO bgpd functions

  // Assert
  // TODO act first; this assertion will check if the TED has two-way direction
  // installed, as well as the RIB
  ASSERT_NE(tc.test_id, 0);
}

INSTANTIATE_TEST_SUITE_P(CrossHairCoverageTestCases, EdgeTest,
                         ::testing::ValuesIn(testCases));

}  // namespace Model