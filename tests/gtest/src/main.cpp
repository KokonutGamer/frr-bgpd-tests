#include <gtest/gtest.h>

#include <fstream>

#include "bgpd_env.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  ::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(
      new Model::BgpdEnvironment);
  return RUN_ALL_TESTS();
}
