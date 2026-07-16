#include "frr_bridge.h"

// clang-format off
#include <zebra.h>

#include <pthread.h>
#include "vector.h"
#include "command.h"
#include "getopt.h"
#include "frrevent.h"
#include <lib/version.h>
#include "memory.h"
#include "prefix.h"
#include "log.h"
#include "privs.h"
#include "sigevent.h"
#include "zclient.h"
#include "routemap.h"
#include "filter.h"
#include "plist.h"
#include "stream.h"
#include "queue.h"
#include "vrf.h"
#include "bfd.h"
#include "libfrr.h"
#include "ns.h"
#include "libagentx.h"
#include <string.h>
#include <sys/stat.h>

#include "bgpd/bgpd.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_mplsvpn.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_dump.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_nexthop.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_clist.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_errors.h"
#include "bgpd/bgp_filter.h"
#include "bgpd/bgp_zebra.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_keepalives.h"
#include "bgpd/bgp_network.h"
#include "bgpd/bgp_errors.h"
#include "bgpd/bgp_evpn_mh.h"
#include "bgpd/bgp_nhg.h"
#include "bgpd/bgp_routemap_nb.h"
#include "bgpd/bgp_community_alias.h"

#include "bgpd/bgp_ls.h"
#include "bgpd/bgp_ls_ted.h"
// clang-format on

/**
 * Initialized later. We just need this as a static variable to perform bgp
 * operations.
 */
static struct bgp *bgp = NULL;
static as_t asn = 100;

DEFINE_HOOK(bgp_hook_config_write_vrf, (struct vty * vty, struct vrf *vrf),
            (vty, vrf));

#ifdef ENABLE_BGP_VNC
#include "bgpd/rfapi/rfapi_backend.h"
#endif

DEFINE_HOOK(bgp_hook_vrf_update, (struct vrf * vrf, bool enabled),
            (vrf, enabled));

void sighup(void);
void sigint(void);
void sigusr1(void);

static void bgp_exit(int);
static void bgp_vrf_terminate(void);

static struct frr_signal_t bgp_signals[] = {
    {
        .signal = SIGHUP,
        .handler = &sighup,
    },
    {
        .signal = SIGUSR1,
        .handler = &sigusr1,
    },
    {
        .signal = SIGINT,
        .handler = &sigint,
    },
    {
        .signal = SIGTERM,
        .handler = &sigint,
    },
};

// mimics skip runas CLI option
struct zebra_privs_t bgpd_privs = {0};

static struct frr_daemon_info bgpd_di;

void sighup(void) { zlog_info("SIGHUP received, ignoring"); }

FRR_NORETURN void sigint(void) {
  zlog_notice("Terminating on signal");
  assert(bm->terminating == false);
  bm->terminating = true;

  bfd_protocol_integration_set_shutdown(true);

  bgp_terminate();

  bgp_exit(0);

  peer_connection_fifo_fini(&bm->connection_fifo);
  event_cancel(&bm->e_process_packet);
  pthread_mutex_destroy(&bm->peer_connection_mtx);

  exit(0);
}

void sigusr1(void) { zlog_rotate(); }

static void bgp_exit(int status) {
  struct bgp *bgp, *bgp_default, *bgp_evpn;
  struct listnode *node, *nnode;

  assert(status == 0);

  frr_early_fini();

  bgp_close();

  bgp_default = bgp_get_default();
  bgp_evpn = bgp_get_evpn();

  bgp_lp_release_pending_lu_locks();

  /* reverse bgp_master_init */
  for (ALL_LIST_ELEMENTS(bm->bgp, node, nnode, bgp)) {
    if (bgp_default == bgp || bgp_evpn == bgp) continue;
    bgp_delete(bgp);
  }
  if (bgp_evpn && bgp_evpn != bgp_default) bgp_delete(bgp_evpn);
  if (bgp_default) bgp_delete(bgp_default);

  bgp_evpn_mh_finish();
  bgp_nhg_finish();

  zebra_announce_fini(&bm->zebra_announce_head);
  zebra_announce_fini(&bm->zebra_announce_early_head);
  zebra_l2_vni_fini(&bm->zebra_l2_vni_head);

  /* reverse bgp_dump_init */
  bgp_dump_finish();

  /* BGP community aliases */
  bgp_community_alias_finish();

  /* reverse bgp_route_init */
  bgp_route_finish();

  /* cleanup route maps */
  bgp_route_map_terminate();

  /* reverse bgp_attr_init */
  bgp_attr_finish();

  /* reverse bgp_labels_init */
  bgp_labels_finish();

  /* stop pthreads */
  bgp_pthreads_finish();

  /* reverse access_list_init */
  access_list_add_hook(NULL);
  access_list_delete_hook(NULL);
  access_list_reset();

  /* reverse bgp_filter_init */
  as_list_add_hook(NULL);
  as_list_delete_hook(NULL);
  bgp_filter_reset();

  /* reverse prefix_list_init */
  prefix_list_add_hook(NULL);
  prefix_list_delete_hook(NULL);
  prefix_list_reset();

  /* reverse community_list_init */
  community_list_terminate(bgp_clist);

  bgp_vrf_terminate();
#ifdef ENABLE_BGP_VNC
  vnc_zebra_destroy();
#endif
  bgp_zebra_destroy();

  bgp_debug_destroy();

  bf_free(bm->rd_idspace);
  list_delete(&bm->bgp);
  list_delete(&bm->addresses);

  bgp_lp_finish();

  memset(bm, 0, sizeof(*bm));

  frr_fini();
}

