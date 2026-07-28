#ifndef BGP_ENV_H
#define BGP_ENV_H

#include <gtest/gtest.h>

namespace Model {
class BgpdEnvironment : public ::testing::Environment {
 public:
  /**
   * @brief Initializes FRR's BGP daemon, bgpd, in a static section of memory.
   *
   * Most of the work is done inside `bridge_init_bgp()` within
   * frr_bridge.h. The constructor simply acts as a C++ wrapper around the
   * bridge's C implementation.
   */
  BgpdEnvironment();

  /**
   * @brief Cleans FRR's BGP daemon, bgpd, by freeing any associated blocks.
   *
   * Similar to the constructor, most of the work is done inside
   * `bridge_clean_bgp()` within frr_bridge.h. Note that because most of
   * the memory allocated for bgpd is kept in static memory, not all blocks are
   * freed by this destructor. This is because pointers to dynamically allocated
   * blocks will be kept even during OS cleanup of the process.
   */
  virtual ~BgpdEnvironment() override;

  /**
   * @brief Checks that FRR's BGP daemon is actively running.
   *
   * The actual boolean assertion is implemented inside
   * `bridge_check_bgpd_running`. Additional configuration parameters may be set
   * inside this function in the future.
   */
  virtual void SetUp() override;
};
}  // namespace Model

#endif  // BGP_ENV_H
