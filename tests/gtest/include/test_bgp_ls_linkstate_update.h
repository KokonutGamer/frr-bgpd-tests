#ifndef TEST_BGP_LS_LINKSTATE_UPDATE_H
#define TEST_BGP_LS_LINKSTATE_UPDATE_H

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string_view>
#include <variant>

#include "common_data.h"
#include "frr_bridge.h"
#include "lib/stream.h"
#include "lib/zclient.h"
#include "linkstate_data.h"
#include "sbuf.h"
#include "utils.hpp"

/**
 * Hacky solution to compiling with C++ (keyword delete cannot be used as a
 * variable name); see https://stackoverflow.com/a/25647229
 */
#define delete to_delete
#include "lib/link_state.h"
#undef delete

namespace Model {

template <AttrPref T, typename U>
requires std::same_as<U, ls_attributes> || std::same_as<U, ls_prefix>
class LinkStateTest : public testing::TestWithParam<TestCase<T>> {
 protected:
  /**
   * @brief Checks that FRR's BGP daemon is actively running and the BGP-LS TED
   * is empty.
   *
   * Most of the work is done inside `bridge_check_bgpd_running` and
   * `bridge_clear_bgp_ls_ted` within frr_bridge.h. Fails the current test run
   * if either condition is not met.
   */
  virtual inline void SetUp() override final {
    ASSERT_TRUE(bridge_check_bgpd_running())
        << "[Fixture SetUp]: bgpd is not running.";
    ASSERT_TRUE(bridge_check_ls_clear())
        << "[Fixture SetUp]: BGP-LS TED is not empty.";
  }

  /**
   * @brief Clears the BGP instance's link-state TED.
   *
   * Most of the work is done inside `bridge_clear_bgp_ls_ted()` within
   * frr_bridge.h. This function simply acts as a C++ wrapper around the
   * bridge's C implementation.
   */
  virtual inline void TearDown() override final { bridge_clear_bgp_ls_ted(); }

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
  inline void NodeIdToFrr(const LinkStateNodeId& nodeId,
                          ls_node_id& frrNodeId) const {
    // for now, we default to IS-IS level 1
    frrNodeId = {.origin = ls_origin::ISIS_L1, .id = {.iso = {.level = 1}}};
    int ret = SysIdToBuffer(frrNodeId.id.iso.sys_id, nodeId.iso_sys_id.c_str());
    ASSERT_EQ(ISIS_SYS_ID_LEN, ret)
        << "[sys_id]: node ID is not a valid ISO system identifier.";
  }

  /**
   * @brief Converts `LinkStatePrefix` to FRR's `ls_prefix`.
   *
   * Dynamically allocates `ls_prefix` at the address of `frrPref`. This
   * requires that the advertised prefix in `pref` is not unspecified;
   * otherwise, FRR assigns it a null pointer. Callers are expected to manage
   * the memory allocated by this function.
   *
   * Checks the advertised prefix in `pref` for a valid IPv6 prefix; fails the
   * current test run if not valid. Otherwise, populates the `frrPref`.
   *
   * @param pref        Link-state prefix provided by the current `TestCase`.
   * @param adv         FRR link-state node ID to be assigned as the advertising
   *                        node.
   * @param frrPref     FRR link-state prefix to be populated with corresponding
   *                        to fields in `pref`.
   */
  inline void PrefixToFrr(const LinkStatePrefix& pref, const ls_node_id& adv,
                          ls_prefix*& frrPref) const {
    prefix p{.family = AF_INET6, .prefixlen = IPV6_MAX_BITLEN};

    std::size_t delim = pref.prefix.find("/");
    ASSERT_NE(delim, std::string::npos)
        << "[ipv6]: prefix does not contain the length delimiter.";

    std::string addr = pref.prefix.substr(0, delim);
    int ret = inet_pton(AF_INET6, addr.c_str(), (void*)&p.u.prefix6);

    ASSERT_EQ(1, ret) << "[ipv6]: prefix is not a valid IPv6 prefix.";
    frrPref = ls_prefix_new(adv, &p);
  }

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
  inline void AttributesToFrr(const LinkStateAttributes& attr,
                              const ls_node_id& adv,
                              ls_attributes*& frrAttr) const {
    in6_addr local{};
    int ret = inet_pton(AF_INET6, attr.local.c_str(), (void*)&local);

    ASSERT_EQ(1, ret) << "[ipv6]: address is not a valid IPv6 address.";
    frrAttr = ls_attributes_new(adv, in_addr{}, local, 0);

    in6_addr remote{};
    ret = inet_pton(AF_INET6, attr.remote.c_str(), (void*)&remote);
    ASSERT_EQ(1, ret) << "[ipv6]: remote address is not a valid IPv6 address.";

    frrAttr->standard.remote6 = remote;
    SET_FLAG(frrAttr->flags, LS_ATTR_NEIGH_ADDR6);
  }

