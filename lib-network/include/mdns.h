/**
 * @file mdns.h
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

#ifndef MDNS_H_
#define MDNS_H_

#include <cstdint>

#include "network.h"

#include "../config/apps_config.h"

namespace mdns {
enum class Services {
	CONFIG, TFTP, HTTP, RDMNET_LLRP, NTP, MIDI, OSC, DDP, PP, LAST_NOT_USED
};
}  // namespace mdns

class MDNS {
public:
	MDNS();
	~MDNS();

	bool AddServiceRecord(const char *pName, const mdns::Services service, const char *pTextContent = nullptr, const uint16_t nPort = 0);

	void Print();
	void SendAnnouncement();
	void Run();

	static MDNS *Get() {
		return s_pThis;
	}

private:
	void Parse();
	void HandleRequest(uint16_t nQuestions);

	uint32_t CreateAnswerLocalIpAddress(uint8_t *pDestination);
	uint32_t CreateAnswerServiceSrv(uint32_t nIndex, uint8_t *pDestination);
	uint32_t CreateAnswerServiceTxt(uint32_t nIndex, uint8_t *pDestination);
	uint32_t CreateAnswerServicePtr(uint32_t nIndex, uint8_t *pDestination);
	uint32_t CreateAnswerServiceDnsSd(uint32_t nIndex, uint8_t *pDestination);

	void SendAnswerLocalIpAddress();
	void SendMessage(uint32_t nIndex);

private:
	static int32_t s_nHandle;
	static uint32_t s_nRemoteIp;
	static uint16_t s_nRemotePort;
	static uint16_t s_nBytesReceived;
	static uint32_t s_nLastAnnounceMillis;
	static uint32_t s_nDNSServiceRecords;
	static uint8_t *s_pReceiveBuffer;

	static MDNS *s_pThis;
};

#endif /* MDNS_H_ */
