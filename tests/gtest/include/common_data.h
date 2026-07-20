#ifndef COMMON_DATA_H
#define COMMON_DATA_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using sys_id_t = std::string;
using prefix_t = std::string;
using addr_t = std::string;

namespace Model {

/**
 * @brief Link-state node according to the BGP-LS protocol.
 *
 * This data structure is a trimmed version of FRR's node descriptor. The
 * original node descriptor data structure fully complies with RFC 9552.
 */
struct BgpLsNode {
  uint32_t asn;
  sys_id_t igp_router_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsNode, asn, igp_router_id)

/**
 * @brief Link-state link according to the BGP-LS protocol.
 *
 * This data structure is a trimmed version of FRR's link descriptor. Most
 * notably, it only includes IPv6 address fields rather than having IDs and IPv4
 * addresses alongside it. The original link descriptor data structure fully
 * complies with RFC 9552.
 */
struct BgpLsLink {
  addr_t interface;
  addr_t neighbor;
  uint32_t remote_asn;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsLink, interface, neighbor, remote_asn)

/**
 * @brief Link-state link Network Layer Reachability Information (NLRI).
 *
 * This data structure is a trimmed version of FRR's link NLRI. Because this
 * test suite is limited to IS-IS in scope, specifying `protocol_id` would be
 * redundant to this implementation. The original link NLRI data structure fully
 * complies with RFC 9552.
 */
struct BgpLsLinkNlri {
  BgpLsNode source;
  BgpLsNode destination;
  BgpLsLink link;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpLsLinkNlri, source, destination, link)

/**
 * @brief Unique identifier for a node in a network.
 *
 * This data structure is a trimmed version of FRR's node ID. Because this test
 * suite is limited to IS-IS in scope, specifying `origin` would be redundant to
 * this implementation. The original node ID data structure is specific to FRR's
 * implementation of an IGP-agnostic link-state representation of the network.
 */
struct LinkStateNodeId {
  sys_id_t iso_sys_id;
  uint8_t level;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkStateNodeId, iso_sys_id, level)

/**
 * @brief Unidirectional path between two nodes in a network.
 *
 * This data structure is a trimmed version of FRR's edge. Most notably, FRR
 * uses a red-black tree internally to speed up edge lookups within the TED. The
 * original edge data structure is specific to FRR's implementation of an
 * IGP-agnostic link-state representation of the network.
 */
struct LinkStateEdge {
  uint32_t asn;
  LinkStateNodeId source_node;
  LinkStateNodeId destination_node;
  addr_t source;
  addr_t destination;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkStateEdge, asn, source_node,
                                   destination_node, source, destination)

/**
 * @brief Various metrics assigned to or collected from a link between two nodes
 * in a network.
 *
 * This data structure is a trimmed version of FRR's attributes. FRR keeps track
 * of 60+ fields; this test suite only uses a small subset of those fields for
 * validation, which includes the source and destination addresses. The original
 * attributes data structure is specific to FRR's implementation of an
 * IGP-agnostic link-state representation of the network.
 */
struct LinkStateAttributes {
  bool valid;
  LinkStateNodeId adv;
  std::string name;
  uint32_t metric;
  addr_t local;
  addr_t remote;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkStateAttributes, valid, adv, name,
                                   metric, local, remote)

/**
 * TODO use this in code - this struct doesn't seem to be used meaningfully at
 * all within the test suite; need to perform RIB validation/verification
 */
struct BgpRibEntry {
  prefix_t synth_prefix;
  BgpLsLinkNlri ls_nlri;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BgpRibEntry, synth_prefix, ls_nlri)

/**
 * @brief Link-state message event.
 *
 * This enum class corresponds directly to the Zebra API's Opaque link-state
 * message event macros. Currently, only five types, `UNDEF`, `SYNC`, `ADD`,
 * `UPDATE`, and `DELETE`, are implemented.
 */
enum class BEvent : uint8_t { UNDEF = 0, SYNC, ADD, UPDATE, DELETE };

/**
 * @brief Link-state update message.
 *
 * This data structure is a trimmed version of FRR's message. Because this test
 * suite is limited to edge updates, specifying `type` would be redundant to
 * this implementation. The original message data structure is specific the
 * Zebra API's Opaque message system.
 */
struct BApiLinkStateUpdate {
  BEvent event;
  LinkStateNodeId remote;
  LinkStateAttributes data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BApiLinkStateUpdate, event, remote, data)

/**
 * @brief Link-state of the BGP instance.
 *
 * This data structure encapsulates both BGP's Routing Information Base (RIB)
 * and BGP-LS's internal Traffic Engineering Database (TED); FRR's
 * implementation does not keep this information together in one place. However,
 * the BGP instance should be keeping both the RIB and the TED synced.
 */
struct BgpLsLinkState {
  uint32_t asn;
  std::vector<LinkStateEdge> ted;
  std::vector<BgpLsLinkNlri> rib;
};
inline void to_json(nlohmann::json& j, const BgpLsLinkState& ls) {
  j = nlohmann::json{
      {"asn", ls.asn}, {"linkstate_ted", ls.ted}, {"rib_nlri", ls.rib}};
}
inline void from_json(const nlohmann::json& j, BgpLsLinkState& ls) {
  j.at("asn").get_to(ls.asn);
  j.at("linkstate_ted").get_to(ls.ted);
  j.at("rib_nlri").get_to(ls.rib);
}

/**
 * @brief Model test case containing initial state, transition, and final state.
 */
struct TestCase {
  int test_id;
  std::string op;  // currently always "api_bgp_ls_edge_update"
  BgpLsLinkState initial_state;
  BApiLinkStateUpdate api_param;
  BgpLsLinkState final_state;
  int response;
};
inline void to_json(nlohmann::json& j, const TestCase& tc) {
  j = nlohmann::json{{"TestId", tc.test_id},
                     {"Op", tc.op},
                     {"InitialState", tc.initial_state},
                     {"ApiParam", tc.api_param},
                     {"FinalState", tc.final_state},
                     {"Response", tc.response}};
}
inline void from_json(const nlohmann::json& j, TestCase& tc) {
  j.at("TestId").get_to(tc.test_id);
  j.at("Op").get_to(tc.op);
  j.at("InitialState").get_to(tc.initial_state);
  j.at("ApiParam").get_to(tc.api_param);
  j.at("FinalState").get_to(tc.final_state);
  j.at("Response").get_to(tc.response);
}

/**
 * @brief `TestCase` objects used in the Google Test value-parameterized tests.
 */
inline std::vector<TestCase> testCases;

/**
 * @brief Prints information about `TestCase` instances.
 *
 * Google Test relies on this function to print information about test runs. If
 * this function isn't defined, Valgrind will complain about "uninitialized
 * values" and "conditional jumps relying on uninitialized values". Note that
 * `AbslStringify` is an alternative function that Google Test uses in the same
 * manner.
 */
inline void PrintTo(const TestCase& tc, std::ostream* os) {
  *os << "{ ID: " << tc.test_id << ", Op: " << tc.op << " }";
}

}  // namespace Model

#endif  // COMMON_DATA_H
