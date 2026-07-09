#ifndef FRR_BRIDGE_H
#define FRR_BRIDGE_H

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

#ifdef __cplusplus
}
#endif

#endif  // FRR_BRIDGE_H
