#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "common_data.h"

TEST(HelloTest, BasicAssertions) {
  EXPECT_STRNE("hello", "world");
  EXPECT_EQ(7 * 6, 42);
}

namespace Model {

class EdgeTest : public testing::Test {
  protected:
    EdgeTest() {
      // set up bgpd; initialize using required functions
    }

    ~EdgeTest() override {
      // clean up bgpd (make sure it doesn't throw an exception)
    }

    void SetUp() override {
      
    }
};

}  // namespace Model