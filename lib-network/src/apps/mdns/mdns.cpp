/**
 * @file mdns.cpp
 *
 */
/* Copyright (C) 2019-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <cstdio>
#include <algorithm>
#include <cassert>

#include "mdns.h"

#include "network.h"
#include "hardware.h"

#include "debug.h"

#define MDNS_TLD                ".local"
#define MDNS_RESPONSE_TTL     	(3600)    ///< (in seconds)
#define ANNOUNCE_TIMEOUT 		((MDNS_RESPONSE_TTL / 2) + (MDNS_RESPONSE_TTL / 4))

enum TDNSClasses {
	DNSClassInternet = 1
};

enum TDNSRecordTypes {
	DNSRecordTypeA = 1,		///< 0x01
	DNSRecordTypePTR = 12,	///< 0x0c
	DNSRecordTypeTXT = 16,	///< 0x10
	DNSRecordTypeSRV = 33	///< 0x21
};

enum TDNSCacheFlush {
	DNSCacheFlushTrue = 0x8000
};

enum TDNSOpCodes {
	DNSOpQuery = 0,
	DNSOpIQuery = 1,
	DNSOpStatus = 2,
	DNSOpNotify = 4,
	DNSOpUpdate = 5
};

namespace mdns {
#if !defined (MDNS_SERVICE_RECORDS_MAX)
static constexpr auto SERVICE_RECORDS_MAX = 8;
#else
static constexpr auto SERVICE_RECORDS_MAX = MDNS_SERVICE_RECORDS_MAX;
#endif

static constexpr uint32_t MULTICAST_ADDRESS = network::convert_to_uint(224, 0, 0, 251);
static constexpr uint16_t UDP_PORT = 5353;

static constexpr size_t DOMAIN_MAXLEN = 256;
static constexpr size_t LABEL_MAXLEN = 63;
static constexpr size_t TXT_MAXLEN = 256;

static constexpr char DOMAIN_LOCAL[]		= { 5 , 'l', 'o', 'c', 'a', 'l' , 0};
static constexpr uint8_t DOMAIN_DNSSD[]		= { 9 , '_','s','e','r','v','i','c','e','s', 7 ,'_','d','n','s','-','s','d', 4 , '_', 'u', 'd', 'p', 5 , 'l', 'o', 'c', 'a', 'l' , 0};
//static constexpr char DOMAIN_REVERSE[]		= { 7 , 'i','n','-','a','d','d','r', 4 , 'a','r','p','a', 5 , 'l', 'o', 'c', 'a', 'l' , 0};
static constexpr char DOMAIN_UDP[]			= { 4 , '_', 'u', 'd', 'p',};
static constexpr char DOMAIN_TCP[]			= { 4 , '_', 't', 'c', 'p' };
static constexpr char DOMAIN_CONFIG[]		= { 7 , '_','c','o','n','f','i','g'};
static constexpr char DOMAIN_TFTP[]			= { 5 , '_','t','f','t','p'};
static constexpr char DOMAIN_HTTP[]			= { 5 , '_','h','t','t','p'};
static constexpr char DOMAIN_RDMNET_LLRP[]	= { 12, '_','r','d','m','n','e','t','-','l','l','r','p'};
static constexpr char DOMAIN_NTP[]			= { 4 , '_','n','t','p'};
static constexpr char DOMAIN_MIDI[]			= { 11, '_','a','p','p','l','e','-','m','i','d','i'};
static constexpr char DOMAIN_OSC[]			= { 4 , '_','o','s','c'};
static constexpr char DOMAIN_DDP[]			= { 4 , '_','d','d','p'};
static constexpr char DOMAIN_PP[]			= { 3 , '_','p','p'};

enum class Protocols {
	UDP, TCP
};

struct Flags {
	uint32_t qr;
	uint32_t opcode;
	uint32_t aa;
	uint32_t tc;
	uint32_t rd;
	uint32_t ra;
	uint32_t zero;
	uint32_t ad;
	uint32_t cd;
	uint32_t rcode;
};

struct Header {
	uint16_t xid;
	uint16_t nFlags;
	uint16_t nQueryCount;
	uint16_t nAnswerCount;
	uint16_t nAuthorityCount;
	uint16_t nAdditionalCount;
} __attribute__((__packed__));

struct Service {
	const char *pDomain;
	const uint16_t nLength;
	const Protocols protocols;
	const uint16_t nPortDefault;
};

static constexpr Service s_Services[] {
		{DOMAIN_CONFIG		, sizeof(DOMAIN_CONFIG)		, Protocols::UDP, 0x2905 },
		{DOMAIN_TFTP		, sizeof(DOMAIN_TFTP)		, Protocols::UDP, 69 },
		{DOMAIN_HTTP		, sizeof(DOMAIN_HTTP)		, Protocols::TCP, 80 },
		{DOMAIN_RDMNET_LLRP	, sizeof(DOMAIN_RDMNET_LLRP), Protocols::UDP, 5569 },
		{DOMAIN_NTP			, sizeof(DOMAIN_NTP)		, Protocols::UDP, 123 },
		{DOMAIN_MIDI		, sizeof(DOMAIN_MIDI)		, Protocols::UDP, 5004 },
		{DOMAIN_OSC			, sizeof(DOMAIN_OSC)		, Protocols::UDP, 0 },
		{DOMAIN_DDP			, sizeof(DOMAIN_DDP)		, Protocols::UDP, 4048 },
		{DOMAIN_PP			, sizeof(DOMAIN_PP)			, Protocols::UDP, 5078 }
};

struct ServiceRecord {
	char *pName;
	char *pTextContent;
	uint16_t nTextContentLength;
	uint16_t nPort;
	mdns::Services services;
};

static ServiceRecord s_ServiceRecords[mdns::SERVICE_RECORDS_MAX];

static uint8_t s_RecordsData[512];

}  // namespace mdns

int32_t MDNS::s_nHandle;
uint32_t MDNS::s_nRemoteIp;
uint16_t MDNS::s_nRemotePort;
uint16_t MDNS::s_nBytesReceived;
uint32_t MDNS::s_nLastAnnounceMillis;
uint32_t MDNS::s_nDNSServiceRecords;
uint8_t *MDNS::s_pReceiveBuffer;

using namespace mdns;

static uint32_t add_label(uint8_t *pDestination, const char *pLabel, const size_t nLabelLength) {
	*pDestination = static_cast<uint8_t>(nLabelLength);
	pDestination++;

	memcpy(pDestination, pLabel, nLabelLength);
	return 1U + nLabelLength;
}

static uint32_t add_protocol(uint8_t *pDestination, const mdns::Protocols protocol) {
	if (protocol == mdns::Protocols::UDP) {
		memcpy(pDestination, DOMAIN_UDP, sizeof(DOMAIN_UDP));
		return sizeof(DOMAIN_UDP);
	}

	memcpy(pDestination, DOMAIN_TCP, sizeof(DOMAIN_TCP));
	return sizeof(DOMAIN_TCP);
}

static uint32_t add_dot_local(uint8_t *pDestination) {
	memcpy(pDestination, DOMAIN_LOCAL, sizeof(DOMAIN_LOCAL));
	return sizeof(DOMAIN_LOCAL);
}

static uint32_t add_dnssd(uint8_t *pDestination) {
	memcpy(pDestination, DOMAIN_DNSSD, sizeof(DOMAIN_DNSSD));
	return sizeof(DOMAIN_DNSSD);
}

static uint32_t create_service_domain(uint8_t *pDestination, ServiceRecord const& serviceRecord, const bool bIncludeName) {
	uint32_t nLength = 0;

	if (bIncludeName) {
		if (serviceRecord.pName != nullptr) {
			nLength = add_label(pDestination, serviceRecord.pName, strlen(serviceRecord.pName));
		} else {
			nLength = add_label(pDestination, Network::Get()->GetHostName(), strlen(Network::Get()->GetHostName()));
		}
	}

	const auto nIndex = static_cast<uint32_t>(serviceRecord.services);

	memcpy(&pDestination[nLength], s_Services[nIndex].pDomain, s_Services[nIndex].nLength);
	nLength += s_Services[nIndex].nLength;
	
	nLength += add_protocol(&pDestination[nLength], s_Services[nIndex].protocols); 
	nLength += add_dot_local(&pDestination[nLength]);

	return nLength;
}

static uint32_t create_host_domain(uint8_t *pDestination) {
	auto *pDst = pDestination;

	pDst += add_label(pDst, Network::Get()->GetHostName(), strlen(Network::Get()->GetHostName()));
	pDst += add_dot_local(pDst);

	return static_cast<uint32_t>(pDst - pDestination);
}

bool domain_compare(const uint8_t *pDomainA, const uint32_t nLengthA, const uint8_t *pDomainB, const uint32_t nLengthB) {
	if (nLengthA != nLengthB) {
		return false;
	}

	auto const *pNameA = pDomainA;
	auto const *pNameB = pDomainB;

	while(*pNameA && *pNameB && (pNameA < &pDomainA[nLengthA])) {
		if (*pNameA != *pNameB) {
			return false;
		}

		auto nLength = static_cast<size_t>(*pNameA);
		pNameA++;
		pNameB++;

		if (strncasecmp(reinterpret_cast<const char *>(pNameA), reinterpret_cast<const char *>(pNameB), nLength) != 0) {
			return false;
		}

		pNameA += nLength;
		pNameB += nLength;
	}

	return true;
}

static void domain_print(const uint8_t *pDomain, const uint32_t nLength) {
	auto const *pName = pDomain;

	while (*pName && (pName < &pDomain[nLength])) {
		auto nLength = static_cast<size_t>(*pName);
		pName++;
		printf("%.*s.", static_cast<int>(nLength), pName);
		pName += nLength;
	}
}

void service_domain_print(ServiceRecord const &serviceRecord) {
	auto *domain = s_RecordsData;
	const auto nLength = create_service_domain(domain, serviceRecord, false);
	domain_print(const_cast<const uint8_t *>(domain), nLength);
}

namespace network {
void mdns_announcement() {
	DEBUG_ENTRY

	assert(MDNS::Get() != nullptr);
	MDNS::Get()->SendAnnouncement();

	DEBUG_ENTRY
}
}  // namespace network

#ifndef NDEBUG
void Dump(const struct Header *pmDNSHeader, uint16_t nFlags) {
	mdns::Flags tmDNSFlags;

	tmDNSFlags.rcode = nFlags & 0xf;
	tmDNSFlags.cd = (nFlags >> 4) & 1;
	tmDNSFlags.ad = (nFlags >> 5) & 1;
	tmDNSFlags.zero = (nFlags) & 1;
	tmDNSFlags.ra = (nFlags >> 7) & 1;
	tmDNSFlags.rd = (nFlags >> 8) & 1;
	tmDNSFlags.tc = (nFlags >> 9) & 1;
	tmDNSFlags.aa = (nFlags >> 10) & 1;
	tmDNSFlags.opcode = (nFlags >> 14) & 0xf;
	tmDNSFlags.qr = (nFlags >> 15) & 1;

	const uint16_t nQuestions = __builtin_bswap16(pmDNSHeader->nQueryCount);
	const uint16_t nAnswers = __builtin_bswap16(pmDNSHeader->nAnswerCount);
	const uint16_t nAuthority = __builtin_bswap16(pmDNSHeader->nAuthorityCount);
	const uint16_t nAdditional = __builtin_bswap16(pmDNSHeader->nAdditionalCount);

	printf("ID: %u\n", pmDNSHeader->xid);
	printf("Flags: \n");
	printf("      QR: %u", tmDNSFlags.qr);
	printf("  OPCODE: %u", tmDNSFlags.opcode);
	printf("      AA: %u", tmDNSFlags.aa);
	printf("      TC: %u", tmDNSFlags.tc);
	printf("      RD: %u", tmDNSFlags.rd);
	printf("      RA: %u", tmDNSFlags.ra);
	printf("       Z: %u", tmDNSFlags.zero);
	printf("      AD: %u", tmDNSFlags.ad);
	printf("      CD: %u", tmDNSFlags.cd);
	printf("   RCODE: %u\n", tmDNSFlags.rcode);
	printf("Questions : %u\n", nQuestions);
	printf("Answers   : %u\n", nAnswers);
	printf("Authority : %u\n", nAuthority);
	printf("Additional: %u\n", nAdditional);

}
#endif

MDNS *MDNS::s_pThis;

MDNS::MDNS() {
	assert(s_pThis == nullptr);
	s_pThis = this;

	for (auto i = 0; i < SERVICE_RECORDS_MAX; i++) {
		s_ServiceRecords[i].services = Services::LAST_NOT_USED;
	}

	s_nHandle = Network::Get()->Begin(mdns::UDP_PORT);
	assert(s_nHandle != -1);

	Network::Get()->JoinGroup(s_nHandle, mdns::MULTICAST_ADDRESS);
	Network::Get()->SetDomainName(&MDNS_TLD[1]);

	SendAnnouncement();
}

MDNS::~MDNS() {
	for (uint32_t i = 0; i < mdns::SERVICE_RECORDS_MAX; i++) {
		if (s_ServiceRecords[i].pName != nullptr) {
			delete[] s_ServiceRecords[i].pName;
		}

		if (s_ServiceRecords[i].pTextContent != nullptr) {
			delete[] s_ServiceRecords[i].pTextContent;
		}
	}

	Network::Get()->End(mdns::UDP_PORT);
	s_nHandle = -1;
}

void MDNS::SendAnnouncement() {
	DEBUG_ENTRY

	SendAnswerLocalIpAddress();

	for (uint32_t nIndex = 0; nIndex < mdns::SERVICE_RECORDS_MAX; nIndex++) {
		if (s_ServiceRecords[nIndex].services < Services::LAST_NOT_USED) {
			SendMessage(nIndex);
		}
	}

	DEBUG_EXIT
}

bool MDNS::AddServiceRecord(const char *pName, const mdns::Services services, const char *pTextContent, const uint16_t nPort) {
	DEBUG_ENTRY
	assert(services < mdns::Services::LAST_NOT_USED);
	uint32_t i;

	for (i = 0; i < mdns::SERVICE_RECORDS_MAX; i++) {
		if (s_ServiceRecords[i].services == Services::LAST_NOT_USED) {
			if (pName != nullptr) {
				const auto nLength = std::min(LABEL_MAXLEN, strlen(pName));
				if (nLength == 0) {
					assert(0);
					return false;
				}

				s_ServiceRecords[i].pName = new char[1 +  nLength];

				assert(s_ServiceRecords[i].pName != nullptr);
				memcpy(s_ServiceRecords[i].pName, pName, nLength);
				s_ServiceRecords[i].pName[nLength] = '\0';
			}

			s_ServiceRecords[i].services = services;

			if (nPort == 0) {
				s_ServiceRecords[i].nPort = __builtin_bswap16(s_Services[static_cast<uint32_t>(services)].nPortDefault);
			} else {
				s_ServiceRecords[i].nPort = __builtin_bswap16(nPort);
			}

			if (pTextContent != nullptr) {
				const auto nLength = std::min(TXT_MAXLEN, strlen(pTextContent));
				s_ServiceRecords[i].pTextContent = new char[nLength];

				assert(s_ServiceRecords[i].pTextContent != nullptr);
				memcpy(s_ServiceRecords[i].pTextContent, pTextContent, nLength);

				s_ServiceRecords[i].nTextContentLength = static_cast<uint16_t>(nLength);
			}

			break;
		}
	}

	if (i == mdns::SERVICE_RECORDS_MAX) {
		assert(0);
		return false;
	}

	SendMessage(i);
	return true;
}

void MDNS::SendAnswerLocalIpAddress() {
	DEBUG_ENTRY

	auto *pHeader = reinterpret_cast<Header*>(&s_RecordsData);

	pHeader->nFlags = __builtin_bswap16(0x8400);
	pHeader->nQueryCount = 0;
	pHeader->nAnswerCount = __builtin_bswap16(1);
	pHeader->nAuthorityCount = 0;
	pHeader->nAdditionalCount = 0;

	uint8_t *pDst = reinterpret_cast<uint8_t *>(&s_RecordsData) + sizeof(struct Header);

	pDst += CreateAnswerLocalIpAddress(pDst);

	const auto nSize = static_cast<uint16_t>(pDst - reinterpret_cast<uint8_t *>(pHeader));

	Network::Get()->SendTo(s_nHandle, s_RecordsData, nSize, mdns::MULTICAST_ADDRESS, mdns::UDP_PORT);

	DEBUG_EXIT
}

void MDNS::SendMessage(uint32_t nIndex) {
	DEBUG_ENTRY
	DEBUG_PRINTF("nIndex=%u", nIndex);

	auto *pHeader = reinterpret_cast<Header *>(&s_RecordsData);

	pHeader->nFlags = __builtin_bswap16(0x8400);
	pHeader->nQueryCount = 0;
	pHeader->nAnswerCount = __builtin_bswap16(4);
	pHeader->nAuthorityCount = __builtin_bswap16(1);
	pHeader->nAdditionalCount = __builtin_bswap16(0);

	auto *pDst = reinterpret_cast<uint8_t *>(&s_RecordsData) + sizeof(struct Header);

	pDst += CreateAnswerServiceSrv(nIndex, pDst);
	pDst += CreateAnswerServiceTxt(nIndex, pDst);
	pDst += CreateAnswerServiceDnsSd(nIndex, pDst);
	pDst += CreateAnswerServicePtr(nIndex, pDst);
	pDst += CreateAnswerLocalIpAddress(pDst);

	const auto nSize = static_cast<uint16_t>(pDst - reinterpret_cast<uint8_t*>(pHeader));

	Network::Get()->SendTo(s_nHandle, &s_RecordsData, nSize, mdns::MULTICAST_ADDRESS, mdns::UDP_PORT);

	DEBUG_EXIT
}

uint32_t MDNS::CreateAnswerServiceSrv(uint32_t nIndex, uint8_t *pDestination) {
	DEBUG_ENTRY

	auto *pDst = pDestination;

	pDst += create_service_domain(pDst, s_ServiceRecords[nIndex], true);

	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSRecordTypeSRV);
	pDst += 2;
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSCacheFlushTrue | DNSClassInternet);
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = __builtin_bswap32(MDNS_RESPONSE_TTL);
	pDst += 4;
	auto *lengtPointer = pDst;
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = 0; // Priority and Weight
	pDst += 4;
	*reinterpret_cast<uint16_t*>(pDst) = s_ServiceRecords[nIndex].nPort;
	pDst += 2;
	auto *pBegin = pDst;
	pDst += create_host_domain(pDst);

	*reinterpret_cast<uint16_t*>(lengtPointer) = __builtin_bswap16(static_cast<uint16_t>(6U + pDst - pBegin));

	DEBUG_EXIT
	return static_cast<uint32_t>(pDst - pDestination);
}

uint32_t MDNS::CreateAnswerServiceTxt(uint32_t nIndex, uint8_t *pDestination) {
	DEBUG_ENTRY

	auto *pDst = pDestination;

	pDst += create_service_domain(pDst, s_ServiceRecords[nIndex], true);

	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSRecordTypeTXT);
	pDst += 2;
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSCacheFlushTrue | DNSClassInternet);
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = __builtin_bswap32(MDNS_RESPONSE_TTL);
	pDst += 4;

	if (s_ServiceRecords[nIndex].pTextContent == nullptr) {
		*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(0x0001);	// Data length
		pDst += 2;
		*pDst = 0;														// Text length
		pDst++;
	} else {
		const auto nSize = s_ServiceRecords[nIndex].nTextContentLength;
		*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(static_cast<uint16_t>(1U + nSize));	// Data length
		pDst += 2;
		*pDst = static_cast<uint8_t>(nSize);														// Text length
		pDst++;
		strcpy(reinterpret_cast<char*>(pDst), s_ServiceRecords[nIndex].pTextContent);
		pDst += nSize;
	}

	DEBUG_EXIT
	return static_cast<uint32_t>(pDst - pDestination);
}

uint32_t MDNS::CreateAnswerServicePtr(uint32_t nIndex, uint8_t *pDestination) {
	DEBUG_ENTRY

	auto *pDst = pDestination;

	pDst += create_service_domain(pDst, s_ServiceRecords[nIndex], false);
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSRecordTypePTR);
	pDst += 2;
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSClassInternet);
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = __builtin_bswap32(MDNS_RESPONSE_TTL);
	pDst += 4;
	auto *lengtPointer = pDst;
	pDst += 2;
	auto *pBegin = pDst;
	pDst += create_service_domain(pDst, s_ServiceRecords[nIndex], true);

	*reinterpret_cast<uint16_t*>(lengtPointer) = __builtin_bswap16(static_cast<uint16_t>(pDst - pBegin));

	DEBUG_EXIT
	return static_cast<uint32_t>(pDst - pDestination);
}

uint32_t MDNS::CreateAnswerLocalIpAddress(uint8_t *pDestination) {
	auto *pDst = pDestination;

	pDst += create_host_domain(pDst);

	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSRecordTypeA);
	pDst += 2;
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSCacheFlushTrue | DNSClassInternet);
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = __builtin_bswap32(MDNS_RESPONSE_TTL);
	pDst += 4;
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(4);	// Data length
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = Network::Get()->GetIp();
	pDst += 4;

	return static_cast<uint32_t>(pDst - pDestination);
}

uint32_t MDNS::CreateAnswerServiceDnsSd(uint32_t nIndex, uint8_t *pDestination) {
	DEBUG_ENTRY

	auto *pDst = pDestination;

	pDst += add_dnssd(pDst);

	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSRecordTypePTR);
	pDst += 2;
	*reinterpret_cast<uint16_t*>(pDst) = __builtin_bswap16(DNSClassInternet);
	pDst += 2;
	*reinterpret_cast<uint32_t*>(pDst) = __builtin_bswap32(MDNS_RESPONSE_TTL);
	pDst += 4;
	auto *lengtPointer = pDst;
	pDst += 2;
	auto *pBegin = pDst;
	pDst += create_service_domain(pDst, s_ServiceRecords[nIndex], false);

	*reinterpret_cast<uint16_t*>(lengtPointer) = __builtin_bswap16(static_cast<uint16_t>(pDst - pBegin));

	DEBUG_EXIT
	return static_cast<uint32_t>(pDst - pDestination);
}

/*
 * https://opensource.apple.com/source/mDNSResponder/mDNSResponder-26.2/mDNSCore/mDNS.c.auto.html
 * mDNSlocal const mDNSu8 *getDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end, domainname *const name)
 *
 * Routine to fetch an FQDN from the DNS message, following compression pointers if necessary.
 */
