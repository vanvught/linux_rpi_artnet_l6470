/**
 * @file pca9685dmxled.cpp
 *
 */
/* Copyright (C) 2018-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
# include <cstdio>
#endif
#include <cassert>

#include "pca9685dmxled.h"

#include "parse.h"

using namespace lightset;

#define DMX_MAX_CHANNELS	512
#define BOARD_INSTANCES_MAX	32

static unsigned long ceil(float f) {
	int i = static_cast<int>(f);
	if (f == static_cast<float>(i)) {
		return static_cast<unsigned long>(i);
	}
	return static_cast<unsigned long>(i + 1);
}

PCA9685DmxLed::PCA9685DmxLed()
{
}

PCA9685DmxLed::~PCA9685DmxLed() {
	delete[] m_pDmxData;
	m_pDmxData = nullptr;

	for (unsigned i = 0; i < m_nBoardInstances; i++) {
		delete m_pPWMLed[i];
		m_pPWMLed[i] = nullptr;
	}

	delete[] m_pPWMLed;
	m_pPWMLed = nullptr;

	delete[] m_pSlotInfo;
	m_pSlotInfo = nullptr;
}

void PCA9685DmxLed::Start(__attribute__((unused)) uint32_t nPortIndex) {
	if (m_bIsStarted) {
		return;
	}

	m_bIsStarted = true;

	if (__builtin_expect((m_pPWMLed == nullptr), 0)) {
		Initialize();
	}
}

void PCA9685DmxLed::Stop(__attribute__((unused)) uint32_t nPortIndex) {
	if (!m_bIsStarted) {
		return;
	}

	m_bIsStarted = false;
}

void PCA9685DmxLed::SetData(__attribute__((unused)) uint32_t nPortIndex, const uint8_t *pDmxData, uint32_t nLength, __attribute__((unused)) const bool doUpdate) {
	assert(pDmxData != nullptr);
	assert(nLength <= DMX_MAX_CHANNELS);

	if (__builtin_expect((m_pPWMLed == nullptr), 0)) {
		Start();
	}

	uint8_t *p = const_cast<uint8_t*>(pDmxData) + m_nDmxStartAddress - 1;
	uint8_t *q = m_pDmxData;

	uint16_t nChannel = m_nDmxStartAddress;

	for (unsigned j = 0; j < m_nBoardInstances; j++) {
		for (unsigned i = 0; i < PCA9685_PWM_CHANNELS; i++) {
			if ((nChannel >= (m_nDmxFootprint + m_nDmxStartAddress)) || (nChannel > nLength)) {
				j = m_nBoardInstances;
				break;
			}
			if (*p != *q) {
				uint8_t value = *p;
#ifndef NDEBUG
				printf("m_pPWMLed[%d]->SetDmx(CHANNEL(%d), %d)\n", static_cast<int>(j), static_cast<int>(i), static_cast<int>(value));
#endif
				m_pPWMLed[j]->Set(CHANNEL(i), value);
			}
			*q = *p;
			p++;
			q++;
			nChannel++;
		}
	}
}

bool PCA9685DmxLed::SetDmxStartAddress(uint16_t nDmxStartAddress) {
	assert((nDmxStartAddress != 0) && (nDmxStartAddress <= DMX_MAX_CHANNELS));

	if ((nDmxStartAddress != 0) && (nDmxStartAddress <= DMX_MAX_CHANNELS)) {
		m_nDmxStartAddress = nDmxStartAddress;
		return true;
	}

	return false;
}

void PCA9685DmxLed::SetSlotInfoRaw(const char *pSlotInfoRaw) {
	m_pSlotInfoRaw = const_cast<char*>(pSlotInfoRaw);
}

bool PCA9685DmxLed::GetSlotInfo(uint16_t nSlotOffset, SlotInfo& tSlotInfo) {
	if (nSlotOffset >  m_nDmxFootprint) {
		return false;
	}

	tSlotInfo.nType = m_pSlotInfo[nSlotOffset].nType;
	tSlotInfo.nCategory = m_pSlotInfo[nSlotOffset].nCategory;

	return true;
}

uint8_t PCA9685DmxLed::GetI2cAddress() const {
	return m_nI2cAddress;
}

void PCA9685DmxLed::SetI2cAddress(uint8_t nI2cAddress) {
	m_nI2cAddress = nI2cAddress;
}

void PCA9685DmxLed::SetBoardInstances(uint8_t nBoardInstances) {
	if ((nBoardInstances != 0) && (nBoardInstances <= BOARD_INSTANCES_MAX)) {
		m_nBoardInstances = nBoardInstances;
		m_nDmxFootprint = static_cast<uint16_t>(nBoardInstances * PCA9685_PWM_CHANNELS);
	}
}

void PCA9685DmxLed::SetPwmfrequency(uint16_t nPwmfrequency) {
	m_nPwmFrequency = nPwmfrequency;
}

bool PCA9685DmxLed::GetInvert() const {
	return m_bOutputInvert;
}

void PCA9685DmxLed::SetInvert(bool bOutputInvert) {
	m_bOutputInvert = bOutputInvert;
}

bool PCA9685DmxLed::GetOutDriver() const {
	return m_bOutputDriver;
}

void PCA9685DmxLed::SetOutDriver(bool bOutputDriver) {
	m_bOutputDriver= bOutputDriver;
}

void PCA9685DmxLed::SetDmxFootprint(uint16_t nDmxFootprint) {
	m_nDmxFootprint = nDmxFootprint;
	m_nBoardInstances = static_cast<uint8_t>(ceil(static_cast<float>((nDmxFootprint)) / PCA9685_PWM_CHANNELS));
}

void PCA9685DmxLed::Initialize() {
	assert(m_pDmxData == nullptr);
	m_pDmxData = new uint8_t[m_nDmxFootprint];
	assert(m_pDmxData != nullptr);

	for (unsigned i = 0; i < m_nDmxFootprint; i++) {
		m_pDmxData[i] = 0;
	}

	assert(m_pPWMLed == nullptr);
	m_pPWMLed = new PCA9685PWMLed*[m_nBoardInstances];
	assert(m_pPWMLed != nullptr);

	for (unsigned i = 0; i < m_nBoardInstances; i++) {
		m_pPWMLed[i] = nullptr;
	}

	for (unsigned i = 0; i < m_nBoardInstances; i++) {
		assert(m_pPWMLed[i] == nullptr);
		m_pPWMLed[i] = new PCA9685PWMLed(static_cast<uint8_t>(m_nI2cAddress + i));
		assert(m_pPWMLed[i] != nullptr);

		m_pPWMLed[i]->SetInvert(m_bOutputInvert);
		m_pPWMLed[i]->SetOutDriver(m_bOutputDriver);
		m_pPWMLed[i]->SetFrequency(m_nPwmFrequency);
		m_pPWMLed[i]->SetFullOff(CHANNEL(16), true);
#ifndef NDEBUG
		printf("Instance %d [%X]\n", i, m_nI2cAddress + i);
		m_pPWMLed[i]->Dump();
		printf("\n");
#endif
	}

	m_pSlotInfo = new SlotInfo[m_nDmxFootprint];
	assert(m_pSlotInfo != nullptr);

	char *pSlotInfoRaw = m_pSlotInfoRaw;

	for (unsigned i = 0; i < m_nDmxFootprint; i++) {
		bool isSet = false;

		if (pSlotInfoRaw != nullptr) {
			pSlotInfoRaw = Parse::DmxSlotInfo(pSlotInfoRaw, isSet, m_pSlotInfo[i].nType, m_pSlotInfo[i].nCategory);
		}

		if (!isSet) {
			m_pSlotInfo[i].nType = 0x00; // ST_PRIMARY
			m_pSlotInfo[i].nCategory = 0x0001; // SD_INTENSITY
		}
	}
}