  /**
   * @brief Sends a `ls_node` with type `event` to the BGP daemon.
   *
   * Uses FRR's `stream` data structure to deliver the `node` information.
   *
   * @param node        FRR link-state node to send to the BGP daemon.
   * @param event       Link-state message event type.
   */
  inline void SendNodeMessage(const ls_node& node, BEvent event) const {
    stream* s = stream_new(ZEBRA_MAX_PACKET_SIZ);

    // from ls_format_msg (lib/link_state.c, lines 1771, 1794)
    stream_putc(s, static_cast<uint8_t>(event));
    stream_putc(s, LS_MSG_TYPE_NODE);

    // from ls_format_node (lib/link_state.c, lines 1532-1580)
    stream_put(s, &node.adv, sizeof(struct ls_node_id));
    stream_putw(s, node.flags);

    stream_put(s, &node.router_id6, IPV6_MAX_BYTELEN);

    bridge_send_message(s, zapi_opaque_registry::LINK_STATE_UPDATE);
    stream_free(s);
  }

  /**
   * @brief Sends a `ls_prefix` with type `event` to the BGP daemon.
   *
   * Uses FRR's `stream` data structure to deliver the `pref` information.
   *
   * @param pref        FRR link-state prefix to send to the BGP daemon.
   * @param event       Link-state message event type.
   */
  inline void SendPrefixMessage(const ls_prefix& pref, BEvent event) const {
    stream* s = stream_new(ZEBRA_MAX_PACKET_SIZ);

    // from ls_format_msg
    stream_putc(s, static_cast<uint8_t>(event));
    stream_putc(s, LS_MSG_TYPE_PREFIX);

    stream_put(s, &pref.adv, sizeof(struct ls_node_id));
    stream_putw(s, pref.flags);
    stream_putc(s, pref.pref.family);
    stream_putw(s, pref.pref.prefixlen);

    // note that a pointer to the union's prefix is the exact same as any of its
    // other types
    size_t len = prefix_blen(&pref.pref);
    stream_put(s, &pref.pref.u.prefix, len);

    bridge_send_message(s, zapi_opaque_registry::LINK_STATE_UPDATE);
    stream_free(s);
  }

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
  inline void SendAttributesMessage(const ls_attributes& attr,
                                    const ls_node_id& remoteNodeId,
                                    BEvent event, bool reverse = false) const {
    stream* s = stream_new(ZEBRA_MAX_PACKET_SIZ);

    stream_putc(s, static_cast<uint8_t>(event));
    stream_putc(s, LS_MSG_TYPE_ATTRIBUTES);

    if (reverse) {
      stream_put(s, (void*)&attr.adv, sizeof(ls_node_id));
      stream_put(s, (void*)&remoteNodeId, sizeof(ls_node_id));
    } else {
      stream_put(s, (void*)&remoteNodeId, sizeof(ls_node_id));
      stream_put(s, (void*)&attr.adv, sizeof(ls_node_id));
    }

    stream_putl(s, attr.flags);

    if (CHECK_FLAG(attr.flags, LS_ATTR_NAME)) {
      size_t len = strlen(attr.name);
      stream_putc(s, len + 1);
      stream_put(s, (void*)attr.name, len);
      stream_putc(s, '\0');
    }

    if (reverse) {
      stream_put(s, (void*)&attr.standard.remote6, IPV6_MAX_BYTELEN);
      stream_put(s, (void*)&attr.standard.local6, IPV6_MAX_BYTELEN);
    } else {
      stream_put(s, (void*)&attr.standard.local6, IPV6_MAX_BYTELEN);
      stream_put(s, (void*)&attr.standard.remote6, IPV6_MAX_BYTELEN);
    }

    bridge_send_message(s, zapi_opaque_registry::LINK_STATE_UPDATE);
    stream_free(s);
  }

  /**
   * @brief Arranges the initial state of the BGP environment's RIB to match the
   * RIB of the current `TestCase`.
   *
   * Iterates over each `BgpLsLinkNlri` and `BgpLsPrefixNlri` and sends an
   * update message consisting of each link-state element to the running BGP
   * instance. Ignores elements of unknown types.
   *
   * @param rib     Collection of `BgpLsLinkNlri` and `BgpLsPrefixNlri`
   *                    representing the initial state of the RIB.
   */
  inline void ArrangeInitialState(const std::vector<RibVar>& rib) const {
    for (const RibVar& var : rib) {
      if (const BgpLsLinkNlri* link = std::get_if<BgpLsLinkNlri>(&var)) {
        auto message =
            static_cast<BApiLinkStateUpdate<LinkStateAttributes>>(*link);
        SendUpdateMessage(message);
      } else if (const BgpLsPrefixNlri* pref =
                     std::get_if<BgpLsPrefixNlri>(&var)) {
        auto message = static_cast<BApiLinkStateUpdate<LinkStatePrefix>>(*pref);
        SendUpdateMessage(message);
      }
      // ignore other alternatives
    }
  }

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
   */
  inline void SendUpdateMessage(
      const BApiLinkStateUpdate<LinkStateAttributes>& apiMessage) const {
    ls_attributes* attr;

    // TODO model may have IS-IS level as a free variable to test for IS-IS
    // interoperability between level 1 and level 2 nodes (specifically 1/2
    // nodes); check this again in the future
    ls_node_id remote_node_id{};
    NodeIdToFrr(apiMessage.data.remote_node, remote_node_id);

    // TODO same as remote node - see above
    ls_node_id adv_node_id{};
    NodeIdToFrr(apiMessage.data.adv_node, adv_node_id);

    AttributesToFrr(apiMessage.data, adv_node_id, attr);

    // send messages to the BGP instance

    if (apiMessage.event == BEvent::DELETE) {
      SendAttributesMessage(*attr, remote_node_id, apiMessage.event);
      SendAttributesMessage(*attr, remote_node_id, apiMessage.event, true);
    } else {
      struct ls_node* remote_node =
          ls_node_new(remote_node_id, in_addr{}, attr->standard.remote6);
      SendNodeMessage(*remote_node, apiMessage.event);

      struct ls_node* adv_node =
          ls_node_new(adv_node_id, in_addr{}, attr->standard.local6);
      SendNodeMessage(*adv_node, apiMessage.event);

      SendAttributesMessage(*attr, remote_node_id, apiMessage.event);
      SendAttributesMessage(*attr, remote_node_id, apiMessage.event, true);

      ls_node_del(adv_node);
      ls_node_del(remote_node);
    }

    ls_attributes_del(attr);
  }

