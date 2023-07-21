/*
 * artnetnode.cpp
 */

#include <cstdint>

#include "artnetnode.h"
#include "artnet.h"

#include "e131bridge.h"

#if (ARTNET_VERSION < 4)
# error ARTNET_VERSION is not 4
#endif

void ArtNetNode::SetPortProtocol4(const uint32_t nPortIndex, const artnet::PortProtocol portProtocol) {
	DEBUG_PRINTF("nPortIndex=%u, PortProtocol=%s", nPortIndex, artnet::get_protocol_mode(portProtocol, false));

	if (nPortIndex >= artnetnode::MAX_PORTS) {
		DEBUG_EXIT
		return;
	}

	m_OutputPort[nPortIndex].genericPort.protocol = portProtocol;
	m_InputPort[nPortIndex].genericPort.protocol = portProtocol;

	if (portProtocol == artnet::PortProtocol::SACN) {
		m_OutputPort[nPortIndex].GoodOutput |= artnet::GoodOutput::OUTPUT_IS_SACN;
	} else {
		m_OutputPort[nPortIndex].GoodOutput &= static_cast<uint8_t>(~artnet::GoodOutput::OUTPUT_IS_SACN);
	}

	if (m_State.status == artnetnode::Status::ON) {
		if (m_pArtNetStore != nullptr) {
			m_pArtNetStore->SavePortProtocol(nPortIndex, portProtocol);
		}

		artnet::display_port_protocol(nPortIndex, portProtocol);
	}

	DEBUG_EXIT
}

artnet::PortProtocol ArtNetNode::GetPortProtocol4(const uint32_t nPortIndex) const {
	DEBUG_ENTRY
	assert(nPortIndex < artnetnode::MAX_PORTS);

	if (m_OutputPort[nPortIndex].genericPort.isEnabled) {
		DEBUG_EXIT
		return m_OutputPort[nPortIndex].genericPort.protocol;
	}

	if (m_InputPort[nPortIndex].genericPort.isEnabled) {
		DEBUG_EXIT
		return m_InputPort[nPortIndex].genericPort.protocol;
	}

	DEBUG_EXIT
	return artnet::PortProtocol::ARTNET;
}

void ArtNetNode::SetPort4(const uint32_t nPortIndex, const lightset::PortDir porDirection) {
	DEBUG_ENTRY

	uint16_t nUniverse;
	const bool isActive = ArtNetNode::GetPortAddress(nPortIndex, nUniverse, porDirection);
	const auto portProtocol = ArtNetNode::GetPortProtocol4(nPortIndex);

	DEBUG_PRINTF("Port %u, Active %c, Universe %d, Protocol %s [%s]", nPortIndex, isActive ? 'Y' : 'N', nUniverse, artnet::get_protocol_mode(portProtocol), porDirection == lightset::PortDir::OUTPUT ? "Output" : "Input");

	if ((isActive) && (portProtocol == artnet::PortProtocol::SACN)) {
		if (porDirection == lightset::PortDir::INPUT) {
			DEBUG_PUTS("Input is not supported");
			return;
		}

		if (IsMapUniverse0()) {
			nUniverse++;
		}

		if (nUniverse == 0) {
			return;
		}

		E131Bridge::SetUniverse(nPortIndex, porDirection, nUniverse);
	}

	DEBUG_EXIT
}

void ArtNetNode::SetPriority4(const uint32_t nPriority) {
	m_Node.AcnPriority = static_cast<uint8_t>(nPriority);

	for (uint32_t nPortIndex = 0; nPortIndex < e131bridge::MAX_PORTS; nPortIndex++) {
		E131Bridge::SetPriority(nPortIndex, static_cast<uint8_t>(nPriority));
	}

}

void ArtNetNode::SetLedBlinkMode4(hardware::ledblink::Mode mode) {
	E131Bridge::SetEnableDataIndicator(mode == hardware::ledblink::Mode::NORMAL);

	for (uint32_t nPortIndex = 0; nPortIndex < e131bridge::MAX_PORTS; nPortIndex++) {
		if (E131Bridge::IsTransmitting(nPortIndex)) {
			return;
		}
	}
	Hardware::Get()->SetMode(mode);
}

void ArtNetNode::HandleAddress4(const uint8_t nCommand, const uint32_t nPortIndex) {
	DEBUG_ENTRY
	DEBUG_PRINTF("artnetnode::PAGES=%u, nPortIndex=%u", artnetnode::PAGES, nPortIndex);

	for (uint32_t i = 0; i < artnetnode::MAX_PORTS; i++) {
		uint16_t nUniverse;
		const bool isActive = GetPortAddress(i, nUniverse, lightset::PortDir::OUTPUT);

		if (isActive) {
			if (IsMapUniverse0()) {
				nUniverse++;
			}

			if (nUniverse == 0) {
				continue;
			}

			if (GetPortProtocol4(i) == artnet::PortProtocol::SACN) {
				E131Bridge::SetUniverse(i, lightset::PortDir::OUTPUT, nUniverse);
			} else {
				E131Bridge::SetUniverse(i, lightset::PortDir::DISABLE, nUniverse);
			}
		}
	}

	switch (nCommand) {

	case artnet::PortCommand::LED_NORMAL:
		E131Bridge::SetEnableDataIndicator(true);
		break;
	case artnet::PortCommand::LED_MUTE:
		E131Bridge::SetEnableDataIndicator(false);
		break;
	case artnet::PortCommand::LED_LOCATE:
		E131Bridge::SetEnableDataIndicator(false);
		break;

	case artnet::PortCommand::MERGE_LTP_O:
	case artnet::PortCommand::MERGE_LTP_1:
	case artnet::PortCommand::MERGE_LTP_2:
	case artnet::PortCommand::MERGE_LTP_3:
		E131Bridge::SetMergeMode(nPortIndex, lightset::MergeMode::LTP);
		break;

	case artnet::PortCommand::MERGE_HTP_0:
	case artnet::PortCommand::MERGE_HTP_1:
	case artnet::PortCommand::MERGE_HTP_2:
	case artnet::PortCommand::MERGE_HTP_3:
		E131Bridge::SetMergeMode(nPortIndex, lightset::MergeMode::HTP);
		break;

	case artnet::PortCommand::CLR_0:
	case artnet::PortCommand::CLR_1:
	case artnet::PortCommand::CLR_2:
	case artnet::PortCommand::CLR_3:
		if (GetPortProtocol4(nPortIndex) == artnet::PortProtocol::SACN) {
			E131Bridge::Clear(nPortIndex);
		}
		break;

	default:
		break;
	}

	DEBUG_EXIT
}

uint8_t ArtNetNode::GetStatus4(const uint32_t nPortIndex) {
	assert(nPortIndex < e131bridge::MAX_PORTS);

	uint16_t nUniverse;
	const auto isActive = E131Bridge::GetUniverse(nPortIndex, nUniverse, lightset::PortDir::OUTPUT);

	DEBUG_PRINTF("Port %u, Active %c, Universe %d", nPortIndex, isActive ? 'Y' : 'N', nUniverse);

	if (isActive) {
		uint8_t nStatus = artnet::GoodOutput::OUTPUT_IS_SACN;
		nStatus = nStatus | (E131Bridge::IsTransmitting(nPortIndex) ? artnet::GoodOutput::DATA_IS_BEING_TRANSMITTED : artnet::GoodOutput::OUTPUT_NONE);
		nStatus = nStatus | (E131Bridge::IsMerging(nPortIndex) ? artnet::GoodOutput::OUTPUT_IS_MERGING : artnet::GoodOutput::OUTPUT_NONE);
		return nStatus;
	}

	return 0;
}
