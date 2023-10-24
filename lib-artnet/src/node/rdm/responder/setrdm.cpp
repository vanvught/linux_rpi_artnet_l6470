/*
 * setrdm.cpp
 */

#include <cstdint>

#include "artnetnode.h"
#include "artnetrdmresponder.h"

#include "debug.h"

void ArtNetNode::SetRdmResponder(ArtNetRdmResponder *pArtNetRdmResponder) {
	DEBUG_ENTRY

	m_pArtNetRdmResponder = pArtNetRdmResponder;
	m_State.rdm.IsEnabled = (pArtNetRdmResponder != nullptr);

	if (m_State.rdm.IsEnabled) {
		m_Node.IsRdmResponder = true;
		m_ArtPollReply.Status1 |= artnet::Status1::RDM_CAPABLE;
	} else {
		m_ArtPollReply.Status1 &= static_cast<uint8_t>(~artnet::Status1::RDM_CAPABLE);
	}

	DEBUG_EXIT
}
