#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

#include "common_data.h"
#include "frr_bridge.h"

// some FRR headers (mainly lib/ headers) already include guards for C++

#define delete to_delete
#include "lib/link_state.h"
#undef delete

#include "lib/stream.h"
#include "lib/zclient.h"

#define ISIS_SYS_ID_LEN 6

namespace Model {

class EdgeTest : public testing::TestWithParam<TestCase> {
 protected:
  void SetUp() override {
    // TODO perhaps do some checks on bgpd before testing
    // think of a "liveness" check - to see if bgpd is working

    // in addition, for each value-paramterized test, we should ensure the
    // LS TED and RIB is empty
  }

  void TearDown() override {
    // TODO check bgpd
    // perhaps have each test clean up the LS TED and RIB
  }

  /**
   * TODO finish documentation
   *
   * Copied from source (I'd rather not statically link against libisis.a since
   * multiple different symbols need to be defined when doing so).
   */
  int sysid2buff(uint8_t* buff, const char* dotted) {
    int len = 0;
    const char* pos = dotted;
    uint8_t number[3];

    number[2] = '\0';
    // surely not a sysid_string if not 14 length
    if (strlen(dotted) != 14) {
      return 0;
    }

    while (len < ISIS_SYS_ID_LEN) {
      if (*pos == '.') {
        /* the . is not positioned correctly */
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
};

TEST_P(EdgeTest, ValidateEdgeUpdate) {
  // Arrange
  TestCase tc = GetParam();

  // Note that for arrange, we also want to place TED entries before the one
  // we actually want to test

  // TODO model may have IS-IS level as a free variable to test for IS-IS
  // interoperability between level 1 and level 2 nodes (specifically 1/2
  // nodes); check this again in the future
  struct ls_node_id remote_node = {.origin = ls_origin::ISIS_L1,
                                   .id = {.iso = {.level = 1}}};
  int ret = sysid2buff(remote_node.id.iso.sys_id,
                       tc.api_param.remote.iso_sys_id.c_str());
  ASSERT_EQ(ISIS_SYS_ID_LEN, ret)
      << "[sys_id]: remote node ID must be a valid ISO system identifier.";

  // TODO same as remote node - see above
  struct ls_node_id adv_node = {.origin = ls_origin::ISIS_L1,
                                .id = {.iso = {.level = 1}}};
  ret = sysid2buff(adv_node.id.iso.sys_id,
                   tc.api_param.data.adv.iso_sys_id.c_str());
  ASSERT_EQ(ISIS_SYS_ID_LEN, ret)
      << "[sys_id]: advertising node ID must be a valid ISO system identifier.";

  struct in_addr local = {.s_addr = INADDR_ANY};
  struct in6_addr local6;
  uint32_t local_id = 0;
  ret = inet_pton(AF_INET6, tc.api_param.data.local.c_str(), (void*)&local6);

  ASSERT_EQ(1, ret) << "[ipv6]: local address must be a valid IPv6 address.";
  struct ls_attributes* attr =
      ls_attributes_new(adv_node, local, local6, local_id);

  if (attr == nullptr) {
    GTEST_SKIP() << "[ls_attr]: test " << tc.test_id
                 << " provides no meaningful input.";
  }

  if (!tc.api_param.data.name.empty()) {
    strncpy(attr->name, tc.api_param.data.name.c_str(), MAX_NAME_LENGTH);
    SET_FLAG(attr->flags, LS_ATTR_NAME);
  }

  attr->metric = tc.api_param.data.metric;
  SET_FLAG(attr->flags, LS_ATTR_METRIC);

  struct in6_addr remote6;
  ret = inet_pton(AF_INET6, tc.api_param.data.remote.c_str(), (void*)&remote6);
  ASSERT_EQ(1, ret) << "[ipv6]: remote address must be a valid IPv6 address.";

  attr->standard.remote6 = remote6;
  SET_FLAG(attr->flags, LS_ATTR_NEIGH_ADDR6);

  struct ls_message msg = {.event = static_cast<uint8_t>(tc.api_param.event),
                           .type = LS_MSG_TYPE_ATTRIBUTES,
                           .remote_id = remote_node,
                           .data = {.attr = attr}};

  struct stream* bgpd_stream = stream_new(ZEBRA_MAX_PACKET_SIZ);

  // see lib/link_state.c, lines 1836-1842 (entry point)

  // from ls_format_msg (lib/link_state.c, lines 1771-1794)
  stream_putc(bgpd_stream, static_cast<uint8_t>(tc.api_param.event));
  stream_putc(bgpd_stream, LS_MSG_TYPE_ATTRIBUTES);

  stream_put(bgpd_stream, (void*)&remote_node, sizeof(struct ls_node_id));

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

  stream_putw_at(bgpd_stream, 0, stream_get_endp(bgpd_stream));

  // Act
  bridge_send_message(bgpd_stream, zapi_opaque_registry::LINK_STATE_UPDATE);

  // Assert
  // TODO act first; this assertion will check if the TED has two-way direction
  // installed, as well as the RIB
  ASSERT_NE(tc.test_id, 0);

  ls_attributes_del(attr);
  stream_free(bgpd_stream);
}

INSTANTIATE_TEST_SUITE_P(CrossHairCoverageTestCases, EdgeTest,
                         ::testing::ValuesIn(testCases));

}  // namespace Model