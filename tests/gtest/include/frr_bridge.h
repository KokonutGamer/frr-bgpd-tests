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
#endif  // __cplusplus

/**
 * TODO document
 */
void bridge_shallow_init_bgp(void);

/**
 * TODO document
 */
void bridge_clear_bgp_ls_ted(void);

/**
 * TODO document
 */
void bridge_shallow_clean_bgp(void);

/**
 * TODO document
 */
void bridge_send_message(struct stream* s, uint8_t msg_type);

/**
 * TODO document
 */
void bridge_show_ted(struct sbuf* sbuf);

/**
 * TODO document
 */
bool bridge_edge_exists_ted(struct ls_attributes* attr);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // FRR_BRIDGE_H
