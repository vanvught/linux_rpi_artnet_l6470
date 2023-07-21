/**
 * @file artnetparams.h
 *
 */
/**
 * Art-Net Designed by and Copyright Artistic Licence Holdings Ltd.
 */
/* Copyright (C) 2016-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"); to deal
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

#ifndef ARTNETPARAMS_H_
#define ARTNETPARAMS_H_

#include <cstdint>
#include <climits>
#include <cassert>

#include "artnet.h"

#include "lightset.h"

#if !defined(LIGHTSET_PORTS)
# define LIGHTSET_PORTS	0
#endif

#if !defined(ARTNET_PAGE_SIZE)
# define ARTNET_PAGE_SIZE	4
#endif

namespace artnetparams {
static constexpr uint16_t clear_mask(const uint32_t i) {
	return static_cast<uint16_t>(~((1U << (i + 8)) | (1U << i)));
}

static constexpr uint16_t shift_left(const uint32_t nValue, const uint32_t i) {
	return static_cast<uint16_t>((nValue & 0x1) << i);
}

static constexpr uint16_t portdir_shif_right(const uint32_t nValue, const uint32_t i) {
	return static_cast<uint16_t>((nValue >> (i * 2)) & 0x3);
}

#if LIGHTSET_PORTS > 4
 static constexpr uint32_t MAX_PORTS = artnet::PORTS;
#else
# if (LIGHTSET_PORTS == 0)
   static constexpr uint32_t MAX_PORTS = 1;	// ISO C++ forbids zero-size array
# else
   static constexpr uint32_t MAX_PORTS = LIGHTSET_PORTS;
# endif
#endif

struct Params {
	uint32_t nSetList;									///< 4	  4
#if (ARTNET_PAGE_SIZE == 4)
	uint8_t  nNet;										///< 1	  5
	uint8_t  nSubnet;									///< 1	  6
#else
	uint8_t  NotUsed2;									///< 1    5
	uint8_t  NotUsed1;									///< 1    6
#endif
	uint8_t  nFailSafe;									///< 1	  7
	uint8_t  nOutputType;								///< 1	  8
	uint16_t nRdm;										///< 2	 10
	uint8_t  NotUsed6;									///< 1	 11
	uint8_t  NotUsed5;									///< 1	 12
	uint8_t  aShortName[artnet::SHORT_NAME_LENGTH];		///< 18	 30
	uint8_t  aLongName[artnet::LONG_NAME_LENGTH];		///< 64	 94
	uint16_t nMultiPortOptions;							///< 2	 96
	uint8_t  NotUsed0[2];								///< 2	 98
	uint8_t  NotUsed7;									///< 1	 99
#if (ARTNET_PAGE_SIZE == 4)
	uint8_t  NotUsed4;									///< 1	100
	uint8_t  NotUsed3;									///< 1	101
	uint8_t  NotUsed2;									///< 1	102
	uint8_t  NotUsed1;									///< 1	103
	uint8_t  nUniversePort[artnet::PORTS];				///< 4	107
#else
	uint16_t nUniverse[artnet::PORTS];					///< 8
#endif
	uint8_t  nsACNPriority;								///< 1	108
	uint8_t  nMergeModePort[artnet::PORTS];				///< 4	112
	uint8_t  nOutputStyle;								///< 1	113
	uint8_t  nProtocolPort[artnet::PORTS];				///< 4	117
	uint16_t nDirection;								///< 2	119
	uint32_t nDestinationIpPort[artnet::PORTS];			///< 16	135
}__attribute__((packed));

static_assert(sizeof(struct Params) <= 144, "struct Params is too large");

struct MaskMultiPortOptions {
	static constexpr uint16_t DESTINATION_IP_A = (1U << 0);
	static constexpr uint16_t DESTINATION_IP_B = (1U << 1);
	static constexpr uint16_t DESTINATION_IP_C = (1U << 2);
	static constexpr uint16_t DESTINATION_IP_D = (1U << 3);
};

struct MaskOutputStyle {
	static constexpr uint8_t OUTPUT_STYLE_A = (1U << 0);
	static constexpr uint8_t OUTPUT_STYLE_B = (1U << 1);
	static constexpr uint8_t OUTPUT_STYLE_C = (1U << 2);
	static constexpr uint8_t OUTPUT_STYLE_D = (1U << 3);
};

struct Mask {
	static constexpr uint32_t LONG_NAME = (1U << 0);
	static constexpr uint32_t SHORT_NAME = (1U << 1);
	static constexpr uint32_t NET = (1U << 2);
	static constexpr uint32_t SUBNET = (1U << 3);
	static constexpr uint32_t FAILSAFE = (1U << 4);
	static constexpr uint32_t RDM = (1U << 5);
	//static constexpr uint32_t  = (1U << 6);
	//static constexpr uint32_t  = (1U << 7);
	static constexpr uint32_t OUTPUT = (1U << 8);
	//static constexpr uint32_t  = (1U << 9);
	//static constexpr uint32_t  = (1U << 10);
	//static constexpr uint32_t  = (1U << 11);
	static constexpr uint32_t DISABLE_MERGE_TIMEOUT = (1U << 12);
	static constexpr uint32_t UNIVERSE_A = (1U << 13);
	static constexpr uint32_t UNIVERSE_B = (1U << 14);
	static constexpr uint32_t UNIVERSE_C = (1U << 15);
	static constexpr uint32_t UNIVERSE_D = (1U << 16);
	static constexpr uint32_t MERGE_MODE = (1U << 17);
	static constexpr uint32_t MERGE_MODE_A = (1U << 18);
	static constexpr uint32_t MERGE_MODE_B = (1U << 19);
	static constexpr uint32_t MERGE_MODE_C = (1U << 20);
	static constexpr uint32_t MERGE_MODE_D = (1U << 21);
	static constexpr uint32_t PROTOCOL = (1U << 22);
	static constexpr uint32_t PROTOCOL_A = (1U << 23);
	static constexpr uint32_t PROTOCOL_B = (1U << 24);
	static constexpr uint32_t PROTOCOL_C = (1U << 25);
	static constexpr uint32_t PROTOCOL_D = (1U << 26);
	static constexpr uint32_t MAP_UNIVERSE0 = (1U << 27);
	static constexpr uint32_t SACN_PRIORITY = (1U << 28);
};

}  // namespace artnetparams

class ArtNetParamsStore {
public:
	virtual ~ArtNetParamsStore() {}

	virtual void Update(const artnetparams::Params *pArtNetParams)=0;
	virtual void Copy(artnetparams::Params *pArtNetParams)=0;
};

class ArtNetParams {
public:
	ArtNetParams(ArtNetParamsStore *pArtNetParamsStore);

	bool Load();
	void Load(const char *pBuffer, uint32_t nLength);

	void Builder(const struct artnetparams::Params *pArtNetParams, char *pBuffer, uint32_t nLength, uint32_t& nSize);
	void Save(char *pBuffer, uint32_t nLength, uint32_t& nSize) {
		Builder(nullptr, pBuffer, nLength, nSize);
	}

	void Set(uint32_t nPortIndexOffset);

	void Dump();

	bool IsRdm() const {
		return isMaskSet(artnetparams::Mask::RDM);
	}

#if (ARTNET_PAGE_SIZE == 4)
	uint8_t GetNet() const {
		return m_Params.nNet;
	}

	uint8_t GetSubnet() const {
		return m_Params.nSubnet;
	}

	uint8_t GetUniversePort(uint32_t nPortIndex) const {
		if (nPortIndex < artnet::PORTS) {
			return m_Params.nUniversePort[nPortIndex];
		}
		return UINT8_MAX;
	}
#else
	uint16_t GetUniverse(uint32_t nPortIndex) const {
		if (nPortIndex < artnet::PORTS) {
			return m_Params.nUniverse[nPortIndex];
		}
		return UINT16_MAX;
	}
#endif

	lightset::PortDir GetDirection(uint32_t nPortIndex) const {
		if (nPortIndex < artnet::PORTS) {
			const auto portDir = static_cast<lightset::PortDir>(artnetparams::portdir_shif_right(m_Params.nDirection, nPortIndex));
			return portDir;
		}
		return lightset::PortDir::DISABLE;
	}

#if defined (ESP8266)
	lightset::OutputType GetOutputType() const {
		return static_cast<lightset::OutputType>(m_Params.nOutputType);
	}
#endif

	static void staticCallbackFunction(void *p, const char *s);

private:
	void callbackFunction(const char *pLine);
	void SetBool(const uint8_t nValue, const uint32_t nMask);
	bool isMaskSet(uint32_t nMask) const {
		return (m_Params.nSetList & nMask) == nMask;
	}
	bool isMaskMultiPortOptionsSet(uint16_t nMask) const {
		return (m_Params.nMultiPortOptions & nMask) == nMask;
	}
	bool isOutputStyleSet(uint8_t nMask) const {
		return (m_Params.nOutputStyle & nMask) == nMask;
	}

private:
	ArtNetParamsStore *m_pArtNetParamsStore;
	artnetparams::Params m_Params;
};

#endif /* ARTNETPARAMS_H_ */
