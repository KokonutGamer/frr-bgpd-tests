#include "common_data.h"

namespace Model {

LinkStateEdge::operator BApiLinkStateUpdate() const {
  BApiLinkStateUpdate message{.event = BEvent::UPDATE,
                              .remote = this->destination_node,
                              .data = {.adv = this->source_node,
                                       .metric = 0,
                                       .local = this->source,
                                       .remote = this->destination}};
  return message;
}

}  // namespace Model
