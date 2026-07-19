#include "test_bgp_ls_edge_update.h"

#include <arpa/inet.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>

/**
 * Hacky solution to compiling with C++ (keyword delete cannot be used as a
 * variable name); see https://stackoverflow.com/a/25647229
 */
#define delete to_delete
#include "lib/link_state.h"
#undef delete

#include "frr_bridge.h"
#include "lib/stream.h"
#include "lib/zclient.h"
#include "sbuf.h"

#define ISIS_SYS_ID_LEN 6

namespace Model {

void EdgeTest::SetUp() {
  // TODO perhaps do some checks on bgpd before testing
  // think of a "liveness" check - to see if bgpd is working

  // in addition, for each value-paramterized test, we should ensure the
  // LS TED and RIB is empty
}

void EdgeTest::TearDown() {
  // TODO check bgpd
  // perhaps have each test clean up the LS TED and RIB
  bridge_clear_bgp_ls_ted();
}

/**
 * TODO finish documentation
 *
 * Copied from source (I'd rather not statically link against libisis.a since
 * multiple different symbols require definition when doing so).
 */
int EdgeTest::SysIdToBuffer(uint8_t* buff, const char* dotted) const {
  // string length must  be 14 characters
  if (strlen(dotted) != 14) {
    return 0;
  }

  int len = 0;
  const char* pos = dotted;
  uint8_t number[3];
  number[2] = '\0';

  while (len < ISIS_SYS_ID_LEN) {
    if (*pos == '.') {
      // period not positioned correctly
      if (((pos - dotted) != 4) && ((pos - dotted) != 9)) {
        len = 0;
        break;
      }
      pos++;
      continue;
    }
    if ((isxdigit((unsigned char)*pos)) &&
        (isxdigit((unsigned char)*(pos + 1)))) {
      memcpy(number, pos, 2);
      pos += 2;
    } else {
      len = 0;
      break;
    }

    *(buff + len) = (char)strtol((char*)number, NULL, 16);
    len++;
  }

  return len;
}

void EdgeTest::GetRemoteNodeId(const TestCase& tc, ls_node_id& remote) const {
  remote = {.origin = ls_origin::ISIS_L1, .id = {.iso = {.level = 1}}};
  int ret = SysIdToBuffer(remote.id.iso.sys_id,
                          tc.api_param.remote.iso_sys_id.c_str());
  ASSERT_EQ(ISIS_SYS_ID_LEN, ret)
      << "[sys_id]: remote node ID must be a valid ISO system identifier.";
}

void EdgeTest::GetAdvNodeId(const TestCase& tc, ls_node_id& adv) const {
  adv = {.origin = ls_origin::ISIS_L1, .id = {.iso = {.level = 1}}};
  int ret = SysIdToBuffer(adv.id.iso.sys_id,
                          tc.api_param.data.adv.iso_sys_id.c_str());
  ASSERT_EQ(ISIS_SYS_ID_LEN, ret) << "[sys_id]: advertising node ID must be "
                                     "a valid ISO system identifier.";
}

void EdgeTest::FillSrcAttributes(const TestCase& tc, const ls_node_id& adv,
                                 in6_addr& local, ls_attributes* &attr) const {
  int ret = inet_pton(AF_INET6, tc.api_param.data.local.c_str(), (void*)&local);

  ASSERT_EQ(1, ret) << "[ipv6]: local address must be a valid IPv6 address.";
  attr = ls_attributes_new(adv, in_addr{}, local, 0);
}

void EdgeTest::FillDstAttributes(const TestCase& tc, in6_addr remote,
                                 ls_attributes& attr) const {
  if (!tc.api_param.data.name.empty()) {
    strncpy(attr.name, tc.api_param.data.name.c_str(), MAX_NAME_LENGTH);
    SET_FLAG(attr.flags, LS_ATTR_NAME);
  }

  attr.metric = tc.api_param.data.metric;
  SET_FLAG(attr.flags, LS_ATTR_METRIC);

  int ret =
      inet_pton(AF_INET6, tc.api_param.data.remote.c_str(), (void*)&remote);
  ASSERT_EQ(1, ret) << "[ipv6]: remote address must be a valid IPv6 address.";

  attr.standard.remote6 = remote;
  SET_FLAG(attr.flags, LS_ATTR_NEIGH_ADDR6);
}

TEST_P(EdgeTest, ValidateEdgeUpdate) {
  // Arrange
  TestCase tc = GetParam();

  // Note that for arrange, we also want to place TED entries before the one
  // we actually want to test

  // TODO model may have IS-IS level as a free variable to test for IS-IS
  // interoperability between level 1 and level 2 nodes (specifically 1/2
  // nodes); check this again in the future
  ls_node_id remote_node_id{};
  GetRemoteNodeId(tc, remote_node_id);

  // TODO same as remote node - see above
  ls_node_id adv_node_id{};
  GetRemoteNodeId(tc, adv_node_id);

  in6_addr local6;
  ls_attributes* attr;
  FillSrcAttributes(tc, adv_node_id, local6, attr);

  if (attr == nullptr) {
    GTEST_SKIP() << "[ls_attr]: test " << tc.test_id
                 << " provides no meaningful input.";
  }

  in6_addr remote6;
  FillDstAttributes(tc, remote6, *attr);

  struct ls_node* remote_node = ls_node_new(remote_node_id, in_addr{}, remote6);

  // TODO send ls_message with remote_node

  struct ls_node* adv_node = ls_node_new(adv_node_id, in_addr{}, local6);

  // TODO send ls_message with adv_node

  struct stream* bgpd_stream = stream_new(ZEBRA_MAX_PACKET_SIZ);

  struct ls_message remote_msg = {
      .event = static_cast<uint8_t>(tc.api_param.event),
      .type = LS_MSG_TYPE_NODE,
      .data = {.node = remote_node}};

  // from ls_format_msg (lib/link_state.c, lines 1771, 1794)
  stream_putc(bgpd_stream, static_cast<uint8_t>(tc.api_param.event));
  stream_putc(bgpd_stream, LS_MSG_TYPE_NODE);

  // from ls_format_node (lib/link_state.c, lines 1532-1580)
  stream_put(bgpd_stream, &remote_node->adv, sizeof(struct ls_node_id));
  stream_putw(bgpd_stream, remote_node->flags);

  stream_put(bgpd_stream, &remote_node->router_id6, IPV6_MAX_BYTELEN);

  bridge_send_message(bgpd_stream, zapi_opaque_registry::LINK_STATE_UPDATE);

  stream_reset(bgpd_stream);

  struct ls_message adv_msg = {
      .event = static_cast<uint8_t>(tc.api_param.event),
      .type = LS_MSG_TYPE_NODE,
      .data = {.node = adv_node}};

  // same as remote_node

  // from ls_format_msg (lib/link_state.c, lines 1771, 1794)
  stream_putc(bgpd_stream, static_cast<uint8_t>(tc.api_param.event));
  stream_putc(bgpd_stream, LS_MSG_TYPE_NODE);

  // from ls_format_node (lib/link_state.c, lines 1532-1580)
  stream_put(bgpd_stream, &adv_node->adv, sizeof(struct ls_node_id));
  stream_putw(bgpd_stream, adv_node->flags);

  stream_put(bgpd_stream, &adv_node->router_id6, IPV6_MAX_BYTELEN);

  bridge_send_message(bgpd_stream, zapi_opaque_registry::LINK_STATE_UPDATE);

  stream_reset(bgpd_stream);

  struct ls_message edge_msg = {
      .event = static_cast<uint8_t>(tc.api_param.event),
      .type = LS_MSG_TYPE_ATTRIBUTES,
      .remote_id = remote_node_id,
      .data = {.attr = attr}};

  // see lib/link_state.c, lines 1836-1842 (entry point)

  // from ls_format_msg (lib/link_state.c, lines 1771-1794)
  stream_putc(bgpd_stream, static_cast<uint8_t>(tc.api_param.event));
  stream_putc(bgpd_stream, LS_MSG_TYPE_ATTRIBUTES);

  stream_put(bgpd_stream, (void*)&remote_node_id, sizeof(struct ls_node_id));

  // from ls_format_attributes (lib/link_state.c, lines 1582-1725)
  size_t len;

  stream_put(bgpd_stream, &attr->adv, sizeof(struct ls_node_id));

  stream_putl(bgpd_stream, attr->flags);

  if (CHECK_FLAG(attr->flags, LS_ATTR_NAME)) {
    len = strlen(attr->name);
    stream_putc(bgpd_stream, len + 1);
    stream_put(bgpd_stream, attr->name, len);
    stream_putc(bgpd_stream, '\0');
  }

  stream_putl(bgpd_stream, attr->metric);
  stream_put(bgpd_stream, &attr->standard.local6, IPV6_MAX_BYTELEN);
  stream_put(bgpd_stream, &attr->standard.remote6, IPV6_MAX_BYTELEN);

  // this line was in the lib/link_state.c implementation, but is probably not
  // needed
  // stream_putw_at(bgpd_stream, 0, stream_get_endp(bgpd_stream));

  // Act
  bridge_send_message(bgpd_stream, zapi_opaque_registry::LINK_STATE_UPDATE);

  // Debug
  struct sbuf sbuf;
  sbuf_init(&sbuf, NULL, 0);

  bridge_show_ted(&sbuf);
  std::cout << sbuf_buf(&sbuf) << std::endl;

  // Assert
  // TODO implement assertion to check if the TED has two-way direction
  // installed, as well as the RIB
  ASSERT_TRUE(bridge_edge_exists_ted(attr))
      << "[ls_attributes]: edge does not exist within TED.";

  // Clean
  sbuf_free(&sbuf);
  ls_node_del(adv_node);
  ls_node_del(remote_node);
  ls_attributes_del(attr);
  stream_free(bgpd_stream);
}

INSTANTIATE_TEST_SUITE_P(CrossHairCoverageTestCases, EdgeTest,
                         ::testing::ValuesIn(testCases));

}  // namespace Model