const uint8_t *get_domain_name(const uint8_t *const msg, const uint8_t *ptr, const uint8_t *const end, uint8_t *const name) {
	const uint8_t *nextbyte = nullptr;					// Record where we got to before we started following pointers
	uint8_t *np = name;									// Name pointer
	const uint8_t *const limit = np + DOMAIN_MAXLEN;	// Limit so we don't overrun buffer

	if (ptr < reinterpret_cast<const uint8_t*>(msg) || ptr >= end) {
		DEBUG_PUTS("getDomainName: Illegal ptr not within packet boundaries");
		return nullptr;
	}

	*np = 0;// Tentatively place the root label here (may be overwritten if we have more labels)

	while (1)						// Read sequence of labels
	{
		const auto len = *ptr++;	// Read length of this label
		if (len == 0)
			break;		// If length is zero, that means this name is complete
		switch (len & 0xC0) {
		int i;
		uint16_t offset;

	case 0x00:
		if (ptr + len >= end)// Remember: expect at least one more byte for the root label
				{
			DEBUG_PUTS("getDomainName: Malformed domain name (overruns packet end)");
			return nullptr;
		}
		if (np + 1 + len >= limit)// Remember: expect at least one more byte for the root label
				{
			DEBUG_PUTS("getDomainName: Malformed domain name (more than 255 characters)");
			return nullptr;
		}
		*np++ = len;
		for (i = 0; i < len; i++)
			*np++ = *ptr++;
		*np = 0;// Tentatively place the root label here (may be overwritten if we have more labels)
		break;

	case 0x40:
		DEBUG_PRINTF("getDomainName: Extended EDNS0 label types 0x40 not supported in name %.*s", static_cast<int>(len), name);
		return nullptr;

	case 0x80:
		DEBUG_PRINTF("getDomainName: Illegal label length 0x80 in domain name %.*s", static_cast<int>(len), name);
		return nullptr;

	case 0xC0:
		offset = static_cast<uint16_t>(((static_cast<uint16_t>(len & 0x3F)) << 8) | *ptr++);
		if (!nextbyte)
			nextbyte = ptr;	// Record where we got to before we started following pointers
		ptr = reinterpret_cast<const uint8_t *>(msg) + offset;
		if (ptr < reinterpret_cast<const uint8_t *>(msg) || ptr >= end) {
			DEBUG_PUTS("getDomainName: Illegal compression pointer not within packet boundaries");
			return nullptr;
		}
		if (*ptr & 0xC0) {
			DEBUG_PUTS("getDomainName: Compression pointer must point to real label");
			return nullptr;
		}
		break;
		}
	}

	if (nextbyte)
		return (nextbyte);
	else
		return (ptr);
}

