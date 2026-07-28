#include "bgpd_env.h"

extern "C" {
#include "frr_bridge.h"
}

namespace Model {

BgpdEnvironment::BgpdEnvironment() { bridge_init_bgp(); }

BgpdEnvironment::~BgpdEnvironment() { bridge_clean_bgp(); }

void BgpdEnvironment::SetUp() {
  ASSERT_TRUE(bridge_check_bgpd_running())
      << "[Environment SetUp]: bgpd is not running.";

  // we can also set some configurations here; ensure that bgpd is self-
  // contained, not listening through other ports
}

}  // namespace Model
