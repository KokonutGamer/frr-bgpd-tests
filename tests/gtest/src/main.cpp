#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "bgpd_env.h"
#include "common_data.h"

/**
 * Entrypoint for Google Test execution. Before initializing the testing
 * framework, the JSON containing the model's output test cases is parsed and
 * saved into a global variable within the Model namespace. bgpd is initialized
 * inside the `BgpdEnvironment` class in which all tests are run.
 */
int main(int argc, char** argv) {
  std::filesystem::path jsonPath = std::filesystem::canonical(MODEL_JSON_PATH);

  if (!std::filesystem::exists(jsonPath)) {
    std::cerr << MODEL_JSON_PATH << " does not exist." << std::endl;
    return EXIT_FAILURE;
  }

  std::ifstream raw(jsonPath);

  nlohmann::json data = nlohmann::json::parse(raw);
  Model::testCases = data.get<std::vector<Model::TestCase>>();

  ::testing::InitGoogleTest(&argc, argv);

  ::testing::Environment* const env =
      ::testing::AddGlobalTestEnvironment(new Model::BgpdEnvironment);
  return RUN_ALL_TESTS();
}
