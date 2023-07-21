#if !defined(ORANGE_PI)
/**
 * @file slushdmx.h
 *
 */
/* Copyright (C) 2017-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cassert>

#include "slushdmx.h"
#include "slushboard.h"

#include "readconfigfile.h"
#include "sscan.h"

#include "l6470params.h"
#include "l6470dmxmodes.h"

#include "motorparams.h"
#include "modeparams.h"

#include "lightset.h"

#include "parse.h"

#include "debug.h"

using namespace lightset;

#define IO_PINS_IOPORT				8
#define DMX_SLOT_INFO_RAW_LENGTH	128

constexpr char PARAMS_SLUSH_USE_SPI[] = "use_spi_busy";

constexpr char PARAMS_SLUSH_DMX_START_ADDRESS_PORT_A[] = "dmx_start_address_port_a";
constexpr char PARAMS_SLUSH_DMX_FOOTPRINT_PORT_A[] = "dmx_footprint_port_a";
constexpr char PARAMS_DMX_SLOT_INFO_PORT_A[] = "dmx_slot_info_port_a";

constexpr char PARAMS_SLUSH_DMX_START_ADDRESS_PORT_B[] = "dmx_start_address_port_b";
constexpr char PARAMS_SLUSH_DMX_FOOTPRINT_PORT_B[] = "dmx_footprint_port_b";
constexpr char PARAMS_DMX_SLOT_INFO_PORT_B[] = "dmx_slot_info_port_b";

constexpr char PARAMS_DMX_MODE[] = "dmx_mode";
constexpr char PARAMS_DMX_START_ADDRESS[] = "dmx_start_address";
constexpr char PARAMS_DMX_SLOT_INFO[] = "dmx_slot_info";

void SlushDmx::staticCallbackFunction(void *p, const char *s) {
	assert(p != nullptr);
	assert(s != nullptr);

	(static_cast<SlushDmx*>(p))->callbackFunction(s);
}

void SlushDmx::callbackFunction(const char *pLine) {
	assert(pLine != nullptr);

	uint8_t nValue8;
	uint16_t nValue16;
	uint32_t nLength;

	if (Sscan::Uint8(pLine, PARAMS_SLUSH_USE_SPI, nValue8) ==Sscan::OK) {
		if (nValue8 != 0) {
			m_bUseSpiBusy = true;
			return;
		}
	}

	if (Sscan::Uint16(pLine, PARAMS_SLUSH_DMX_START_ADDRESS_PORT_A, nValue16) == Sscan::OK) {
		if (nValue16 <= dmx::UNIVERSE_SIZE) {
			m_nDmxStartAddressPortA = nValue16;
		}
		return;
	}

	if (Sscan::Uint16(pLine, PARAMS_SLUSH_DMX_START_ADDRESS_PORT_B, nValue16) == Sscan::OK) {
		if (nValue16 <= dmx::UNIVERSE_SIZE) {
			m_nDmxStartAddressPortB = nValue16;
		}
		return;
	}

	if (Sscan::Uint16(pLine, PARAMS_SLUSH_DMX_FOOTPRINT_PORT_A, nValue16) == Sscan::OK) {
		if ((nValue16 > 0) && (nValue16 <= IO_PINS_IOPORT)) {
			m_nDmxFootprintPortA = nValue16;
		}
		return;
	}

	if (Sscan::Uint16(pLine, PARAMS_SLUSH_DMX_FOOTPRINT_PORT_B, nValue16) == Sscan::OK) {
		if ((nValue16 > 0) && (nValue16 <= IO_PINS_IOPORT)) {
			m_nDmxFootprintPortB = nValue16;
		}
		return;
	}

	if (Sscan::Uint8(pLine, PARAMS_DMX_MODE, m_nDmxMode) == Sscan::OK) {
		return;
	}

	if (Sscan::Uint16(pLine, PARAMS_DMX_START_ADDRESS, m_nDmxStartAddressMode) == Sscan::OK) {
		return;
	}

	nLength = DMX_SLOT_INFO_RAW_LENGTH;
	if (Sscan::Char(pLine, PARAMS_DMX_SLOT_INFO_PORT_A, m_pSlotInfoRawPortA, nLength) == Sscan::OK) {
		if (nLength < 7) { // 00:0000 at least one value set
			m_pSlotInfoRawPortA[0] = '\0';
		}
		return;
	}

	nLength = DMX_SLOT_INFO_RAW_LENGTH;
	if (Sscan::Char(pLine, PARAMS_DMX_SLOT_INFO_PORT_B, m_pSlotInfoRawPortB, nLength) == Sscan::OK) {
		if (nLength < 7) { // 00:0000 at least one value set
			m_pSlotInfoRawPortB[0] = '\0';
		}
	}

	nLength = DMX_SLOT_INFO_RAW_LENGTH;
	if (Sscan::Char(pLine, PARAMS_DMX_SLOT_INFO, m_pSlotInfoRaw, nLength) == Sscan::OK) {
		if (nLength < 7) { // 00:0000 at least one value set
			m_pSlotInfoRaw[0] = '\0';
		}
	}
}

SlushDmx::SlushDmx(bool bUseSPI):
	
	m_bUseSpiBusy(bUseSPI),
	
	m_nDmxStartAddress(dmx::ADDRESS_INVALID)
{
	DEBUG_ENTRY;

	m_pBoard = new SlushBoard;
	assert(m_pBoard != nullptr);

	m_nDmxMode = L6470DMXMODE_UNDEFINED;
	m_nDmxStartAddressMode = 0;

	m_nDmxStartAddressPortA = 0;
	m_nDmxFootprintPortA = IO_PINS_IOPORT;
	m_pSlotInfoRawPortA = new char[DMX_SLOT_INFO_RAW_LENGTH];
	m_pSlotInfoPortA = nullptr;
	m_nDataPortA = 0;

	m_nDmxStartAddressPortB = 0;
	m_nDmxFootprintPortB = IO_PINS_IOPORT;
	m_pSlotInfoRawPortB = new char[DMX_SLOT_INFO_RAW_LENGTH];
	m_pSlotInfoPortB = nullptr;
	m_nDataPortB = 0;

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		m_pSlushMotor[i] = nullptr;
		m_pMotorParams[i] = nullptr;
		m_pModeParams[i] = nullptr;
		m_pL6470DmxModes[i] = nullptr;
		m_pSlotInfo[i] = nullptr;
	}

	m_pSlotInfoRaw = new char[DMX_SLOT_INFO_RAW_LENGTH];

	for (uint32_t i = 0; i < DMX_SLOT_INFO_RAW_LENGTH; i++) {
		m_pSlotInfoRawPortA[i] = 0;
		m_pSlotInfoRawPortB[i] = 0;
		m_pSlotInfoRaw[i] = 0;
	}

	DEBUG_EXIT;
}

SlushDmx::~SlushDmx() {
	DEBUG_ENTRY;

	if (m_pSlotInfoRawPortA != nullptr) {
		delete[] m_pSlotInfoRawPortA;
		m_pSlotInfoRawPortA = nullptr;
	}

	if (m_pSlotInfoPortA != nullptr) {
		delete[] m_pSlotInfoPortA;
		m_pSlotInfoPortA = nullptr;
	}

	if (m_pSlotInfoRawPortB != nullptr) {
		delete[] m_pSlotInfoRawPortB;
		m_pSlotInfoRawPortB = nullptr;
	}

	if (m_pSlotInfoPortB != nullptr) {
		delete[] m_pSlotInfoPortB;
		m_pSlotInfoPortB = nullptr;
	}

	for (int i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {

		if (m_pSlushMotor[i] != nullptr) {
			delete m_pSlushMotor[i];
			m_pSlushMotor[i] = nullptr;
		}

		if (m_pMotorParams[i] != nullptr) {
			delete m_pMotorParams[i];
			m_pMotorParams[i] = nullptr;
		}

		if (m_pModeParams[i] != nullptr) {
			delete m_pModeParams[i];
			m_pModeParams[i] = nullptr;
		}

		if (m_pL6470DmxModes[i] != nullptr) {
			delete m_pL6470DmxModes[i];
			m_pL6470DmxModes[i] = nullptr;
		}

		if (m_pSlotInfo[i] != nullptr) {
			delete[] m_pSlotInfo[i];
			m_pSlotInfo[i] = nullptr;
		}
	}

	if (m_pSlotInfoRaw != nullptr) {
		delete[] m_pSlotInfoRaw;
		m_pSlotInfoRaw = nullptr;
	}

	delete m_pBoard;
	m_pBoard = nullptr;

	DEBUG_EXIT;
}

void SlushDmx::Start(__attribute__((unused)) uint32_t nPortIndex) {
	DEBUG_ENTRY;

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pL6470DmxModes[i] != nullptr) {
			m_pL6470DmxModes[i]->Start();
		}
	}

	DEBUG_EXIT;
}

void SlushDmx::Stop(__attribute__((unused)) uint32_t nPortIndex) {
	DEBUG_ENTRY;

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pL6470DmxModes[i] != nullptr) {
			m_pL6470DmxModes[i]->Stop();
		}
	}

	DEBUG_EXIT;
}

void SlushDmx::ReadConfigFiles() {
	DEBUG_ENTRY;

	ReadConfigFile configfile(SlushDmx::staticCallbackFunction, this);

	m_nDmxStartAddressPortA = 0;
	m_nDmxStartAddressPortB = 0;

	if (configfile.Read("slush.txt")) {

		// MCP23017 Port A
		if (m_nDmxStartAddressPortA > 0) {
			for (uint32_t pin = 0; pin < m_nDmxFootprintPortA; pin++) {
				m_pBoard->IOFSel(SLUSH_IO_PORTA, static_cast<TSlushIOPins>(pin), SLUSH_IO_FSEL_OUTP);
			}
			m_bSetPortA = true;

			m_nDmxStartAddress = m_nDmxStartAddressPortA;
			m_nDmxFootprint = m_nDmxFootprintPortA;

			m_pSlotInfoPortA = new SlotInfo[m_nDmxFootprintPortA];
			assert(m_pSlotInfoPortA != nullptr);

			char *pSlotInfoRaw = m_pSlotInfoRawPortA;

			for (uint32_t i = 0; i < m_nDmxFootprintPortA; i++) {
				bool isSet = false;

				if (pSlotInfoRaw != nullptr) {
					pSlotInfoRaw = Parse::DmxSlotInfo(pSlotInfoRaw, isSet, m_pSlotInfoPortA[i].nType, m_pSlotInfoPortA[i].nCategory);
				}

				if (!isSet) {
					m_pSlotInfoPortA[i].nType = 0x00; // ST_PRIMARY
					m_pSlotInfoPortA[i].nCategory = 0xFFFF; // SD_UNDEFINED
				}
			}

#ifndef NDEBUG
			printf("DMX Start Address Output PortA = %d, Footprint PortA = %d\n", m_nDmxStartAddressPortA, m_nDmxFootprintPortA);
			printf("DMX Start Address:%d, DMX Footprint:%d\n", static_cast<int>(m_nDmxStartAddress), static_cast<int>(m_nDmxFootprint));
#endif
		}

		// MCP23017 Port B
		if (m_nDmxStartAddressPortB > 0) {
			for (uint32_t pin = 0; pin < m_nDmxFootprintPortB; pin++) {
				m_pBoard->IOFSel(SLUSH_IO_PORTB, static_cast<TSlushIOPins>(pin), SLUSH_IO_FSEL_OUTP);
			}
			m_bSetPortB = true;

			if (m_nDmxStartAddress == dmx::ADDRESS_INVALID) {
				m_nDmxStartAddress = m_nDmxStartAddressPortB;
				m_nDmxFootprint = m_nDmxFootprintPortB;
			} else {
				const uint16_t nDmxChannelLastCurrent = m_nDmxStartAddress + m_nDmxFootprint;
				m_nDmxStartAddress = std::min(m_nDmxStartAddress, m_nDmxStartAddressPortB);

				const uint16_t nDmxChannelLastNew = m_nDmxStartAddressPortB + m_nDmxFootprintPortB;
				m_nDmxFootprint = std::max(nDmxChannelLastCurrent, nDmxChannelLastNew) - m_nDmxStartAddress;
			}

			m_pSlotInfoPortB = new SlotInfo[m_nDmxFootprintPortB];
			assert(m_pSlotInfoPortB != nullptr);

			char *pSlotInfoRaw = m_pSlotInfoRawPortB;

			for (uint32_t i = 0; i < m_nDmxFootprintPortB; i++) {
				bool isSet = false;

				if (pSlotInfoRaw != nullptr) {
					pSlotInfoRaw = Parse::DmxSlotInfo(pSlotInfoRaw, isSet, m_pSlotInfoPortB[i].nType, m_pSlotInfoPortB[i].nCategory);
				}

				if (!isSet) {
					m_pSlotInfoPortB[i].nType = 0x00; // ST_PRIMARY
					m_pSlotInfoPortB[i].nCategory = 0xFFFF; // SD_UNDEFINED
				}
			}

#ifndef NDEBUG
			printf("DMX Start Address Output PortB = %d, Footprint PortB = %d\n", m_nDmxStartAddressPortB, m_nDmxFootprintPortB);
			printf("DMX Start Address:%d, DMX Footprint:%d\n", static_cast<int>(m_nDmxStartAddress), static_cast<int>(m_nDmxFootprint));
#endif
		}
	}

	char fileName[] = "motor%.txt";

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {

		fileName[5] = i + '0';

		m_pSlotInfoRaw[0] = 0;

		if (configfile.Read(fileName)) {
#ifndef NDEBUG
			printf("Motor %d:\n", i);
			printf("\t%s=%d (DMX footprint=%d)\n", PARAMS_DMX_MODE, m_nDmxMode, L6470DmxModes::GetDmxFootPrintMode(m_nDmxMode));
			printf("\t%s=%d\n", PARAMS_DMX_START_ADDRESS, m_nDmxStartAddressMode);
			printf("\t=============================\n");
#endif
			if ((m_nDmxStartAddressMode <= dmx::UNIVERSE_SIZE) && (L6470DmxModes::GetDmxFootPrintMode(m_nDmxMode) != 0)) {
				m_pSlushMotor[i] = new SlushMotor(i, m_bUseSpiBusy);
				assert(m_pSlushMotor[i] != nullptr);

				if (m_pSlushMotor[i] != nullptr) {
					if (m_pSlushMotor[i]->IsConnected()) {
						m_nMotorsConnected++;
						m_pSlushMotor[i]->Dump();

						m_pMotorParams[i] = new MotorParams;
						assert(m_pMotorParams[i] != nullptr);
						m_pMotorParams[i]->Load(i);
						m_pMotorParams[i]->Dump();
						m_pMotorParams[i]->Set(m_pSlushMotor[i]);

						L6470Params l6470Params;
						l6470Params.Load(i);
						l6470Params.Dump();
						l6470Params.Set(m_pSlushMotor[i]);

						m_pSlushMotor[i]->Dump();

						m_pModeParams[i] = new ModeParams;
						assert(m_pModeParams[i] != nullptr);
						m_pModeParams[i]->Load(i);
						m_pModeParams[i]->Dump();

						m_pL6470DmxModes[i] = new L6470DmxModes(static_cast<TL6470DmxModes>(m_nDmxMode), m_nDmxStartAddressMode, m_pSlushMotor[i], m_pMotorParams[i], m_pModeParams[i]);
						assert(m_pL6470DmxModes[i] != nullptr);

						if (m_pL6470DmxModes[i] != nullptr) {
							if (m_nDmxStartAddress == dmx::ADDRESS_INVALID) {
								m_nDmxStartAddress = m_pL6470DmxModes[i]->GetDmxStartAddress();
								m_nDmxFootprint = m_pL6470DmxModes[i]->GetDmxFootPrint();
							} else {
								const uint16_t nDmxChannelLastCurrent = m_nDmxStartAddress + m_nDmxFootprint;
								m_nDmxStartAddress = std::min(m_nDmxStartAddress, m_pL6470DmxModes[i]->GetDmxStartAddress());

								const uint16_t nDmxChannelLastNew = m_nDmxStartAddressMode + m_pL6470DmxModes[i]->GetDmxFootPrint();
								m_nDmxFootprint = std::max(nDmxChannelLastCurrent, nDmxChannelLastNew) - m_nDmxStartAddress;
							}

							m_pSlotInfo[i] = new SlotInfo[m_pL6470DmxModes[i]->GetDmxFootPrint()];
							char *pSlotInfoRaw = m_pSlotInfoRaw;

							for (uint32_t j = 0; j < m_pL6470DmxModes[i]->GetDmxFootPrint(); j++) {
								bool isSet = false;

								if (pSlotInfoRaw != nullptr) {
									pSlotInfoRaw = Parse::DmxSlotInfo(pSlotInfoRaw, isSet, m_pSlotInfo[i][j].nType, m_pSlotInfo[i][j].nCategory);
								}

								if (!isSet) {
									m_pSlotInfo[i][j].nType = 0x00; // ST_PRIMARY
									m_pSlotInfo[i][j].nCategory = 0xFFFF; // SD_UNDEFINED
								}
							}

#ifndef NDEBUG
							printf("DMX Mode: %d, DMX Start Address: %d\n", m_pL6470DmxModes[i]->GetMode(), m_pL6470DmxModes[i]->GetDmxStartAddress());
							printf("DMX Start Address:%d, DMX Footprint:%d\n", static_cast<int>(m_nDmxStartAddress), static_cast<int>(m_nDmxFootprint));
#endif
						}
#ifndef NDEBUG
						printf("Use SPI Busy: %s\n", m_pSlushMotor[i]->GetUseSpiBusy() ? "Yes" : "No");
#endif
					} else {
						delete m_pSlushMotor[i];
						m_pSlushMotor[i] = nullptr;
					}
				} else {
					printf("Internal error!\n");
				}
			}
#ifndef NDEBUG
			printf("Motor %d: --------- end ---------\n", i);
#endif
		} else {
#ifndef NDEBUG
			printf("Configuration file : %s not found\n", fileName);
#endif
		}
	}

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pL6470DmxModes[i] != nullptr) {
			m_pL6470DmxModes[i]->InitSwitch();
		}
	}

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pSlushMotor[i] != nullptr) {
			while (m_pL6470DmxModes[i]->BusyCheck())
				;
		}
	}

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pL6470DmxModes[i] != nullptr) {
			m_pL6470DmxModes[i]->InitPos();
		}
	}

#ifndef NDEBUG
	printf("Motors connected : %d\n", static_cast<int>(m_nMotorsConnected));
#endif
	DEBUG_EXIT;
}

void SlushDmx::SetData(__attribute__((unused)) uint32_t nPortIndex, const uint8_t *pData, uint32_t nLength, __attribute__((unused)) const bool doUpdate) {
	DEBUG_ENTRY;

	assert(pData != nullptr);
	assert(nLength <= dmx::UNIVERSE_SIZE);

	bool bIsDmxDataChanged[SLUSH_DMX_MAX_MOTORS];

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pL6470DmxModes[i] != nullptr) {
			bIsDmxDataChanged[i] = m_pL6470DmxModes[i]->IsDmxDataChanged(pData, nLength);

			if(bIsDmxDataChanged[i]) {
				m_pL6470DmxModes[i]->HandleBusy();
			}
		} else {
			bIsDmxDataChanged[i] = false;
		}
#ifndef NDEBUG
		printf("bIsDmxDataChanged[%d]=%d\n", i, bIsDmxDataChanged[i]);
#endif
	}

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (bIsDmxDataChanged[i]) {
			while (m_pL6470DmxModes[i]->BusyCheck())
				;
		}
	}

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (bIsDmxDataChanged[i]) {
			m_pL6470DmxModes[i]->DmxData(pData, nLength);
		}
	}

	UpdateIOPorts(pData, nLength);

	DEBUG_EXIT;
}

void SlushDmx::Sync(__attribute__((unused)) uint32_t const nPortIndex) {
	//TODO Implement Sync
}

void SlushDmx::Sync(__attribute__((unused)) const bool doForce) {
	//TODO Implement Sync
}

void SlushDmx::UpdateIOPorts(const uint8_t *pData, uint32_t nLength) {
	DEBUG_ENTRY;

	assert(pData != nullptr);
	assert(nLength <= dmx::UNIVERSE_SIZE);

	uint8_t *p;
	uint8_t nPortData;
	uint16_t nDmxAddress;

	nDmxAddress = m_nDmxStartAddressPortA;

	if (m_bSetPortA && (nLength >= nDmxAddress)) {
		nPortData = 0;
		p = const_cast<uint8_t*>(pData) + nDmxAddress - 1;

		for (uint32_t i = 0; i < m_nDmxFootprintPortA; i++) {
			if (nDmxAddress++ > nLength) {
				break;
			}
			if ((*p & 0x80) != 0) {	// 0-127 is off, 128-255 is on
				nPortData = nPortData | (1 << i);
			}
			p++;
		}

		if (nPortData != m_nDataPortA) {
			m_nDataPortA = nPortData;
			m_pBoard->IOWrite(SLUSH_IO_PORTA, nPortData);
#ifndef NDEBUG
			printf("\tPort A: DMX data has changed! %.2X\n", nPortData);
#endif
		} else {
#ifndef NDEBUG
			printf("\tPort A: Nothing to do..\n");
#endif
		}
	}

	nDmxAddress = m_nDmxStartAddressPortB;

	if (m_bSetPortB && (nLength >= nDmxAddress)) {
		nPortData = 0;
		p = const_cast<uint8_t*>(pData) + nDmxAddress - 1;

		for (uint32_t i = 0; i < m_nDmxFootprintPortB; i++) {
			if (nDmxAddress++ > nLength) {
				break;
			}
			if ((*p & 0x80) != 0) {	// 0-127 is off, 128-255 is on
				nPortData = nPortData | (1 << i);
			}
			p++;
		}

		if (nPortData != m_nDataPortB) {
			m_nDataPortB = nPortData;
			m_pBoard->IOWrite(SLUSH_IO_PORTB, nPortData);
#ifndef NDEBUG
			printf("\tPort B: DMX data has changed! %.2X\n", nPortData);
#endif
		} else {
#ifndef NDEBUG
			printf("\tPort B: Nothing to do..\n");
#endif
		}
	}

	DEBUG_EXIT;
}

bool SlushDmx::SetDmxStartAddress(uint16_t nDmxStartAddress) {
	DEBUG_ENTRY;

	if (nDmxStartAddress == m_nDmxStartAddress) {
		return true;
	}

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if (m_pL6470DmxModes[i] != nullptr) {
			const uint16_t nCurrentDmxStartAddress = m_pL6470DmxModes[i]->GetDmxStartAddress();
			const uint16_t nNewDmxStartAddress =  (nCurrentDmxStartAddress - m_nDmxStartAddress) + nDmxStartAddress;
#ifndef NDEBUG
			printf("\tMotor=%d, Current: DMX Start Address=%d, New: DMX Start Address=%d\n", i, nCurrentDmxStartAddress, nNewDmxStartAddress);
#endif
			m_pL6470DmxModes[i]->SetDmxStartAddress(nNewDmxStartAddress);
		}
	}

#ifndef NDEBUG
	printf("Current: m_nDmxStartAddressPortA=%d, m_nDmxStartAddressPortB=%d\n", m_nDmxStartAddressPortA, m_nDmxStartAddressPortB);
#endif

	m_nDmxStartAddressPortA = (m_nDmxStartAddressPortA - m_nDmxStartAddress) +  nDmxStartAddress;
	m_nDmxStartAddressPortB = (m_nDmxStartAddressPortB - m_nDmxStartAddress) +  nDmxStartAddress;

#ifndef NDEBUG
	printf("New: m_nDmxStartAddressPortA=%d, m_nDmxStartAddressPortB=%d\n", m_nDmxStartAddressPortA, m_nDmxStartAddressPortB);
#endif

	m_nDmxStartAddress = nDmxStartAddress;

	DEBUG_EXIT
	return true;
}

bool SlushDmx::GetSlotInfo(uint16_t nSlotOffset, SlotInfo& tSlotInfo) {
	DEBUG2_ENTRY;

	if (nSlotOffset >  m_nDmxFootprint) {
		DEBUG2_EXIT
		return false;
	}

	const uint16_t nDmxAddress = m_nDmxStartAddress + nSlotOffset;

	for (uint32_t i = 0; i < SLUSH_DMX_MAX_MOTORS; i++) {
		if ((m_pL6470DmxModes[i] != nullptr) && (m_pSlotInfo[i] != nullptr)) {
			const int16_t nOffset = nDmxAddress - m_pL6470DmxModes[i]->GetDmxStartAddress();

			if ((nDmxAddress >= m_pL6470DmxModes[i]->GetDmxStartAddress()) && (nOffset < m_pL6470DmxModes[i]->GetDmxFootPrint())) {

				tSlotInfo.nType = m_pSlotInfo[i][nOffset].nType;
				tSlotInfo.nCategory = m_pSlotInfo[i][nOffset].nCategory;

				DEBUG2_EXIT
				return true;
			}
		}
	}

	int16_t nOffset = nDmxAddress - m_nDmxStartAddressPortA;

#ifndef NDEBUG
	printf("\t\tnDmxAddress=%d, nOffset=%d\n", nDmxAddress, static_cast<int>(nOffset));
	printf("\t\tm_bSetPortA=%d, m_nDmxStartAddressPortA=%d, m_nDmxFootprintPortA=%d\n",m_bSetPortA,m_nDmxStartAddressPortA, m_nDmxFootprintPortA);
#endif

	if (m_bSetPortA && (nDmxAddress >= m_nDmxStartAddressPortA) && (nOffset < m_nDmxFootprintPortA)) {

		tSlotInfo.nType = m_pSlotInfoPortA[nOffset].nType;
		tSlotInfo.nCategory = m_pSlotInfoPortA[nOffset].nCategory;

		DEBUG2_EXIT
		return true;
	}

	nOffset = nDmxAddress - m_nDmxStartAddressPortB;

#ifndef NDEBUG
	printf("\t\tm_bSetPortB=%d, m_nDmxStartAddressPortB=%d, m_nDmxFootprintPortB=%d\n",m_bSetPortB,m_nDmxStartAddressPortB, m_nDmxFootprintPortB);
#endif

	if (m_bSetPortB && (nDmxAddress >= m_nDmxStartAddressPortB) && (nOffset < m_nDmxFootprintPortB)) {

		tSlotInfo.nType = m_pSlotInfoPortB[nOffset].nType;
		tSlotInfo.nCategory = m_pSlotInfoPortB[nOffset].nCategory;

		DEBUG2_EXIT
		return true;
	}

	DEBUG2_EXIT
	return false;
}
#endif
