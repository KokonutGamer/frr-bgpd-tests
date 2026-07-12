#include "bgpd_env.h"

extern "C" {
#include "frr_bridge.h"
}

namespace Model {

BgpdEnvironment::BgpdEnvironment() { bridge_init_bgp(); }

BgpdEnvironment::~BgpdEnvironment() { bgp_exit(0); }

void BgpdEnvironment::SetUp() {
  // TODO ifstream check
}

void BgpdEnvironment::TearDown() {
  // TODO do some last assertions (possibly)
}

}  // namespace Model