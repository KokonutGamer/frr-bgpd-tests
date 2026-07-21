#ifndef TEST_BGP_LS_EDGE_UPDATE_H
#define TEST_BGP_LS_EDGE_UPDATE_H

#include <gtest/gtest.h>
#include <netinet/in.h>

#include "common_data.h"

/**
 * Hacky solution to compiling with C++ (keyword delete cannot be used as a
 * variable name); see https://stackoverflow.com/a/25647229
 */
#define delete to_delete
#include "lib/link_state.h"
#undef delete

namespace Model {

class EdgeTest : public testing::TestWithParam<TestCase> {
 protected:
  /**
   * TODO document how set up works; this should include liveliness test, ensure
   * LS TED and RIB are empty
   */
  virtual void SetUp() override;

  /**
   * @brief Clears the BGP instance's link-state TED.
   *
   * Most of the work is done inside `bridge_clear_bgp_ls_ted()` within
   * frr_bridge.h. This function simply acts as a C++ wrapper around the
   * bridge's C implementation.
   */
  virtual void TearDown() override;

  /**
   * TODO document conversion function
   */
  void NodeIdToFrr(const LinkStateNodeId& nodeId, ls_node_id& frrNodeId) const;

  /**
   * TODO document conversion function
   */
  void AttributesToFrr(const LinkStateAttributes& attr, const ls_node_id& adv,
                       ls_attributes*& frrAttr) const;

  /**
   * TODO document sending function
   */
  void SendNodeMessage(const ls_node& node, BEvent event) const;

  /**
   * TODO document sending function
   */
  void SendAttributesMessage(const ls_attributes& attr,
                             const ls_node_id& remoteNodeId,
                             BEvent event, bool reverse = false) const;

  /**
   * TODO document sending function
   *
   * Note that this will do more than just sending
   */
  void SendUpdateMessage(const BApiLinkStateUpdate& apiMessage,
                         ls_attributes*& attr) const;
};
}  // namespace Model

#endif  // TEST_BGP_LS_EDGE_UPDATE_H
