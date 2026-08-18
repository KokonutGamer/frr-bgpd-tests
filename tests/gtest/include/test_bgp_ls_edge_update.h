#ifndef TEST_BGP_LS_EDGE_UPDATE_H
#define TEST_BGP_LS_EDGE_UPDATE_H

#include <gtest/gtest.h>

#include "common_data.h"
#include "test_bgp_ls_linkstate_update.h"

/**
 * Hacky solution to compiling with C++ (keyword delete cannot be used as a
 * variable name); see https://stackoverflow.com/a/25647229
 */
#define delete to_delete
#include "lib/link_state.h"
#undef delete

namespace Model {

class EdgeTest : public LinkStateTest<LinkStateAttributes, ls_attributes> {
 protected:
  virtual ls_attributes* SendUpdateMessage(
      const BApiLinkStateUpdate<LinkStateAttributes>& apiMessage)
      const override;
};

}  // namespace Model

#endif  // TEST_BGP_LS_EDGE_UPDATE_H
