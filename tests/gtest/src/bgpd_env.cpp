#include "bgpd_env.h"

extern "C" {
#include "frr_bridge.h"
}

namespace Model {

BgpdEnvironment::BgpdEnvironment() { bridge_init_bgp(); }

BgpdEnvironment::~BgpdEnvironment() { bridge_clean_bgp(); }

void BgpdEnvironment::SetUp() {
  // TODO perhaps do some checks on bgpd before testing
  // think of a "liveness" check - to see if bgpd is working

  // we can also set some configurations here; ensure that bgpd is self-
  // contained, not listening through other ports
}

void BgpdEnvironment::TearDown() {
  // TODO check bgpd
  // perhaps have each test clean up the LS TED and RIB
}

}  // namespace Model