#if !defined(ORANGE_PI)
/**
 * @file slushdmxparams.h
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

#if !defined(__clang__)	// Needed for compiling on MacOS
# pragma GCC push_options
# pragma GCC optimize ("Os")
#endif

#include <cstdint>
#include <cstring>
#ifndef NDEBUG
# include <cstdio>
#endif
#include <cassert>

#include "slushdmxparams.h"
#include "slushdmxparamsconst.h"
#include "slushdmx.h"
#include "storeslushdmx.h"

#include "l6470dmxconst.h"

#include "lightset.h"

#include "readconfigfile.h"
#include "sscan.h"
#include "propertiesbuilder.h"

#include "debug.h"

using namespace lightset;

SlushDmxParams::SlushDmxParams() {
	assert(sizeof(m_aFileName) > strlen(L6470DmxConst::FILE_NAME_MOTOR));
	strncpy(m_aFileName, L6470DmxConst::FILE_NAME_MOTOR, sizeof(m_aFileName));
}

void SlushDmxParams::Load() {
	m_Params.nSetList = 0;

	ReadConfigFile configfile(SlushDmxParams::staticCallbackFunction, this);

	if (configfile.Read(SlushDmxParamsConst::FILE_NAME)) {
		StoreSlushDmx::Update(&m_Params);
	} else {
		StoreSlushDmx::Copy(&m_Params);
	}
}

void SlushDmxParams::Load(const char *pBuffer, uint32_t nLength) {
	assert(pBuffer != nullptr);
	assert(nLength != 0);

	m_Params.nSetList = 0;

	ReadConfigFile config(SlushDmxParams::staticCallbackFunction, this);

	config.Read(pBuffer, nLength);

	StoreSlushDmx::Update(&m_Params);
}

void SlushDmxParams::callbackFunction(const char *pLine) {
	uint8_t value;
	uint16_t value16;

	if (Sscan::Uint8(pLine, SlushDmxParamsConst::USE_SPI, value) == Sscan::OK) {
		if (value != 0) {
			m_Params.nUseSpiBusy = 1;
			m_Params.nSetList |= slushdmxparams::Mask::USE_SPI_BUSY;
			return;
		}
	}

	if (Sscan::Uint16(pLine, SlushDmxParamsConst::DMX_START_ADDRESS_PORT_A, value16) == Sscan::OK) {
		if (value16 <= dmx::UNIVERSE_SIZE) {
			m_Params.nDmxStartAddressPortA = value16;
			m_Params.nSetList |= slushdmxparams::Mask::START_ADDRESS_PORT_A;
		}
		return;
	}

	if (Sscan::Uint16(pLine, SlushDmxParamsConst::DMX_START_ADDRESS_PORT_B, value16) == Sscan::OK) {
		if (value16 <= dmx::UNIVERSE_SIZE) {
			m_Params.nDmxStartAddressPortB = value16;
			m_Params.nSetList |= slushdmxparams::Mask::START_ADDRESS_PORT_B;
		}
		return;
	}

	if (Sscan::Uint16(pLine, SlushDmxParamsConst::DMX_FOOTPRINT_PORT_A, value16) == Sscan::OK) {
		if ((value16 > 0) && (value16 <= IO_PINS_IOPORT)) {
			m_Params.nDmxFootprintPortA = value16;
			m_Params.nSetList |= slushdmxparams::Mask::FOOTPRINT_PORT_A;
		}
		return;
	}

	if (Sscan::Uint16(pLine, SlushDmxParamsConst::DMX_FOOTPRINT_PORT_B, value16) == Sscan::OK) {
		if ((value16 > 0) && (value16 <= IO_PINS_IOPORT)) {
			m_Params.nDmxFootprintPortB = value16;
			m_Params.nSetList |= slushdmxparams::Mask::FOOTPRINT_PORT_B;
		}
		return;
	}
}

void SlushDmxParams::Set(SlushDmx *pSlushDmx) {
	assert(pSlushDmx != nullptr);

	if (isMaskSet(slushdmxparams::Mask::USE_SPI_BUSY)) {
		pSlushDmx->SetUseSpiBusy(m_Params.nUseSpiBusy == 1);
	}

	if (isMaskSet(slushdmxparams::Mask::START_ADDRESS_PORT_A)) {
		pSlushDmx->SetDmxStartAddressPortA(m_Params.nDmxStartAddressPortA);
	}

	if (isMaskSet(slushdmxparams::Mask::FOOTPRINT_PORT_A)) {
		pSlushDmx->SetDmxFootprintPortA(m_Params.nDmxFootprintPortA);
	}

	if (isMaskSet(slushdmxparams::Mask::START_ADDRESS_PORT_B)) {
		pSlushDmx->SetDmxStartAddressPortB(m_Params.nDmxStartAddressPortB);
	}

	if (isMaskSet(slushdmxparams::Mask::FOOTPRINT_PORT_B)) {
		pSlushDmx->SetDmxFootprintPortB(m_Params.nDmxFootprintPortB);
	}
}

void SlushDmxParams::Builder(const struct slushdmxparams::Params *ptSlushDmxParams, char *pBuffer, uint32_t nLength, uint32_t& nSize) {
	assert(pBuffer != nullptr);

	if (ptSlushDmxParams != nullptr) {
		memcpy(&m_Params, ptSlushDmxParams, sizeof(struct slushdmxparams::Params));
	} else {
		StoreSlushDmx::Copy(&m_Params);
	}

	PropertiesBuilder builder(SlushDmxParamsConst::FILE_NAME, pBuffer, nLength);

	builder.Add(SlushDmxParamsConst::USE_SPI, m_Params.nUseSpiBusy, isMaskSet(slushdmxparams::Mask::USE_SPI_BUSY));

	builder.Add(SlushDmxParamsConst::DMX_START_ADDRESS_PORT_A, m_Params.nDmxStartAddressPortA, isMaskSet(slushdmxparams::Mask::START_ADDRESS_PORT_A));
	builder.Add(SlushDmxParamsConst::DMX_FOOTPRINT_PORT_A, m_Params.nDmxFootprintPortA, isMaskSet(slushdmxparams::Mask::FOOTPRINT_PORT_A));

	builder.Add(SlushDmxParamsConst::DMX_START_ADDRESS_PORT_B, m_Params.nDmxStartAddressPortB, isMaskSet(slushdmxparams::Mask::START_ADDRESS_PORT_B));
	builder.Add(SlushDmxParamsConst::DMX_FOOTPRINT_PORT_B, m_Params.nDmxFootprintPortB, isMaskSet(slushdmxparams::Mask::FOOTPRINT_PORT_B));

	nSize = builder.GetSize();
}

void SlushDmxParams::Save(char *pBuffer, uint32_t nLength, uint32_t& nSize) {
	Builder(nullptr, pBuffer, nLength, nSize);
}

void SlushDmxParams::staticCallbackFunction(void *p, const char *s) {
	assert(p != nullptr);
	assert(s != nullptr);

	(static_cast<SlushDmxParams*>(p))->callbackFunction(s);
}

void SlushDmxParams::Dump() {
	printf("%s::%s \'%s\':\n", __FILE__, __FUNCTION__, SlushDmxParamsConst::FILE_NAME);
	printf(" %s=%d [%s]\n", SlushDmxParamsConst::USE_SPI, m_Params.nUseSpiBusy, m_Params.nUseSpiBusy == 0 ? "No" : "Yes");
	printf(" %s=%d\n", SlushDmxParamsConst::DMX_START_ADDRESS_PORT_A, m_Params.nDmxStartAddressPortA);
	printf(" %s=%d\n", SlushDmxParamsConst::DMX_FOOTPRINT_PORT_A, m_Params.nDmxFootprintPortA);
	printf(" %s=%d\n", SlushDmxParamsConst::DMX_START_ADDRESS_PORT_B, m_Params.nDmxStartAddressPortB);
	printf(" %s=%d\n", SlushDmxParamsConst::DMX_FOOTPRINT_PORT_B, m_Params.nDmxFootprintPortB);
}

#endif /* #if !defined(ORANGE_PI) */
