/*
 * setrdm.cpp
 */

#include <cstdint>

#include "artnetnode.h"
#include "artnetrdmcontroller.h"

#include "debug.h"

void ArtNetNode::SetRdmController(ArtNetRdmController *pArtNetRdmController) {
	DEBUG_ENTRY

	m_pArtNetRdmController = pArtNetRdmController;
	m_State.rdm.IsEnabled = (pArtNetRdmController != nullptr);

	if (m_State.rdm.IsEnabled) {
		m_Node.IsRdmResponder = false;
		m_ArtPollReply.Status1 |= artnet::Status1::RDM_CAPABLE;
	} else {
		m_ArtPollReply.Status1 &= static_cast<uint8_t>(~artnet::Status1::RDM_CAPABLE);
	}

	DEBUG_EXIT
}
