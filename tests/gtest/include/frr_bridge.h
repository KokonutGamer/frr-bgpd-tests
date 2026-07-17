#ifndef FRR_BRIDGE_H
#define FRR_BRIDGE_H

#include "lib/stream.h"
#include "sbuf.h"  // modified version of lib/sbuf.h

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void bridge_shallow_init_bgp(void);

void bridge_clear_bgp_ls_ted(void);

void bridge_shallow_clean_bgp(void);

void bridge_send_message(struct stream* s, uint8_t msg_type);

void bridge_show_ted(struct sbuf* sbuf);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // FRR_BRIDGE_H