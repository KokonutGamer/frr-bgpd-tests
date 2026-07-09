#ifndef COMMON_DATA_H
#define COMMON_DATA_H

#include <cstdint>
#include <string>
#include <vector>

using prefix_t = std::string;
using addr_t = std::string;

#define NUM_BYTES_SYS_ID 6

struct BgpLsNode
{
    uint32_t asn;
    addr_t igp_router_id;
};

struct BgpLsLink
{
    addr_t interface;
    addr_t neighbor;
    uint32_t remote_asn;
};

struct BgpLsLinkNlri
{
    BgpLsNode source;
    BgpLsNode destination;
    BgpLsLink link;
};

struct LinkStateNodeId
{
    uint8_t iso_sys_id[NUM_BYTES_SYS_ID];
    uint8_t level;
};

struct LinkStateEdge
{
    uint32_t asn;
    LinkStateNodeId source_node;
    LinkStateNodeId dest_node;
    addr_t source;
    addr_t destination;
};

struct LinkStateAttributes
{
    bool valid;
    LinkStateNodeId adv;
    std::string name;
    uint32_t metric;
    addr_t local;
    addr_t remote;
};

struct BgpRibEntry
{
    prefix_t synth_prefix;
    BgpLsLinkNlri ls_nlri;
};

enum class BEvent : uint8_t
{
    UNDEF = 0,
    SYNC,
    ADD,
    UPDATE,
    DELETE
};

struct BApiLinkStateUpdate
{
    BEvent event;
    LinkStateNodeId remote;
    LinkStateAttributes data;
};

struct BgpLsLinkState
{
    uint32_t asn;
    std::vector<LinkStateEdge> ted;
    std::vector<BgpRibEntry> rib;
    prefix_t next_id;
};

struct TestCase
{
    int test_id;
    std::string op; // currently always "api_bgp_ls_edge_update"
    BgpLsLinkState initial_state;
    BApiLinkStateUpdate api_param;
    BgpLsLinkState final_state;
    int response;
};

#endif // COMMON_DATA_H