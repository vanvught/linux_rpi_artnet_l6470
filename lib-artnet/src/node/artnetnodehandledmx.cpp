/**
 * @file artnetnodehandledmx.cpp
 *
 */
/**
 * Art-Net Designed by and Copyright Artistic Licence Holdings Ltd.
 */
/* Copyright (C) 2021-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <algorithm>
#include <cassert>

#include "artnetnode.h"
#include "artnet.h"

#include "lightsetdata.h"

void ArtNetNode::UpdateMergeStatus(const uint32_t nPortIndex) {
	if (!m_State.IsMergeMode) {
		m_State.IsMergeMode = true;
		m_State.IsChanged = true;
	}

	m_OutputPort[nPortIndex].GoodOutput |= artnet::GoodOutput::OUTPUT_IS_MERGING;
}

void ArtNetNode::CheckMergeTimeouts(const uint32_t nPortIndex) {
	const auto nTimeOutAMillis = m_nCurrentPacketMillis - m_OutputPort[nPortIndex].sourceA.nMillis;

	if (nTimeOutAMillis > (artnet::MERGE_TIMEOUT_SECONDS * 1000U)) {
		m_OutputPort[nPortIndex].sourceA.nIp = 0;
		m_OutputPort[nPortIndex].GoodOutput &= static_cast<uint8_t>(~artnet::GoodOutput::OUTPUT_IS_MERGING);
	}

	const auto nTimeOutBMillis = m_nCurrentPacketMillis - m_OutputPort[nPortIndex].sourceB.nMillis;

	if (nTimeOutBMillis > (artnet::MERGE_TIMEOUT_SECONDS * 1000U)) {
		m_OutputPort[nPortIndex].sourceB.nIp = 0;
		m_OutputPort[nPortIndex].GoodOutput &= static_cast<uint8_t>(~artnet::GoodOutput::OUTPUT_IS_MERGING);
	}

	auto bIsMerging = false;

	for (uint32_t i = 0; i < artnetnode::MAX_PORTS; i++) {
		bIsMerging |= ((m_OutputPort[i].GoodOutput & artnet::GoodOutput::OUTPUT_IS_MERGING) != 0);
	}

	if (!bIsMerging) {
		m_State.IsChanged = true;
		m_State.IsMergeMode = false;
#if defined ( ARTNET_ENABLE_SENDDIAG )
		SendDiag("Leaving Merging Mode", artnet::PriorityCodes::DP_LOW);
#endif
	}
}

void ArtNetNode::HandleDmx() {
	const auto *const pArtDmx = reinterpret_cast<TArtDmx *>(m_pReceiveBuffer);

	auto nDmxSlots = static_cast<uint16_t>( ((pArtDmx->LengthHi << 8) & 0xff00) | pArtDmx->Length);
	nDmxSlots = std::min(nDmxSlots, artnet::DMX_LENGTH);

	for (uint32_t nPortIndex = 0; nPortIndex < artnetnode::MAX_PORTS; nPortIndex++) {
		if ((m_Node.Port[nPortIndex].direction == lightset::PortDir::OUTPUT)
		 && (m_Node.Port[nPortIndex].protocol == artnet::PortProtocol::ARTNET)
		 && (m_Node.Port[nPortIndex].PortAddress == pArtDmx->PortAddress)) {

			const auto ipA = m_OutputPort[nPortIndex].sourceA.nIp;
			const auto ipB = m_OutputPort[nPortIndex].sourceB.nIp;

			m_OutputPort[nPortIndex].GoodOutput |= artnet::GoodOutput::DATA_IS_BEING_TRANSMITTED;

			if (m_State.IsMergeMode) {
				if (__builtin_expect((!m_State.bDisableMergeTimeout), 1)) {
					CheckMergeTimeouts(nPortIndex);
				}
			}

			const auto mergeMode = ((m_OutputPort[nPortIndex].GoodOutput & artnet::GoodOutput::MERGE_MODE_LTP) == artnet::GoodOutput::MERGE_MODE_LTP) ? lightset::MergeMode::LTP : lightset::MergeMode::HTP;

			if (ipA == 0 && ipB == 0) {
				m_OutputPort[nPortIndex].sourceA.nIp = m_nIpAddressFrom;
				m_OutputPort[nPortIndex].sourceA.nMillis = m_nCurrentPacketMillis;
				lightset::Data::SetSourceA(nPortIndex, pArtDmx->Data, nDmxSlots);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("1. first packet recv on this port", artnet::PriorityCodes::DP_LOW);
#endif
			} else if (ipA == m_nIpAddressFrom && ipB == 0) {
				m_OutputPort[nPortIndex].sourceA.nMillis = m_nCurrentPacketMillis;
				lightset::Data::SetSourceA(nPortIndex, pArtDmx->Data, nDmxSlots);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("2. continued transmission from the same ip (source A)", artnet::PriorityCodes::DP_LOW);
#endif
			} else if (ipA == 0 && ipB == m_nIpAddressFrom) {
				m_OutputPort[nPortIndex].sourceB.nMillis = m_nCurrentPacketMillis;
				lightset::Data::SetSourceB(nPortIndex, pArtDmx->Data, nDmxSlots);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("3. continued transmission from the same ip (source B)", artnet::PriorityCodes::DP_LOW);
#endif
			} else if (ipA != m_nIpAddressFrom && ipB == 0) {
				m_OutputPort[nPortIndex].sourceB.nIp = m_nIpAddressFrom;
				m_OutputPort[nPortIndex].sourceB.nMillis = m_nCurrentPacketMillis;
				UpdateMergeStatus(nPortIndex);
				lightset::Data::MergeSourceB(nPortIndex, pArtDmx->Data, nDmxSlots, mergeMode);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("4. new source, start the merge", artnet::PriorityCodes::DP_LOW);
#endif
			} else if (ipA == 0 && ipB != m_nIpAddressFrom) {
				m_OutputPort[nPortIndex].sourceA.nIp = m_nIpAddressFrom;
				m_OutputPort[nPortIndex].sourceA.nMillis = m_nCurrentPacketMillis;
				UpdateMergeStatus(nPortIndex);
				lightset::Data::MergeSourceA(nPortIndex, pArtDmx->Data, nDmxSlots, mergeMode);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("5. new source, start the merge", artnet::PriorityCodes::DP_LOW);
#endif
			} else if (ipA == m_nIpAddressFrom && ipB != m_nIpAddressFrom) {
				m_OutputPort[nPortIndex].sourceA.nMillis = m_nCurrentPacketMillis;
				UpdateMergeStatus(nPortIndex);
				lightset::Data::MergeSourceA(nPortIndex, pArtDmx->Data, nDmxSlots, mergeMode);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("6. continue merge", artnet::PriorityCodes::DP_LOW);
#endif
			} else if (ipA != m_nIpAddressFrom && ipB == m_nIpAddressFrom) {
				m_OutputPort[nPortIndex].sourceB.nMillis = m_nCurrentPacketMillis;
				UpdateMergeStatus(nPortIndex);
				lightset::Data::MergeSourceB(nPortIndex, pArtDmx->Data, nDmxSlots, mergeMode);
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("7. continue merge", artnet::PriorityCodes::DP_LOW);
#endif
			}
#ifndef NDEBUG
			else if (ipA == m_nIpAddressFrom && ipB == m_nIpAddressFrom) {
# if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("8. Source matches both buffers, this shouldn't be happening!", artnet::PriorityCodes::DP_LOW);
# endif
				puts("ERROR: 8. Source matches both buffers, this shouldn't be happening!");
				return;
			} else if (ipA != m_nIpAddressFrom && ipB != m_nIpAddressFrom) {
# if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("9. More than two sources, discarding data", artnet::PriorityCodes::DP_LOW);
# endif
				puts("WARN: 9. More than two sources, discarding data");
				return;
			}
#endif
			else {
#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("0. No cases matched, this shouldn't happen!", artnet::PriorityCodes::DP_LOW);
#endif
#ifndef NDEBUG
				puts("ERROR: 0. No cases matched, this shouldn't happen!");
#endif
				return;
			}

			if ((m_State.IsSynchronousMode) && ((m_OutputPort[nPortIndex].GoodOutput & artnet::GoodOutput::OUTPUT_IS_MERGING) != artnet::GoodOutput::OUTPUT_IS_MERGING)) {
				lightset::Data::Set(m_pLightSet, nPortIndex);
				m_OutputPort[nPortIndex].IsDataPending = true;

#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("Buffering data", artnet::PriorityCodes::DP_LOW);
#endif
			} else {
				lightset::Data::Output(m_pLightSet, nPortIndex);

				if (!m_OutputPort[nPortIndex].IsTransmitting) {
					m_pLightSet->Start(nPortIndex);
					m_State.IsChanged = true;
					m_OutputPort[nPortIndex].IsTransmitting = true;
				}

#if defined ( ARTNET_ENABLE_SENDDIAG )
				SendDiag("Send data", artnet::PriorityCodes::DP_LOW);
#endif
			}

			m_State.nReceivingDmx |= (1U << static_cast<uint8_t>(lightset::PortDir::OUTPUT));
		}
	}
}