static int bgp_vrf_new(struct vrf *vrf) {
  if (BGP_DEBUG(zebra, ZEBRA))
    zlog_debug("VRF Created: %s(%u)", vrf->name, vrf->vrf_id);

  return 0;
}

static int bgp_vrf_delete(struct vrf *vrf) {
  if (BGP_DEBUG(zebra, ZEBRA))
    zlog_debug("VRF Deletion: %s(%u)", vrf->name, vrf->vrf_id);

  return 0;
}

static int bgp_vrf_enable(struct vrf *vrf) {
  struct bgp *bgp;
  vrf_id_t old_vrf_id;
  int ret = BGP_GR_FAILURE;
  (void)ret; /* for unused variable warning */

  if (BGP_DEBUG(zebra, ZEBRA))
    zlog_debug("VRF enable add %s id %u", vrf->name, vrf->vrf_id);

  bgp = bgp_lookup_by_name(vrf->name);
  if (bgp && bgp->vrf_id != vrf->vrf_id) {
    old_vrf_id = bgp->vrf_id;
    /* We have instance configured, link to VRF and make it "up". */
    bgp_vrf_link(bgp, vrf);

    VTY_BGP_GR_ROUTER_DETECT_AND_SEND_CAPABILITY_TO_ZEBRA(bgp, bgp->peer, ret);

    bgp_handle_socket(bgp, vrf, old_vrf_id, true);
    bgp_instance_up(bgp);
    hook_call(bgp_hook_vrf_update, vrf, true);
    vpn_leak_zebra_vrf_label_update(bgp, AFI_IP);
    vpn_leak_zebra_vrf_label_update(bgp, AFI_IP6);
    vpn_leak_zebra_vrf_sid_update(bgp, AFI_IP);
    vpn_leak_zebra_vrf_sid_update(bgp, AFI_IP6);
    vpn_leak_postchange(BGP_VPN_POLICY_DIR_TOVPN, AFI_IP, bgp_get_default(),
                        bgp);
    vpn_leak_postchange(BGP_VPN_POLICY_DIR_FROMVPN, AFI_IP, bgp_get_default(),
                        bgp);
    vpn_leak_postchange(BGP_VPN_POLICY_DIR_TOVPN, AFI_IP6, bgp_get_default(),
                        bgp);
    vpn_leak_postchange(BGP_VPN_POLICY_DIR_FROMVPN, AFI_IP6, bgp_get_default(),
                        bgp);
  }

  return 0;
}

