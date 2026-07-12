#ifndef BGP_ENV_H
#define BGP_ENV_H

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <vector>

#include "common_data.h"

namespace nm = nlohmann;

namespace Model {
class BgpdEnvironment : public ::testing::Environment {
 public:
  /**
   * TODO document
   */
  BgpdEnvironment();

  /**
   * TODO document
   */
  ~BgpdEnvironment() override;

  /**
   * TODO document
   */
  void SetUp() override;

  /**
   * TODO document
   */
  void TearDown() override;

 private:
  /**
   * TODO document
   */
  int currTestId;

  /**
   * TODO document
   */
  std::vector<TestCase> testCases;
};
}  // namespace Model

#endif  // BGP_ENV_H