  /**
   * @brief Sends messages to the BGP daemon for subnet updates.
   *
   * Internally calls `SendNodeMessage` for both the advertising node of a
   * subnet and `SendPrefixMessage` for the advertised prefix. Delegates FRR
   * primitives construction and allocation to `NodeIdToFRR` and
   * `PrefixToFrr`.
   *
   * @param apiMessage      `UPDATE` message specified by the current
   *                            `TestCase`.
   */
  inline void SendUpdateMessage(
      const BApiLinkStateUpdate<LinkStatePrefix>& apiMessage) const {
    ls_prefix* pref;

    ls_node_id adv_node_id{};
    NodeIdToFrr(apiMessage.data.adv, adv_node_id);

    PrefixToFrr(apiMessage.data, adv_node_id, pref);

    if (apiMessage.event == BEvent::DELETE) {
      SendPrefixMessage(*pref, apiMessage.event);
    } else {
      struct ls_node* adv_node =
          ls_node_new(adv_node_id, in_addr{}, pref->pref.u.prefix6);
      SendNodeMessage(*adv_node, apiMessage.event);

      SendPrefixMessage(*pref, apiMessage.event);

      ls_node_del(adv_node);
    }

    ls_prefix_del(pref);
  }

  /**
   * @brief Verifies the entirety of the model's RIB exists within the current
   * BGP instance.
   *
   * Iterates over each `BgpLsLinkNlri` and `BgpLsPrefixNlri` and confirms their
   * existence within BGP-LS's RIB table. This function uses an unsafe cast,
   * `reinterpret_cast`, to provide the C API the correct data structure. Fails
   * the current test run if at least one entry within `rib` is missing.
   *
   * @param rib     Collection of `BgpLsLinkNlri` and `BgpLsPrefixNlri`
   *                    containing NLRI to check the RIB with.
   */
  inline void VerifyNlri(const std::vector<Model::RibVar>& rib) const {
    for (const Model::RibVar& var : rib) {
      if (const BgpLsLinkNlri* entry = std::get_if<BgpLsLinkNlri>(&var)) {
        LinkState::LinkNlri nlri = static_cast<LinkState::LinkNlri>(*entry);
        LinkState::ParentNlri p{.type = LinkState::NlriType::LINK,
                                .data = {.link = nlri}};
        // WARNING: unsafe cast; this is being used to bypass include errors
        // with the FRR bgpd library
        bgp_ls_nlri* nlriCast = reinterpret_cast<bgp_ls_nlri*>(&p);
        ASSERT_TRUE(bridge_nlri_exists(nlriCast))
            << "[bgp_ls_nlri]: link NLRI does not exist within the RIB.";
      } else if (const BgpLsPrefixNlri* entry =
                     std::get_if<BgpLsPrefixNlri>(&var)) {
        LinkState::PrefixNlri nlri = static_cast<LinkState::PrefixNlri>(*entry);
        LinkState::ParentNlri p{.type = LinkState::NlriType::IPV6_PREFIX,
                                .data = {.prefix = nlri}};

        // WARNING: unsafe cast
        bgp_ls_nlri* nlriCast = reinterpret_cast<bgp_ls_nlri*>(&p);
        ASSERT_TRUE(bridge_nlri_exists(nlriCast))
            << "[bgp_ls_nlri]: prefix NLRI does not exist within the RIB.";
      }
    }
  }

  /**
   * @brief Prints the BGP instance's link-state TED and RIB table to standard
   * out.
   */
  inline void DebugBgpd() const {
    struct sbuf sbuf;
    sbuf_init(&sbuf, NULL, 0);

    bridge_show_ted(&sbuf);
    bridge_show_table(&sbuf);
    std::cout << sbuf_buf(&sbuf) << std::endl;
    sbuf_free(&sbuf);
  }
};

}  // namespace Model

#endif  // TEST_BGP_LS_LINKSTATE_UPDATE_H