static int bgp_vrf_disable(struct vrf *vrf) {
  struct bgp *bgp;

  if (vrf->vrf_id == VRF_DEFAULT) return 0;

  if (BGP_DEBUG(zebra, ZEBRA))
    zlog_debug("VRF disable %s id %d", vrf->name, vrf->vrf_id);

  bgp = bgp_lookup_by_name_filter(vrf->name, false);
  if (bgp) {
    vpn_leak_zebra_vrf_label_withdraw(bgp, AFI_IP);
    vpn_leak_zebra_vrf_label_withdraw(bgp, AFI_IP6);
    vpn_leak_prechange(BGP_VPN_POLICY_DIR_TOVPN, AFI_IP, bgp_get_default(),
                       bgp);
    vpn_leak_prechange(BGP_VPN_POLICY_DIR_FROMVPN, AFI_IP, bgp_get_default(),
                       bgp);
    vpn_leak_prechange(BGP_VPN_POLICY_DIR_TOVPN, AFI_IP6, bgp_get_default(),
                       bgp);
    vpn_leak_prechange(BGP_VPN_POLICY_DIR_FROMVPN, AFI_IP6, bgp_get_default(),
                       bgp);

    bgp_handle_socket(bgp, vrf, VRF_UNKNOWN, false);
    /* We have instance configured, unlink from VRF and make it
     * "down". */
    bgp_instance_down(bgp);
    bgp_vrf_unlink(bgp, vrf);
    hook_call(bgp_hook_vrf_update, vrf, false);
  }

  /* Note: This is a callback, the VRF will be deleted by the caller. */
  return 0;
}

static int bgp_vrf_config_write(struct vty *vty) {
  struct vrf *vrf;

  RB_FOREACH(vrf, vrf_name_head, &vrfs_by_name) {
    if (vrf->vrf_id == VRF_DEFAULT) {
      vty_out(vty, "!\n");
      continue;
    }
    vty_out(vty, "vrf %s\n", vrf->name);

    hook_call(bgp_hook_config_write_vrf, vty, vrf);

    vty_out(vty, "exit-vrf\n!\n");
  }

  return 0;
}

static void bgp_vrf_init(void) {
  vrf_init(bgp_vrf_new, bgp_vrf_enable, bgp_vrf_disable, bgp_vrf_delete);
  vrf_cmd_init(bgp_vrf_config_write);
}

static void bgp_vrf_terminate(void) { vrf_terminate(); }

static const struct frr_yang_module_info *const bgpd_yang_modules[] = {
    &frr_filter_info, &frr_interface_info,     &frr_route_map_info,
    &frr_vrf_info,    &frr_bgp_route_map_info,
};

FRR_DAEMON_INFO(bgpd, BGP, .vty_port = BGP_VTY_PORT,
                .proghelp = "Implementation of the BGP routing protocol.",
                .signals = bgp_signals, .n_signals = array_size(bgp_signals),
                .privs = &bgpd_privs, .yang_modules = bgpd_yang_modules,
                .n_yang_modules = array_size(bgpd_yang_modules), );

void bridge_init_bgp(void) {
  const char *test_dir = "/tmp/frr";
  mkdir(test_dir, 0777);

  strncpy(frr_libstatedir, test_dir, 255);
  strncpy(frr_runstatedir, test_dir, 255);

  int bgp_port = BGP_PORT_DEFAULT;
  struct list *addresses = list_new();
  int instance = 0;
  int buffer_size = BGP_SOCKET_SNDBUF_SIZE;
  char *address;
  struct listnode *node;
  bool v6_with_v4_nexthops = false;

  addresses->cmp = (int (*)(void *, void *))strcmp;

  int argc = 4;
  char *argv[] = {"bgpd", "-i", "/tmp/frr_test/bgpd.pid", NULL};
  frr_preinit(&bgpd_di, argc, argv);

  bgpd_di.limit_fds = 1000;  // mimics setting the --limit-fds option

  // bm is a a global bgp_master struct defined in bgpd/bgpd.h
  // we initialize it with bgp_master_init
  bgp_master_init(frr_init(), buffer_size, addresses);

  bm->startup_time = monotime(NULL);
  bm->port = bgp_port;
  bm->v6_with_v4_nexthops = v6_with_v4_nexthops;

  bgp_option_set(BGP_OPT_NO_LISTEN);
  bgp_option_set(BGP_OPT_NO_FIB);
  bgp_option_set(BGP_OPT_NO_ZEBRA);

  bgp_error_init();
  libagentx_init();
  bgp_vrf_init();  // TODO check if this is actually needed (static method in
                   // source, see bgpd/bgp_main.c)

  bgp_init((unsigned short)instance);

  if (list_isempty(bm->addresses)) {
    snprintf(bgpd_di.startinfo, sizeof(bgpd_di.startinfo), ", bgp@<all>:%d",
             bm->port);
  } else {
    for (ALL_LIST_ELEMENTS_RO(bm->addresses, node, address))
      snprintf(bgpd_di.startinfo + strlen(bgpd_di.startinfo),
               sizeof(bgpd_di.startinfo) - strlen(bgpd_di.startinfo),
               ", bgp@%s:%d", address, bm->port);
  }

  bgp_if_init();

  frr_config_fork();
  bgp_pthreads_run();

  bgp_get(&bgp, &asn, NULL, BGP_INSTANCE_TYPE_DEFAULT, NULL, ASNOTATION_PLAIN);
}