void MDNS::HandleRequest(uint16_t nQuestions) {
	DEBUG_ENTRY

	uint8_t domain[mdns::DOMAIN_MAXLEN];
	uint8_t domainHost[mdns::DOMAIN_MAXLEN];
	const auto nDomainHostLength = create_host_domain(domainHost);
	uint32_t nOffset = sizeof(struct Header);

	DEBUG_PRINTF("nQuestions=%u", nQuestions);

	for (uint32_t i = 0; i < nQuestions; i++) {
		auto *pResult = get_domain_name(s_pReceiveBuffer, &s_pReceiveBuffer[nOffset], &s_pReceiveBuffer[s_nBytesReceived], domain);
		if (pResult == nullptr) {
			DEBUG_EXIT
			return;
		}

		const auto nDomainLength = static_cast<uint32_t>(pResult - &s_pReceiveBuffer[nOffset]);
		nOffset += nDomainLength;

		const auto nType = __builtin_bswap16(*reinterpret_cast<uint16_t*>(&s_pReceiveBuffer[nOffset]));
		nOffset += 2;

		const auto nClass = __builtin_bswap16(*reinterpret_cast<uint16_t*>(&s_pReceiveBuffer[nOffset])) & 0x7F;
		nOffset += 2;

#ifndef NDEBUG
		domain_print(domain, nDomainLength)
		printf(" ==> Type : %d, Class: %d\n", nType, nClass);
#endif

		if (nClass == DNSClassInternet) {
			const auto isEqual = domain_compare(domainHost, nDomainHostLength, domain, nDomainLength);

			if (isEqual && (nType == DNSRecordTypeA)) {
				SendAnswerLocalIpAddress();
			}

			const auto isDnsDs = domain_compare(DOMAIN_DNSSD, sizeof(DOMAIN_DNSSD), domain, nDomainLength);
			DEBUG_PRINTF("isDnsDs=%u", isDnsDs);

			for (uint32_t i = 0; i < mdns::SERVICE_RECORDS_MAX; i++) {
				if (s_ServiceRecords[i].services < Services::LAST_NOT_USED) {
					uint8_t domainService[mdns::DOMAIN_MAXLEN];
					const auto nLengthDomainService = create_service_domain(domainService, s_ServiceRecords[i], false);
					const auto isEqual = domain_compare(domainService, nLengthDomainService, domain, nDomainLength);

					DEBUG_PRINTF("%d:%d",isEqual, nType == DNSRecordTypePTR);

					if ((isDnsDs || (isEqual && (nType == DNSRecordTypePTR))) ) {
						SendMessage(i);
					}
				}
			}
		}

	}

	DEBUG_EXIT
}

