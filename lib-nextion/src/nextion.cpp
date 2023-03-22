/**
 * @file nextion.cpp
 *
 */
/* Copyright (C) 2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <cstring>
#include <cassert>

#include "nextion.h"
#include "firmwareversion.h"
#include "hardware.h"
#include "hal_uart.h"

#include "debug.h"

namespace nextion {
static constexpr uint32_t BUFFER_SIZE = 128;
}  // namespace nextion

static const uint8_t s_aTermination[3] = { 0xFF, 0xFF, 0xFF};
static char s_uartData[nextion::BUFFER_SIZE];

Nextion::Nextion() {
	DEBUG_ENTRY

	FUNC_PREFIX (uart_begin(EXT_UART_BASE, display::nextion::BAUD, hal::uart::BITS_8, hal::uart::PARITY_NONE, hal::uart::STOP_1BIT));

	while (GetData() != 0)
		;

//	SendCommand("page 1", 6);
//
//	char componentText[] = "title.txt=\"Art-Net 4 Node\"";
//	SendCommand(componentText, sizeof(componentText) - 1);
//
//	SetText("version", FirmwareVersion::Get()->GetPrint());
//	SetValue("uptime", static_cast<uint32_t>(Hardware::Get()->GetUpTime()));

	DEBUG_EXIT
}

void Nextion::SetBacklight(uint32_t nBacklight) {
	if (nBacklight > 100) {
		nBacklight = 100;
	}

	const uint32_t nLength = snprintf(s_uartData, nextion::BUFFER_SIZE - 1, "dim=%u", nBacklight);
	assert(nLength < nextion::BUFFER_SIZE);

	SendCommand(s_uartData, nLength);
}

void Nextion::SetSleep(bool bSleep) {
	const uint32_t nLength = snprintf(s_uartData, nextion::BUFFER_SIZE - 1, "sleep=%d", bSleep);
	assert(nLength < nextion::BUFFER_SIZE);

	SendCommand(s_uartData, nLength);
}

void Nextion::Run() {
	uint32_t nLength;

	if (__builtin_expect(((nLength = GetData()) == 0), 1)) {
		return;
	}

	debug_dump(s_uartData, nLength);
}

uint32_t Nextion::GetData() {
	uint32_t nRead = FUNC_PREFIX (uart_get_rx(EXT_UART_BASE, reinterpret_cast<char *>(s_uartData), nextion::BUFFER_SIZE));

	if (__builtin_expect((nRead <= 0), 1)) {
		return 0;
	}

	uint32_t nSize = nRead;

	while (true) {
		nRead = FUNC_PREFIX (uart_get_rx(EXT_UART_BASE, reinterpret_cast<char *>(&s_uartData[nSize]), nextion::BUFFER_SIZE - nSize));
		if (nRead <= 0) {
			break;
		}
		nSize +=nRead;
	}

	return nSize;
}

void Nextion::SetText(const char *pObjectName, const char *pValue) {
	assert(pObjectName != nullptr);
	assert(pValue != nullptr);

	const uint32_t nLength = snprintf(s_uartData, nextion::BUFFER_SIZE - 1, "%s.txt=\"%s\"", pObjectName, pValue);
	assert(nLength < nextion::BUFFER_SIZE);

	SendCommand(s_uartData, nLength);
}

void Nextion::SetValue(const char *pObjectName, const int32_t nValue) {
	assert(pObjectName != nullptr);

	const uint32_t nLength = snprintf(s_uartData, nextion::BUFFER_SIZE - 1, "%s.val=%d", pObjectName, nValue);
	assert(nLength < nextion::BUFFER_SIZE);

	SendCommand(s_uartData, nLength);
}

void Nextion::SendCommand(const char *pCommand, const uint32_t nLength) {
	assert(pCommand != nullptr);

	DEBUG_PUTS(pCommand);

	FUNC_PREFIX (uart_transmit(EXT_UART_BASE, reinterpret_cast<const uint8_t *>(pCommand), nLength));
	FUNC_PREFIX (uart_transmit(EXT_UART_BASE, s_aTermination, sizeof(s_aTermination)));
}

void Nextion::TextLine(const uint32_t nLine, const char *pText, const uint32_t nLength) {
	if (__builtin_expect((!(nLine <= display::nextion::ROWS)), 0)) {
		return;
	}
	char buffer[display::nextion::COLUMNS + 14];
	const uint32_t nSize = snprintf(buffer, display::nextion::COLUMNS + 13, "line%u.txt=\"%.*s\"", nLine, nLength, pText);
	printf("%s: nLength=%u, nSize=%u\n", __func__, nLength, nSize);
	assert(nSize <= display::nextion::COLUMNS + 14);
	SendCommand(buffer, nSize);
}
