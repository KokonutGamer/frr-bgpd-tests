#include "bgpd_env.h"

extern "C" {
#include "frr_bridge.h"
}

namespace Model {

BgpdEnvironment::BgpdEnvironment() { bridge_init_bgp(); }

BgpdEnvironment::~BgpdEnvironment() { bridge_clean_bgp(); }

void BgpdEnvironment::SetUp() {
  // TODO
}

}  // namespace Model