void MDNS::Parse() {
	auto *pHeader = reinterpret_cast<Header*>(s_pReceiveBuffer);
	const auto nFlags = __builtin_bswap16(pHeader->nFlags);

	if ((((nFlags >> 15) & 1) == 0) && (((nFlags >> 14) & 0xf) == DNSOpQuery)) {
		if (pHeader->nQueryCount != 0) {
			HandleRequest(__builtin_bswap16(pHeader->nQueryCount));
		}
	}
}

void MDNS::Run() {
#if 0
	 uint32_t nNow = Hardware::Get()->Millis();
#endif

	s_nBytesReceived = Network::Get()->RecvFrom(s_nHandle, const_cast<const void **>(reinterpret_cast<void **>(&s_pReceiveBuffer)), &s_nRemoteIp, &s_nRemotePort);

	if ((s_nRemotePort == mdns::UDP_PORT) && (s_nBytesReceived > sizeof(struct Header))) {
		Parse();
	}

#if 0
	if (__builtin_expect(((nNow - s_nLastAnnounceMillis) > 1000 * ANNOUNCE_TIMEOUT), 0)) {
		DEBUG_PUTS("> Announce <");
		for (uint32_t i = 0; i < s_nDNSServiceRecords; i++) {
			if (s_ServiceRecords[i].pName != 0) {
				//Network::Get()->SendTo(m_nHandle, (uint8_t *)&m_aServiceRecordsData[i].aBuffer, m_aServiceRecordsData[i].nSize, m_nMulticastIp, UDP_PORT);
			}
		}

		s_nLastAnnounceMillis = nNow;
	}
#endif
}

void MDNS::Print() {
	printf("mDNS\n");
	if (s_nHandle == -1) {
		printf(" Not running\n");
		return;
	}

	const auto nLength = create_host_domain(s_RecordsData);
	putchar(' ');domain_print(s_RecordsData, nLength);putchar('\n');

	for (uint32_t i = 0; i < mdns::SERVICE_RECORDS_MAX; i++) {
		if (s_ServiceRecords[i].services < Services::LAST_NOT_USED) {
			putchar(' ');service_domain_print(s_ServiceRecords[i]);
			printf(" %d %.*s\n", __builtin_bswap16(s_ServiceRecords[i].nPort), s_ServiceRecords[i].nTextContentLength, s_ServiceRecords[i].pTextContent == nullptr ? "" : s_ServiceRecords[i].pTextContent);
		}
	}
}
