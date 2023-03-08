/**
 * @file pca9685dmxparams.cpp
 *
 */
/* Copyright (C) 2017-2019 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#ifndef NDEBUG
 #include <cstdio>
#endif
#include <cassert>

#include "pca9685dmxparams.h"

#include "pca9685.h"

#include "lightset.h"
#include "lightsetparamsconst.h"

#include "readconfigfile.h"
#include "sscan.h"

using namespace lightset;

#define DMX_START_ADDRESS_MASK	(1 << 0)
#define DMX_FOOTPRINT_MASK		(1 << 1)
#define DMX_SLOT_INFO_MASK		(1 << 2)
#define I2C_SLAVE_ADDRESS_MASK	(1 << 3)
#define BOARD_INSTANCES_MASK	(1 << 4)

constexpr char PARAMS_DMX_FOOTPRINT[] = "dmx_footprint";
constexpr char PARAMS_I2C_SLAVE_ADDRESS[] = "i2c_slave_address";
constexpr char PARAMS_BOARD_INSTANCES[] = "board_instances";

#define PARAMS_DMX_START_ADDRESS_DEFAULT	1
#define PARAMS_DMX_FOOTPRINT_DEFAULT		PCA9685_PWM_CHANNELS
#define PARAMS_BOARD_INSTANCES_DEFAULT		1
#define PARAMS_BOARD_INSTANCES_MAX			32

#define DMX_SLOT_INFO_LENGTH				128

PCA9685DmxParams::PCA9685DmxParams(const char *pFileName) {
	assert(pFileName != nullptr);

	m_nI2cAddress = PCA9685_I2C_ADDRESS_DEFAULT;

	m_nDmxStartAddress = PARAMS_DMX_START_ADDRESS_DEFAULT;
	m_nDmxFootprint = PARAMS_DMX_FOOTPRINT_DEFAULT;
	m_nBoardInstances = PARAMS_BOARD_INSTANCES_DEFAULT;

	m_pDmxSlotInfoRaw = new char[DMX_SLOT_INFO_LENGTH];

	assert(m_pDmxSlotInfoRaw != nullptr);
	for (unsigned i = 0; i < DMX_SLOT_INFO_LENGTH; i++) {
		m_pDmxSlotInfoRaw[i] = 0;
	}

	ReadConfigFile configfile(PCA9685DmxParams::staticCallbackFunction, this);
	configfile.Read(pFileName);
}

PCA9685DmxParams::~PCA9685DmxParams() {
	delete[] m_pDmxSlotInfoRaw;
	m_pDmxSlotInfoRaw = nullptr;
}

void PCA9685DmxParams::staticCallbackFunction(void *p, const char *s) {
	assert(p != nullptr);
	assert(s != nullptr);

	(static_cast<PCA9685DmxParams*>(p))->callbackFunction(s);
}

void PCA9685DmxParams::callbackFunction(const char *pLine) {
	assert(pLine != nullptr);

	uint8_t value8;
	uint16_t value16;
	uint32_t nLength;

	if (Sscan::Uint16(pLine, LightSetParamsConst::DMX_START_ADDRESS, value16) == Sscan::OK) {
		if ((value16 != 0) && (value16 <= dmx::UNIVERSE_SIZE)) {
			m_nDmxStartAddress = value16;
			m_bSetList |= DMX_START_ADDRESS_MASK;
		}
		return;
	}

	if (Sscan::Uint16(pLine, PARAMS_DMX_FOOTPRINT, value16) == Sscan::OK) {
		if ((value16 != 0) && (value16 <= (PCA9685_PWM_CHANNELS * PARAMS_BOARD_INSTANCES_MAX))) {
			m_nDmxFootprint = value16;
			m_bSetList |= DMX_FOOTPRINT_MASK;
		}
		return;
	}

	if (Sscan::I2cAddress(pLine, PARAMS_I2C_SLAVE_ADDRESS, value8) == Sscan::OK) {
		if ((value8 >= PCA9685_I2C_ADDRESS_DEFAULT) && (value8 != PCA9685_I2C_ADDRESS_FIXED)) {
			m_nI2cAddress = value8;
			m_bSetList |= I2C_SLAVE_ADDRESS_MASK;
		return;
		}
	}

	if (Sscan::Uint8(pLine, PARAMS_BOARD_INSTANCES, value8) == Sscan::OK) {
		if ((value8 != 0) && (value8 <= PARAMS_BOARD_INSTANCES_MAX)) {
			m_nBoardInstances = value8;
			m_bSetList |= BOARD_INSTANCES_MASK;
		}
		return;
	}

	nLength = DMX_SLOT_INFO_LENGTH;
	if (Sscan::Char(pLine, LightSetParamsConst::DMX_SLOT_INFO, m_pDmxSlotInfoRaw, nLength) == Sscan::OK) {
		if (nLength >= 7) { // 00:0000 at least one value set
			m_bSetList |= DMX_SLOT_INFO_MASK;
		}
	}
}

uint8_t PCA9685DmxParams::GetI2cAddress(bool &pIsSet) const {
	pIsSet = isMaskSet(I2C_SLAVE_ADDRESS_MASK);
	return m_nI2cAddress;
}

uint16_t PCA9685DmxParams::GetDmxStartAddress(bool &pIsSet) const {
	pIsSet = isMaskSet(DMX_START_ADDRESS_MASK);
	return m_nDmxStartAddress;
}

uint16_t PCA9685DmxParams::GetDmxFootprint(bool &pIsSet) const {
	pIsSet = isMaskSet(DMX_FOOTPRINT_MASK);
	return m_nDmxFootprint;
}

uint8_t PCA9685DmxParams::GetBoardInstances(bool &pIsSet) const {
	pIsSet = isMaskSet(BOARD_INSTANCES_MASK);
	return m_nBoardInstances;
}

const char* PCA9685DmxParams::GetDmxSlotInfoRaw(bool &pIsSet) const {
	pIsSet = isMaskSet(DMX_SLOT_INFO_MASK);
	return m_pDmxSlotInfoRaw;
}

void PCA9685DmxParams::Dump() {
#ifndef NDEBUG
	if (m_bSetList == 0) {
		return;
	}

	if(isMaskSet(DMX_START_ADDRESS_MASK)) {
		printf(" %s=%d\n", LightSetParamsConst::PARAMS_DMX_START_ADDRESS, m_nDmxStartAddress);
	}

	if(isMaskSet(DMX_FOOTPRINT_MASK)) {
		printf(" %s=%d\n", PARAMS_DMX_FOOTPRINT, m_nDmxFootprint);
	}

	if(isMaskSet(I2C_SLAVE_ADDRESS_MASK)) {
		printf(" %s=0x%2x\n", PARAMS_I2C_SLAVE_ADDRESS, m_nI2cAddress);
	}

	if(isMaskSet(BOARD_INSTANCES_MASK)) {
		printf(" %s=%d\n", PARAMS_BOARD_INSTANCES, m_nBoardInstances);
	}

	if(isMaskSet(DMX_SLOT_INFO_MASK)) {
		printf(" %s=%s\n", LightSetParamsConst::PARAMS_DMX_SLOT_INFO, m_pDmxSlotInfoRaw);
	}
#endif
}

bool PCA9685DmxParams::isMaskSet(uint32_t mask) const {
	return (m_bSetList & mask) == mask;
}

bool PCA9685DmxParams::GetSetList() const {
	return m_bSetList != 0;
}
