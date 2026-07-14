#ifndef FRR_BRIDGE_H
#define FRR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include "lib/stream.h"

typedef struct bgp bgp_t;
typedef struct peer peer_t;
typedef struct bgp_path_info bgp_path_info_t;

void bridge_init_bgp(void);

void bridge_clean_bgp(void);

void bridge_send_message(struct stream* s, uint8_t msg_type);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // FRR_BRIDGE_H