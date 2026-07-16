#ifndef FRR_BRIDGE_H
#define FRR_BRIDGE_H

#include "lib/stream.h"
#include "sbuf.h"  // modified version of lib/sbuf.h

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes FRR's BGP daemon along with a single BGP routing instance.
 *
 * The configuration specified by this function is bare-bones, allowing for
 * testing of BGP and BGP-LS without requiring a Zebra instance or an IGP daemon
 * to be running at the same time. It may be extended in the future for testing
 * with pthreads enabled.
 */
void bridge_init_bgp(void);

/**
 * @brief Clears the BGP instance's link-state TED.
 *
 * This function is called on `TearDown` after each model test case. Note that
 * FRR states that the RIB is also cleared during the function call to
 * `bgp_ls_withdraw_ted`, specifically only for BGP-LS locally originated paths.
 */
void bridge_clear_bgp_ls_ted(void);

/**
 * @brief Cleans the running BGP daemon along with any BGP routing instances.
 *
 * This function only needs to terminate all running BGP instances before the
 * daemon is shutdown.
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

/**
 * @brief Pushes BGP-LS's TED to a string buffer for debugging.
 */
void bridge_show_ted(struct sbuf* sbuf);

#ifdef __cplusplus
}
#endif

#endif  // FRR_BRIDGE_H
