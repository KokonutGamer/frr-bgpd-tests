#include "frr_bridge.h"

// clang-format off
#include <stdlib.h>
#include <zebra.h>

#include "lib/asn.h"
#include "lib/bfd.h"
#include "lib/compiler.h"
#include "lib/frrevent.h"
#include "lib/libfrr.h"
#include "lib/linklist.h"
#include "lib/log_vty.h"
#include "lib/northbound.h"
#include "lib/privs.h"
#include "lib/sigevent.h"
#include "lib/typesafe.h"
#include "lib/version.h"
#include "lib/vrf.h"
#include "lib/qobj.h"
#include "lib/zlog.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_ls.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_network.h"
#include "bgpd/bgp_routemap_nb.h"
#include "bgpd/bgp_ls_ted.h"
#include "bgpd/bgp_vty.h"
// clang-format on

/**
 * The BGP instance to initialize in bgpd. Note that because this is declared as
 * static, teardown of this memory isn't necessary as the operating system will
 * automatically clean this up for us. In Valgrind, the memory pointed to by
 * this pointer will be considered "still reachable".
 */
static struct bgp *bgp = NULL;

/**
 * The Autonomous System number assigned to the BGP instance running in bgpd.
 */
static as_t asn = 100;

void sighup(void);
void sigint(void);
void sigusr1(void);

/**
 * Signal handlers for catching signals from the operating system.
 */
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

void sighup(void) { zlog_info("SIGHUP received, ignoring"); }

FRR_NORETURN void sigint(void) {
  zlog_notice("Terminating on signal");
  assert(bm->terminating == false);
  bm->terminating = true;
  bfd_protocol_integration_set_shutdown(true);
  bgp_terminate();
  exit(EXIT_SUCCESS);
}

void sigusr1(void) { zlog_rotate(); }

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
  qobj_init();
  bgp_attr_init();
  cmd_init(0);
  bgp_vty_init();
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

static const char *const status2txt[] = {"Unknown", "New",  "Update",
                                         "Delete",  "Sync", "Orphan"};

static const char *const origin2txt[] = {"Unknown", "ISIS_L1", "ISIS_L2",
                                         "OSPFv2",  "Direct",  "Static"};

static void vertex_to_text(const struct ls_vertex *const vertex,
                           struct sbuf *sbuf) {
  struct ls_node *lsn = vertex->node;
  sbuf_push(sbuf, 2, "Vertex (%" PRIu64 "): %s", vertex->key, lsn->name);
  sbuf_push(sbuf, 0, "\tRouter Id: %pI4", &lsn->router_id);
  sbuf_push(sbuf, 0, "\tOrigin: %s", origin2txt[lsn->adv.origin]);
  sbuf_push(sbuf, 0, "\tStatus: %s\n", status2txt[vertex->status]);
  sbuf_push(sbuf, 0, "\t%d Outgoing Edges, %d Incoming Edges, %d Subnets\n",
            listcount(vertex->outgoing_edges),
            listcount(vertex->incoming_edges), listcount(vertex->prefixes));
}

/**
 * from ls_show_edge_vty (lib/link_state.c, lines 2463-2637)
 */
static void edge_to_text(const struct ls_edge *const edge, struct sbuf *sbuf) {
  struct ls_attributes *attr = edge->attributes;
  char buf[INET6_BUFSIZ];

  sbuf_push(sbuf, 2, "Edge (%s): ", edge_key_to_text(edge->key));
  sbuf_push(sbuf, 0, "%pI6", &attr->standard.local6);
  ls_node_id_to_text(attr->adv, buf, INET6_BUFSIZ);
  sbuf_push(sbuf, 0, "\tAdv. Vertex: %s", buf);
  sbuf_push(sbuf, 0, "\tMetric: %u", attr->metric);
  sbuf_push(sbuf, 4, "Origin: %s\n", origin2txt[attr->adv.origin]);

  sbuf_push(sbuf, 4, "Local IPv6 address: %pI6\n", &attr->standard.local6);
  sbuf_push(sbuf, 4, "Remote IPv6 address: %pI6\n", &attr->standard.remote6);
}

void bridge_show_ted(struct sbuf *sbuf) {
  struct ls_vertex *vertex;
  frr_each(vertices, &bgp->ls_info->ted->vertices, vertex) {
    if (!vertex) {
      continue;
    }
    vertex_to_text(vertex, sbuf);
  }

  struct ls_edge *edge;
  frr_each(edges, &bgp->ls_info->ted->edges, edge) {
    if (!edge) {
      continue;
    }
    edge_to_text(edge, sbuf);
  }
}

bool bridge_edge_exists_ted(struct ls_attributes *attr) {
  struct ls_edge *src_edge = ls_find_edge_by_source(bgp->ls_info->ted, attr);
  struct ls_edge *dst_edge =
      ls_find_edge_by_destination(bgp->ls_info->ted, attr);

  // NOTE: we do not need to free src_edge or dst_edge; these are essentially
  // the raw pointers from the TED, NOT copies, which will be cleaned up during
  // test `TearDown`.

  return src_edge != NULL && dst_edge != NULL &&
         ls_vertex_same(src_edge->source, dst_edge->destination) &&
         ls_vertex_same(src_edge->destination, dst_edge->source);
}
