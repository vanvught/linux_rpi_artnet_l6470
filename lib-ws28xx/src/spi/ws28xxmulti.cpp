/**
 * @file ws28xxmulti.cpp
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
# pragma GCC optimize ("O3")
# pragma GCC optimize ("-funroll-loops")
# pragma GCC optimize ("-fprefetch-loop-arrays")
#endif

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>

#include "ws28xxmulti.h"
#include "pixelconfiguration.h"
#include "pixeltype.h"

#include "hal_gpio.h"
#include "hal_spi.h"

#include "jamstapl.h"

#include "debug.h"

using namespace pixel;

WS28xxMulti *WS28xxMulti::s_pThis;

WS28xxMulti::WS28xxMulti(PixelConfiguration& pixelConfiguration): m_PixelConfiguration(pixelConfiguration) {
	DEBUG_ENTRY

	assert(s_pThis == nullptr);
	s_pThis = this;

	uint32_t nLedsPerPixel;
	m_PixelConfiguration.Validate(nLedsPerPixel);

	const auto nCount = m_PixelConfiguration.GetCount();
	m_nBufSize = nCount * nLedsPerPixel;

	const auto type = m_PixelConfiguration.GetType();

	if ((type == Type::APA102) || (type == Type::SK9822) || (type == Type::P9813)) {
		m_nBufSize += nCount;
		m_nBufSize += 8;
	}

	m_nBufSize *= 8;

	DEBUG_PRINTF("m_nBufSize=%d", m_nBufSize);

	const auto nLowCode = m_PixelConfiguration.GetLowCode();
	const auto nHighCode = m_PixelConfiguration.GetHighCode();

	m_hasCPLD = SetupCPLD();

	SetupHC595(ReverseBits(nLowCode), ReverseBits(nHighCode));

	if (m_PixelConfiguration.IsRTZProtocol()) {
		SetupSPI(m_PixelConfiguration.GetClockSpeedHz());
	} else {
		if (m_hasCPLD) {
			SetupSPI(m_PixelConfiguration.GetClockSpeedHz() * 6);
		} else {
			SetupSPI(m_PixelConfiguration.GetClockSpeedHz() * 4);
		}
	}

	m_nBufSize++;

	SetupBuffers();

	printf("Board: %s\n", m_hasCPLD ? "CPLD" : "74-logic");
}

WS28xxMulti::~WS28xxMulti() {
	m_pBlackoutBuffer = nullptr;
	m_pBuffer = nullptr;

	s_pThis = nullptr;
}

void WS28xxMulti::SetupBuffers() {
	DEBUG_ENTRY

	uint32_t nSize;

	m_pBuffer = const_cast<uint8_t*>(FUNC_PREFIX(spi_dma_tx_prepare(&nSize)));
	assert(m_pBuffer != nullptr);

	const uint32_t nSizeHalf = nSize / 2;
	assert(m_nBufSize <= nSizeHalf);

	m_pBlackoutBuffer = m_pBuffer + (nSizeHalf & static_cast<uint32_t>(~3));

	const auto type = m_PixelConfiguration.GetType();
	const auto nCount = m_PixelConfiguration.GetCount();

	if ((type == Type::APA102) || (type == Type::SK9822) || (type == Type::P9813)) {
		DEBUG_PUTS("SPI");

		for (uint32_t nPortIndex = 0; nPortIndex < 8; nPortIndex++) {
			SetPixel(nPortIndex, 0, 0, 0, 0, 0);

			for (uint32_t nPixelIndex = 1; nPixelIndex <= nCount; nPixelIndex++) {
				SetPixel(nPortIndex, nPixelIndex, 0, 0xE0, 0, 0);
			}

			if ((type == Type::APA102) || (type == Type::SK9822)) {
				SetPixel(nPortIndex, 1U + nCount, 0xFF, 0xFF, 0xFF, 0xFF);
			} else {
				SetPixel(nPortIndex, 1U + nCount, 0, 0, 0, 0);
			}
		}
		memcpy(m_pBlackoutBuffer, m_pBuffer, m_nBufSize);
	} else {
		memset(m_pBuffer, 0, m_nBufSize);
		memset(m_pBlackoutBuffer, 0, m_nBufSize);
	}

	DEBUG_PRINTF("nSize=%x, m_pBuffer=%p, m_pBlackoutBuffer=%p", nSize, m_pBuffer, m_pBlackoutBuffer);
	DEBUG_EXIT
}

#define SPI_CS1		GPIO_EXT_26

void WS28xxMulti::SetupHC595(uint8_t nT0H, uint8_t nT1H) {
	DEBUG_ENTRY

	nT0H = static_cast<uint8_t>(nT0H << 1);
	nT1H = static_cast<uint8_t>(nT1H << 1);

	DEBUG_PRINTF("nT0H=%.2x nT1H=%.2x", nT0H, nT1H);

	FUNC_PREFIX(gpio_fsel(SPI_CS1, GPIO_FSEL_OUTPUT));
	FUNC_PREFIX(gpio_set(SPI_CS1));

	FUNC_PREFIX(spi_begin());
	FUNC_PREFIX(spi_chipSelect(SPI_CS_NONE));
	FUNC_PREFIX(spi_set_speed_hz(1000000));

	FUNC_PREFIX(gpio_clr(SPI_CS1));
	FUNC_PREFIX(spi_write(static_cast<uint16_t>((nT1H << 8) | nT0H)));
	FUNC_PREFIX(gpio_set(SPI_CS1));

	DEBUG_EXIT
}

void WS28xxMulti::SetupSPI(uint32_t nSpeedHz) {
	DEBUG_ENTRY

	FUNC_PREFIX(spi_begin());
	FUNC_PREFIX(spi_chipSelect(SPI_CS0));
	FUNC_PREFIX(spi_set_speed_hz(nSpeedHz));

	DEBUG_PRINTF("nSpeedHz=%u", nSpeedHz);
	DEBUG_EXIT
}

extern uint32_t PIXEL8X4_PROGRAM;

extern "C" {
uint32_t getPIXEL8X4_SIZE();
}

bool WS28xxMulti::SetupCPLD() {
	DEBUG_ENTRY

	JamSTAPL jbc(reinterpret_cast<uint8_t*>(&PIXEL8X4_PROGRAM), getPIXEL8X4_SIZE(), true);
	jbc.SetJamSTAPLDisplay(m_pJamSTAPLDisplay);

	if (jbc.PrintInfo() == JBIC_SUCCESS) {
		if ((jbc.CheckCRC() == JBIC_SUCCESS) && (jbc.GetCRC() == 0x1D3C)) {
			jbc.CheckIdCode();
			if (jbc.GetExitCode() == 0) {
				jbc.ReadUsercode();
				if ((jbc.GetExitCode() == 0) && (jbc.GetExportIntegerInt() != 0x0018ad81)) {
					jbc.Program();
				}
				DEBUG_EXIT
				return true;
			}
		}
	}

	DEBUG_EXIT
	return false;
}

uint8_t WS28xxMulti::ReverseBits(uint8_t nBits) {
	const uint32_t input = nBits;
	uint32_t output;
	asm("rbit %0, %1" : "=r"(output) : "r"(input));
	return static_cast<uint8_t>((output >> 24));
}

#define BIT_SET(a,b) 	((a) |= static_cast<uint8_t>((1<<(b))))
#define BIT_CLEAR(a,b) 	((a) &= static_cast<uint8_t>(~(1<<(b))))

void WS28xxMulti::SetColour(uint32_t nPortIndex, uint32_t nPixelIndex, uint8_t nColour1, uint8_t nColour2, uint8_t nColour3) {
	uint32_t j = 0;
	const uint32_t k = nPixelIndex * pixel::single::RGB;

	for (uint8_t mask = 0x80; mask != 0; mask = static_cast<uint8_t>(mask >> 1)) {
		if (mask & nColour1) {
			BIT_SET(m_pBuffer[k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[k + j], nPortIndex);
		}
		if (mask & nColour2) {
			BIT_SET(m_pBuffer[8 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[8 + k + j], nPortIndex);
		}
		if (mask & nColour3) {
			BIT_SET(m_pBuffer[16 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[16 + k + j], nPortIndex);
		}

		j++;
	}
}

void WS28xxMulti::SetPixel4Bytes(uint32_t nPortIndex, uint32_t nPixelIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue, uint8_t nWhite) {
	const auto k = nPixelIndex * pixel::single::RGBW;
	uint32_t j = 0;

	for (uint8_t mask = 0x80; mask != 0; mask = static_cast<uint8_t>(mask >> 1)) {
		// GRBW
		if (mask & nGreen) {
			BIT_SET(m_pBuffer[k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[k + j], nPortIndex);
		}

		if (mask & nRed) {
			BIT_SET(m_pBuffer[8 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[8 + k + j], nPortIndex);
		}

		if (mask & nBlue) {
			BIT_SET(m_pBuffer[16 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[16 + k + j], nPortIndex);
		}

		if (mask & nWhite) {
			BIT_SET(m_pBuffer[24 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[24 + k + j], nPortIndex);
		}

		j++;
	}
}


void WS28xxMulti::SetPixel(uint32_t nPortIndex, uint32_t nPixelIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue) {
	const auto pGammaTable = m_PixelConfiguration.GetGammaTable();

	nRed = pGammaTable[nRed];
	nGreen = pGammaTable[nGreen];
	nBlue = pGammaTable[nBlue];

	const auto type = m_PixelConfiguration.GetType();

	if ((m_PixelConfiguration.IsRTZProtocol()) || (type == Type::WS2801)) {
		SetColour(nPortIndex, nPixelIndex, nRed, nGreen, nBlue);
		return;
	}

	if ((type == Type::APA102) || (type == Type::SK9822)) {
		SetPixel4Bytes(nPortIndex, 1 + nPixelIndex, nBlue, m_PixelConfiguration.GetGlobalBrightness(), nGreen, nRed);
		return;
	}

	if (type == Type::P9813) {
		const auto nFlag = static_cast<uint8_t>(0xC0 | ((~nBlue & 0xC0) >> 2) | ((~nGreen & 0xC0) >> 4) | ((~nRed & 0xC0) >> 6));
		SetPixel4Bytes(nPortIndex, 1+ nPixelIndex, nRed, nFlag, nGreen, nBlue);
		return;
	}

	assert(0);
	__builtin_unreachable();
}

void WS28xxMulti::SetPixel(uint32_t nPortIndex, uint32_t nPixelIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue, uint8_t nWhite) {
	const auto pGammaTable = m_PixelConfiguration.GetGammaTable();

	nRed = pGammaTable[nRed];
	nGreen = pGammaTable[nGreen];
	nBlue = pGammaTable[nBlue];
	nWhite = pGammaTable[nWhite];

	const auto k = nPixelIndex * pixel::single::RGBW;
	uint32_t j = 0;

	for (uint8_t mask = 0x80; mask != 0; mask = static_cast<uint8_t>(mask >> 1)) {
		// GRBW
		if (mask & nGreen) {
			BIT_SET(m_pBuffer[k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[k + j], nPortIndex);
		}

		if (mask & nRed) {
			BIT_SET(m_pBuffer[8 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[8 + k + j], nPortIndex);
		}

		if (mask & nBlue) {
			BIT_SET(m_pBuffer[16 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[16 + k + j], nPortIndex);
		}

		if (mask & nWhite) {
			BIT_SET(m_pBuffer[24 + k + j], nPortIndex);
		} else {
			BIT_CLEAR(m_pBuffer[24 + k + j], nPortIndex);
		}

		j++;
	}
}

void WS28xxMulti::Update() {
	assert(!FUNC_PREFIX(spi_dma_tx_is_active()));

	FUNC_PREFIX(spi_dma_tx_start(m_pBuffer, m_nBufSize));
}

void WS28xxMulti::Blackout() {
	DEBUG_ENTRY

	// Can be called any time.
	do {
		asm volatile ("isb" ::: "memory");
	} while (FUNC_PREFIX(spi_dma_tx_is_active()));

	FUNC_PREFIX(spi_dma_tx_start(m_pBlackoutBuffer, m_nBufSize));

	// A blackout may not be interrupted.
	do {
		asm volatile ("isb" ::: "memory");
	} while (FUNC_PREFIX(spi_dma_tx_is_active()));

	DEBUG_EXIT
}

void WS28xxMulti::FullOn() {
	DEBUG_ENTRY

	// Can be called any time.
	do {
		asm volatile ("isb" ::: "memory");
	} while (FUNC_PREFIX(spi_dma_tx_is_active()));

	const auto type = m_PixelConfiguration.GetType();

	if ((type == Type::APA102) || (type == Type::SK9822) || (type == Type::P9813)) {
		for (uint32_t nPortIndex = 0; nPortIndex < 8; nPortIndex++) {
			SetPixel(nPortIndex, 0, 0, 0, 0, 0);

			const auto nCount = m_PixelConfiguration.GetCount();

			for (uint32_t nPixelIndex = 1; nPixelIndex <= nCount; nPixelIndex++) {
				SetPixel(nPortIndex, nPixelIndex, 0xFF, 0xE0, 0xFF, 0xFF);
			}

			if ((type == Type::APA102) || (type == Type::SK9822)) {
				SetPixel(nPortIndex, 1U + nCount, 0xFF, 0xFF, 0xFF, 0xFF);
			} else {
				SetPixel(nPortIndex, 1U + nCount, 0, 0, 0, 0);
			}
		}
	} else {
		memset(m_pBuffer, 0xFF, m_nBufSize);
	}

	Update();

	// May not be interrupted.
	do {
		asm volatile ("isb" ::: "memory");
	} while (FUNC_PREFIX(spi_dma_tx_is_active()));

	DEBUG_EXIT
}
