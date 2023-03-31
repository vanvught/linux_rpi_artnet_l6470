/**
 * @file arp.cpp
 *
 */
/* Copyright (C) 2018-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <cstdint>
#include <cstring>
#include <cassert>

#include "net.h"
#include "net_private.h"
#include "net_packets.h"
#include "net_debug.h"

#include "../../config/net_config.h"

namespace net {
namespace arp {
enum class RequestType {
	REQUEST,
	PROBE,
	ANNNOUNCEMENT
};
}  // namespace arp
}  // namespace net

static struct t_arp s_arp_request ALIGNED ;
static struct t_arp s_arp_reply ALIGNED;
static net::arp::RequestType s_requestType ALIGNED;
static bool s_isProbeReplyReceived ALIGNED;
static uint32_t broadcast_ip ALIGNED;

extern struct ip_info g_ip_info;
extern uint8_t g_mac_address[ETH_ADDR_LEN];

typedef union pcast32 {
	uint32_t u32;
	uint8_t u8[4];
} _pcast32;

void __attribute__((cold)) arp_init() {
	broadcast_ip = g_ip_info.ip.addr | ~g_ip_info.netmask.addr;
	arp_cache_init();

	s_requestType = net::arp::RequestType::REQUEST;
	const auto nLocalIp = g_ip_info.ip.addr;

	// ARP Request template
	// Ethernet header
	memcpy(s_arp_request.ether.src, g_mac_address, ETH_ADDR_LEN);
	memset(s_arp_request.ether.dst, 0xFF , ETH_ADDR_LEN);
	s_arp_request.ether.type = __builtin_bswap16(ETHER_TYPE_ARP);

	// ARP Header
	s_arp_request.arp.hardware_type = __builtin_bswap16(ARP_HWTYPE_ETHERNET);
	s_arp_request.arp.protocol_type = __builtin_bswap16(ARP_PRTYPE_IPv4);
	s_arp_request.arp.hardware_size = ARP_HARDWARE_SIZE;
	s_arp_request.arp.protocol_size = ARP_PROTOCOL_SIZE;
	s_arp_request.arp.opcode = __builtin_bswap16(ARP_OPCODE_RQST);

	memcpy(s_arp_request.arp.sender_mac, g_mac_address, ETH_ADDR_LEN);
	s_arp_request.arp.sender_ip = nLocalIp;
	memset(s_arp_request.arp.target_mac, 0x00, ETH_ADDR_LEN);

	// ARP Reply Template
	// Ethernet header
	memcpy(s_arp_reply.ether.src, g_mac_address, ETH_ADDR_LEN);
	s_arp_reply.ether.type = __builtin_bswap16(ETHER_TYPE_ARP);

	// ARP Header
	s_arp_reply.arp.hardware_type = __builtin_bswap16(ARP_HWTYPE_ETHERNET);
	s_arp_reply.arp.protocol_type = __builtin_bswap16(ARP_PRTYPE_IPv4);
	s_arp_reply.arp.hardware_size = ARP_HARDWARE_SIZE;
	s_arp_reply.arp.protocol_size = ARP_PROTOCOL_SIZE;
	s_arp_reply.arp.opcode = __builtin_bswap16(ARP_OPCODE_REPLY);

	memcpy(s_arp_reply.arp.sender_mac, g_mac_address, ETH_ADDR_LEN);
	s_arp_reply.arp.sender_ip = nLocalIp;
}

bool arp_do_probe() {
	int32_t nTimeout;
	auto nRetries = 3;

	while (nRetries--) {
		arp_send_probe();

		nTimeout = 0x1FFFF;
#ifndef NDEBUG
		nTimeout+= 0x40000;
#endif

		while ((nTimeout-- > 0) && !s_isProbeReplyReceived) {
			net_handle();
		}

		if (s_isProbeReplyReceived) {
			return true;
		}
	}

	return false;
}

void arp_send_request(uint32_t nIp) {
	DEBUG_ENTRY
	DEBUG_PRINTF(IPSTR, IP2STR(nIp));

	s_requestType = net::arp::RequestType::REQUEST;
	s_arp_request.arp.target_ip = nIp;

	emac_eth_send(reinterpret_cast<void *>(&s_arp_request), sizeof(struct t_arp));

	DEBUG_EXIT
}

/*
 *  The Sender IP is set to all zeros,
 *  which means it cannot map to the Sender MAC address.
 *  The Target MAC address is all zeros,
 *  which means it cannot map to the Target IP address.
 */
void arp_send_probe() {
	DEBUG_ENTRY

	s_requestType = net::arp::RequestType::PROBE;
	s_isProbeReplyReceived = false;

	s_arp_request.arp.sender_ip = 0;

	arp_send_request(g_ip_info.ip.addr);

	s_arp_request.arp.sender_ip = g_ip_info.ip.addr;

	DEBUG_EXIT
}

/*
 * The packet structure is identical to the ARP Probe above,
 * with the exception that a complete mapping exists.
 * Both the Sender MAC address and the Sender IP address create a complete ARP mapping,
 * and hosts on the network can use this pair of addresses in their ARP table.
 */
void arp_send_announcement() {
	DEBUG_ENTRY

	s_requestType = net::arp::RequestType::ANNNOUNCEMENT;

	arp_send_request(g_ip_info.ip.addr);

	DEBUG_EXIT
}

void arp_handle_request(struct t_arp *p_arp) {
	DEBUG_ENTRY

	_pcast32 target;

	const auto *p = reinterpret_cast<uint8_t *>(&p_arp->arp.target_ip);

	memcpy(target.u8, p, IPv4_ADDR_LEN);

	DEBUG_PRINTF("Sender " IPSTR " Target " IPSTR, IP2STR(p_arp->arp.sender_ip), IP2STR(target.u32));

	if (!((target.u32 == g_ip_info.ip.addr) || (target.u32 == broadcast_ip))) {
		DEBUG_PUTS("No for me.");
		DEBUG_EXIT
		return;
	}

	// Ethernet header
	memcpy(s_arp_reply.ether.dst, p_arp->ether.src, ETH_ADDR_LEN);

	// ARP Header
	memcpy(s_arp_reply.arp.target_mac, p_arp->arp.sender_mac, ETH_ADDR_LEN);
	s_arp_reply.arp.target_ip = p_arp->arp.sender_ip;

	emac_eth_send(reinterpret_cast<void *>(&s_arp_reply), sizeof(struct t_arp));

	DEBUG_EXIT
}

void arp_handle_reply(struct t_arp *p_arp) {
	DEBUG_ENTRY

	switch (s_requestType) {
	case net::arp::RequestType::REQUEST:
		arp_cache_update(p_arp->arp.sender_mac, p_arp->arp.sender_ip);
		break;
	case net::arp::RequestType::PROBE:
		s_isProbeReplyReceived = true;
		break;
	default:
		assert(0);
		__builtin_unreachable();
		break;
	}

	DEBUG_EXIT
}

__attribute__((hot)) void arp_handle(struct t_arp *pArp) {
	DEBUG_ENTRY

	switch (pArp->arp.opcode) {
		case __builtin_bswap16(ARP_OPCODE_RQST):
			arp_handle_request(pArp);
			break;
		case __builtin_bswap16(ARP_OPCODE_REPLY):
			arp_handle_reply(pArp);
			break;
		default:
			DEBUG_PRINTF("opcode %04x not handled", __builtin_bswap16(pArp->arp.opcode));
			break;
	}

	DEBUG_EXIT
}
