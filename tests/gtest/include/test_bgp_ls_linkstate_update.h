#ifndef TEST_BGP_LS_LINKSTATE_UPDATE_H
#define TEST_BGP_LS_LINKSTATE_UPDATE_H

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

class LinkStateTest : public testing::TestWithParam<TestCase> {
 public:
  /**
   * @brief Sets debugging on (true) or off (false).
   *
   * Debugging on prints BGP-LS's TED and RIB table to the console during Google
   * Test runs. Debugging off skips these lines.
   */
  static void SetDebugMode(bool debug);

 protected:
  /**
   * @brief Checks that FRR's BGP daemon is actively running and the BGP-LS TED
   * is empty.
   *
   * Most of the work is done inside `bridge_check_bgpd_running` and
   * `bridge_clear_bgp_ls_ted` within frr_bridge.h. Fails the current test run
   * if either condition is note met.
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
   * @brief Checks
   */
  void ArrangeLinkTest(const TestCase& tc) const;

  /**
   * @brief Converts a `LinkStateNodeId` to FRR's `ls_node_id`.
   *
   * Defaults to `ISIS_L1` for node origin and `.iso.level`. Checks the passed
   * `nodeId` for a valid ISO sys ID; fails the current test run if not valid.
   * Otherwise, populates the `.iso.sys_id` field within `frrNodeId`.
   *
   * @param nodeId          Link-state node ID provided by the current
   *                            `TestCase`.
   * @param frrNodeId       FRR link-state node ID to be populated with the ISO
   *                            sys ID from `nodeId`.
   */
  void NodeIdToFrr(const LinkStateNodeId& nodeId, ls_node_id& frrNodeId) const;

  /**
   * @brief Converts `LinkStateAttributes` to FRR's `ls_attributes`.
   *
   * Dynamically allocates `ls_attributes` at the address of `frrAttr`. This
   * requires that the local address in `attr` is not unspecified; otherwise,
   * FRR assigns it a null pointer. Callers are expected to manage the memory
   * allocated by this function.
   *
   * Checks both the local and remote addresses in `attr` for valid IPv6
   * addresses; fails the current test run if at least one of them is not valid.
   * Otherwise, populates the `frrAttr` and sets its flags.
   *
   * @param attr        Link-state attributes provided by the current
   *                        `TestCase`.
   * @param adv         FRR link-state node ID to be assigned as the advertising
   *                        node.
   * @param frrAttr     FRR link-state attributes to be populated with
   *                        corresponding fields in `attr`.
   */
  void AttributesToFrr(const LinkStateAttributes& attr, const ls_node_id& adv,
                       ls_attributes*& frrAttr) const;

  /**
   * @brief Sends a `ls_node` with type `event` to the BGP daemon.
   *
   * Uses FRR's `stream` data structure to deliver the `node` information.
   *
   * @param node      FRR link-state node to send to the BGP daemon.
   * @param event     Link-state message event type.
   */
  void SendNodeMessage(const ls_node& node, BEvent event) const;

  /**
   * @brief Sends `ls_attributes` with type `event` to the BGP daemon.
   *
   * Uses FRR's `stream` data structure to deliver the `attr` information. Note
   * that `ls_attributes` messages include a field for storing the remote node
   * as a `ls_node_id`. This function includes an option to send `attr` in
   * reverse (switching the advertising/local node with the remote node).
   *
   * @param attr            FRR link-state attributes to send to the BGP daemon.
   * @param remoteNodeId    FRR link-state node ID to be appended to the message
   *                            as the remote node.
   * @param event           Link-state message event type.
   * @param reverse         If true, sends `attr` in reverse; otherwise, sends
   *                            `attr` as a forward edge. Defaults to false.
   */
  void SendAttributesMessage(const ls_attributes& attr,
                             const ls_node_id& remoteNodeId, BEvent event,
                             bool reverse = false) const;

  /**
   * @brief Sends messages to the BGP daemon for edge updates.
   *
   * Internally calls `SendNodeMessage` for both the local and remote endpoints
   * of an edge and `SendAttributesMessage` for the forward and reverse
   * directions of an edge. Delegates FRR primitives construction and allocation
   * to `NodeIdToFRR` and `AttributesToFrr`.
   *
   * @param apiMessage      `UPDATE` message specified by the current
   *                            `TestCase`.
   * @param frrAttr         FRR link-state attributes to allocate. Managed by
   *                            the caller of this function.
   */
  void SendLinkUpdateMessage(const BApiLinkStateUpdate& apiMessage,
                             ls_attributes*& frrAttr) const;

  /**
   * @brief Verifies the entirety of the model's RIB exists within the current
   * BGP instance.
   *
   * Iterates over each `BgpLsLinkNlri` and confirms their existence within
   * BGP-LS's RIB table. This function uses an unsafe cast, `reinterpret_cast`,
   * to provide the C API the correct data structure. Fails the current test run
   * if at least one entry within `rib` is missing.
   *
   * @param rib     Collection of `BgpLsLinkNlri` containing NLRI to check the
   *                    RIB with.
   */
  void VerifyNlri(const std::vector<Model::RibVar>& rib) const;

  static inline bool DebugMode = false;
};
}  // namespace Model

#endif  // TEST_BGP_LS_LINKSTATE_UPDATE_H
