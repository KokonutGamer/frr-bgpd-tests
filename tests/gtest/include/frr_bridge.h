#ifndef FRR_BRIDGE_H
#define FRR_BRIDGE_H

#include "lib/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes FRR's BGP daemon along with a single BGP routing instance.
 */
void bridge_init_bgp(void);

/**
 * @brief Cleans the running BGP daemon along with any BGP routing instances.
 */
void bridge_clean_bgp(void);

/**
 * @brief Sends a link-state message to the BGP instance.
 *
 * Note that this is specifically for link-state messages (`ls_message`), NOT
 * for any other type of message. Because the test suite doesn't use a mock
 * Zebra instance, this function needs to route the message via an FRR stream.
 */
void bridge_send_message(struct stream* s, uint8_t msg_type);

#ifdef __cplusplus
}
#endif

#endif  // FRR_BRIDGE_H
