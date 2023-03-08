/**
 * @file pca9685dmxservo.cpp
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

#include "pca9685dmxservo.h"

#define DMX_MAX_CHANNELS	512
#define BOARD_INSTANCES_MAX	32

static unsigned long ceil(float f) {
	int i = static_cast<int>(f);
	if (f == static_cast<float>(i)) {
		return static_cast<unsigned long>(i);
	}
	return static_cast<unsigned long>(i + 1);
}

PCA9685DmxServo::PCA9685DmxServo()
	
{
}

PCA9685DmxServo::~PCA9685DmxServo() {
	delete m_pServo;
	m_pServo = nullptr;
}

bool PCA9685DmxServo::SetDmxStartAddress(uint16_t nDmxStartAddress) {
	assert((nDmxStartAddress != 0) && (nDmxStartAddress <= DMX_MAX_CHANNELS));

	if ((nDmxStartAddress != 0) && (nDmxStartAddress <= DMX_MAX_CHANNELS)) {
		m_nDmxStartAddress = nDmxStartAddress;
		return true;
	}

	return false;
}

void PCA9685DmxServo::Start(__attribute__((unused)) uint32_t nPortIndex) {
	if (m_bIsStarted) {
		return;
	}

	m_bIsStarted = true;

	if (__builtin_expect((m_pServo == nullptr), 0)) {
		Initialize();
	}
}

void PCA9685DmxServo::Stop(__attribute__((unused)) uint32_t nPortIndex) {
	if (!m_bIsStarted) {
		return;
	}

	m_bIsStarted = false;
}

void PCA9685DmxServo::SetData(__attribute__((unused)) uint32_t nPortIndex, const uint8_t* pDmxData, uint32_t nLength) {
	assert(pDmxData != nullptr);
	assert(nLength <= DMX_MAX_CHANNELS);

	if (__builtin_expect((m_pServo == nullptr), 0)) {
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
				printf("m_pServo[%d]->SetDmx(CHANNEL(%d), %d)\n", (int) j, (int) i, (int) value);
#endif
				m_pServo[j]->Set(CHANNEL(i), value);
			}
			*q = *p;
			p++;
			q++;
			nChannel++;
		}
	}
}

void PCA9685DmxServo::SetI2cAddress(uint8_t nI2cAddress) {
	m_nI2cAddress = nI2cAddress;
}

void PCA9685DmxServo::SetBoardInstances(uint8_t nBoardInstances) {
	if ((nBoardInstances != 0) && (nBoardInstances <= BOARD_INSTANCES_MAX)) {
		m_nBoardInstances = nBoardInstances;
		m_nDmxFootprint = static_cast<uint16_t>(nBoardInstances * PCA9685_PWM_CHANNELS);
	}
}

void PCA9685DmxServo::SetLeftUs(uint16_t nLeftUs) {
	m_nLeftUs = nLeftUs;
}

void PCA9685DmxServo::SetRightUs(uint16_t nRightUs) {
	m_nRightUs = nRightUs;
}

void PCA9685DmxServo::SetDmxFootprint(uint16_t nDmxFootprint) {
	m_nDmxFootprint = nDmxFootprint;
	m_nBoardInstances = static_cast<uint8_t>(ceil(static_cast<float>((nDmxFootprint)) / PCA9685_PWM_CHANNELS));
}

void PCA9685DmxServo::Initialize() {
	assert(m_pDmxData == nullptr);
	m_pDmxData = new uint8_t[m_nBoardInstances * PCA9685_PWM_CHANNELS];
	assert(m_pDmxData != nullptr);

	for (unsigned i = 0; i < m_nBoardInstances * PCA9685_PWM_CHANNELS; i++) {
		m_pDmxData[i] = 0;
	}

	assert(m_pServo == nullptr);
	m_pServo = new PCA9685Servo*[m_nBoardInstances];
	assert(m_pServo != nullptr);

	for (unsigned i = 0; i < m_nBoardInstances; i++) {
		m_pServo[i] = nullptr;
	}

	for (unsigned i = 0; i < m_nBoardInstances; i++) {
		assert(m_pServo[i] == nullptr);
		m_pServo[i] = new PCA9685Servo(static_cast<uint8_t>(m_nI2cAddress + i));
		assert(m_pServo[i] != nullptr);

		m_pServo[i]->SetLeftUs(m_nLeftUs);
		m_pServo[i]->SetRightUs(m_nRightUs);
#ifndef NDEBUG
		printf("Instance %d [%X]\n", i, m_nI2cAddress + i);
		m_pServo[i]->Dump();
		printf("\n");
#endif
	}
}