void bridge_shallow_init_bgp(void) {
  qobj_init();
  bgp_attr_init();
  struct event_loop *master = event_master_create(NULL);
  bgp_master_init(master, BGP_SOCKET_SNDBUF_SIZE, list_new());
  vrf_init(NULL, NULL, NULL, NULL);
  bgp_option_set(BGP_OPT_NO_LISTEN);

  bgp_get(&bgp, &asn, NULL, BGP_INSTANCE_TYPE_DEFAULT, NULL, ASNOTATION_PLAIN);
}

void bridge_clear_bgp_ls_ted(void) { bgp_ls_withdraw_ted(bgp); }

void bridge_clean_bgp(void) {
  assert(bm->terminating == false);
  bm->terminating = true;

  bfd_protocol_integration_set_shutdown(true);

  bgp_terminate();

  bgp_exit(0);

  peer_connection_fifo_fini(&bm->connection_fifo);
  event_cancel(&bm->e_process_packet);
  pthread_mutex_destroy(&bm->peer_connection_mtx);
}

void bridge_shallow_clean_bgp(void) {
  assert(bm->terminating == false);
  bm->terminating = true;

  bfd_protocol_integration_set_shutdown(true);

  bgp_terminate();
}

void bridge_send_message(struct stream *s, uint8_t msg_type) {
  bgp_ls_process_linkstate_message(s, msg_type);
}

static const char *ls_node_id_to_text(struct ls_node_id lnid, char *str,
                                      size_t size) {
  if (lnid.origin == ISIS_L1 || lnid.origin == ISIS_L2)
    snprintfrr(str, size, "%pSY", lnid.id.iso.sys_id);
  else
    snprintfrr(str, size, "%pI4", &lnid.id.ip.addr);

  return str;
}

static const char *edge_key_to_text(struct ls_edge_key key) {
#define FORMAT_BUF_COUNT 4
  static char buf_ring[FORMAT_BUF_COUNT][INET6_BUFSIZ];
  static size_t cur_buf = 0;
  char *rv;

  rv = buf_ring[cur_buf];
  cur_buf = (cur_buf + 1) % FORMAT_BUF_COUNT;

  switch (key.family) {
    case AF_INET:
      snprintfrr(rv, INET6_BUFSIZ, "%pI4", &key.k.addr);
      break;
    case AF_INET6:
      snprintfrr(rv, INET6_BUFSIZ, "%pI6", &key.k.addr6);
      break;
    case AF_LOCAL:
      snprintfrr(rv, INET6_BUFSIZ, "%" PRIu64, key.k.link_id);
      break;
    default:
      snprintfrr(rv, INET6_BUFSIZ, "(Unknown)");
      break;
  }

  return rv;
}

static const char *const origin2txt[] = {"Unknown", "ISIS_L1", "ISIS_L2",
                                         "OSPFv2",  "Direct",  "Static"};

void bridge_show_ted(struct sbuf *sbuf) {
  struct ls_edge *edge;
  frr_each(edges, &bgp->ls_info->ted->edges, edge) {
    if (!edge) {
      continue;
    }

    // from ls_show_edge_vty (lib/link_state.c, lines 2463-2637)

    struct ls_attributes *attr = edge->attributes;
    char buf[INET6_BUFSIZ];

    sbuf_init(sbuf, NULL, 0);
    sbuf_push(sbuf, 2, "Edge (%s): ", edge_key_to_text(edge->key));
    sbuf_push(sbuf, 0, "%pI6", &attr->standard.local6);
    ls_node_id_to_text(attr->adv, buf, INET6_BUFSIZ);
    sbuf_push(sbuf, 0, "\tAdv. Vertex: %s", buf);
    sbuf_push(sbuf, 0, "\tMetric: %u", attr->metric);
    sbuf_push(sbuf, 4, "Origin: %s\n", origin2txt[attr->adv.origin]);

    sbuf_push(sbuf, 4, "Local IPv6 address: %pI6\n", &attr->standard.local6);
    sbuf_push(sbuf, 4, "Remote IPv6 address: %pI6\n", &attr->standard.remote6);
  }
}
