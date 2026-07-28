#ifndef FRR_BRIDGE_H
#define FRR_BRIDGE_H

#include <stdbool.h>

#define UNKNOWN LS_UNKNOWN
#define delete to_delete
#include "lib/link_state.h"
#undef delete
#undef UNKNOWN

#include "lib/stream.h"
#include "sbuf.h"  // modified version of lib/sbuf.h

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward-declaration for `bridge_link_exists_nlri`.
 */
struct bgp_ls_nlri;

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
 * @brief Checks that FRR's BGP daemon is actively running.
 *
 * Checks if the `bgp` instance is not equal to `NULL` and `bgp_master` is not
 * yet terminating.
 *
 * @return      True if the BGP daemon is running; false otherwise.
 */
bool bridge_check_bgpd_running(void);

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

/**
 * @brief Pushes BGP-LS's RIB table to a string buffer for debugging.
 */
void bridge_show_table(struct sbuf* sbuf);

/**
 * @brief Checks to see if an edge exists within BGP-LS's TED.
 *
 * This function checks two-way connectivity. Therefore, test cases must send
 * both the forward edge and the reverse edge to the BGP instance to ensure this
 * function returns true.
 *
 * @param attr      Link-state attributes corresponding to the edge to check.
 * @return          True if the forward and reverse edge exists; false
 *                      otherwise.
 */
bool bridge_edge_exists_ted(struct ls_attributes* attr);

/**
 * @brief Checks to see if a link NLRI exists within the BGP instance's RIB.
 *
 * This function checks the RIB for a link NLRI corresponding to the NLRI
 * passed.
 *
 * @param nlri      Link NLRI corresponding to the link to check. Represents a
 *                      generic BGP-LS NLRI; however, the value passed should
 *                      point to an NLRI struct with the link parameter filled.
 * @return          True if the NLRI exists; false otherwise.
 */
bool bridge_link_exists_nlri(struct bgp_ls_nlri* nlri);

/**
 * @brief Checks to see if the BGP-LS TED and RIB table are empty.
 *
 * @return      True if both the TED and the RIB table are empty; false
 *                  otherwise.
 */
bool bridge_check_ls_clear();

#ifdef __cplusplus
}
#endif

#endif  // FRR_BRIDGE_H
