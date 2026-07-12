#include "bgpd_env.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

extern "C" {
#include "frr_bridge.h"
}

namespace Model {

BgpdEnvironment::BgpdEnvironment() { bridge_init_bgp(); }

BgpdEnvironment::~BgpdEnvironment() { bridge_clean_bgp(); }

void BgpdEnvironment::SetUp() {
  std::filesystem::path jsonPath = std::filesystem::canonical(MODEL_JSON_PATH);

  ASSERT_TRUE(std::filesystem::exists(jsonPath));

  std::ifstream raw(jsonPath);

  nlohmann::json data = nlohmann::json::parse(raw);
  this->testCases = data.get<std::vector<TestCase>>();
  this->currTestId = 1;
}

void BgpdEnvironment::TearDown() {
  // TODO check bgpd
}

}  // namespace Model