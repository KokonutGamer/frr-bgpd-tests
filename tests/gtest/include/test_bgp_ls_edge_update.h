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
   * TODO document how tear down works; this should include clearing the BGP
   * TED.
   */
  virtual void TearDown() override;

  /**
   * TODO document how this was taken from FRR's implementation
   */
  int SysIdToBuffer(uint8_t* buff, const char* dotted) const;

  /**
   * TODO document this utility function
   */
  void GetRemoteNodeId(const TestCase& tc, ls_node_id& remote) const;

  /**
   * TODO document this utility function
   */
  void GetAdvNodeId(const TestCase& tc, ls_node_id& adv) const;

  /**
   * TODO document this utility function
   */
  void FillSrcAttributes(const TestCase& tc, const ls_node_id& adv, in6_addr& local,
                     ls_attributes* &attr) const;

  /**
   * TODO document this utility function
   */
  void FillDstAttributes(const TestCase& tc, in6_addr remote,
                      ls_attributes& attr) const;
};
}  // namespace Model

#endif  // TEST_BGP_LS_EDGE_UPDATE_H
