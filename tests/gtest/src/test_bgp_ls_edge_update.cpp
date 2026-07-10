#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

TEST(HelloTest, BasicAssertions) {
  EXPECT_STRNE("hello", "world");
  EXPECT_EQ(7 * 6, 42);
}
