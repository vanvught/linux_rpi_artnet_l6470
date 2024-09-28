/**
 * @file display.h
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

#ifndef CUSTOM_DISPLAY_H_
#define CUSTOM_DISPLAY_H_

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>

#include "nextion.h"

class Display: Nextion {
public:
	Display() {
		assert(s_pThis == nullptr);
		s_pThis = this;

		SetContrast(0xFF);
		SetSleep(false);
	}

	void Cls() {
		char buffer[display::nextion::COLUMNS];
		memset(buffer, ' ', display::nextion::COLUMNS);

		for (auto nLine = 1U; nLine <= display::nextion::ROWS; nLine++) {
			Nextion::TextLine(nLine, buffer, display::nextion::COLUMNS);
		}
	}

	void ClearLine(const uint32_t nLine) {
		char buffer[display::nextion::COLUMNS];
		memset(buffer, ' ', display::nextion::COLUMNS);

		Nextion::TextLine(nLine, buffer, display::nextion::COLUMNS);
	}

	void ClearEndOfLine() {
		m_bClearEndOfLine = true;
	}

	int Printf(const uint32_t nLine, const char *format, ...) {
		char buffer[display::nextion::COLUMNS + 1];

		va_list arp;

		va_start(arp, format);

		auto i = vsnprintf(buffer, display::nextion::COLUMNS, format, arp);

		va_end(arp);

		Nextion::TextLine(nLine, buffer, static_cast<uint32_t>(i));

		return i;
	}

	int Write(const uint32_t nLine, const char *pText) {
		const auto *p = pText;
		uint32_t nCount = 0;

		const auto nColumns = display::nextion::COLUMNS;

		while ((*p != 0) && (nCount++ < nColumns)) {
			++p;
		}

		Nextion::TextLine(nLine, pText, nCount);

		return static_cast<int>(nCount);
	}

	void TextStatus(const char *pText) {
		Write(display::nextion::ROWS, pText);
	}

	void TextStatus(const char *pText, uint32_t nConsoleColor) {
		TextStatus(pText);

		if (nConsoleColor == UINT32_MAX) {
			return;
		}

		console_status(nConsoleColor, pText);
	}

	void SetSleepTimeout(uint32_t nTimeout) {
		printf("SetSleepTimeout=%u\n", nTimeout);
	}

	void SetSleep(const bool bSleep) {
		m_bSleep = bSleep;
		Nextion::SetSleep(bSleep);
	}

	bool isSleep() const {
		return m_bSleep;
	}

	void SetFlipVertically([[maybe_unused]] bool sFlipVertically) {}

	bool GetFlipVertically() const {
		return false;
	}

	void SetContrast(const uint8_t nContrast) {
		m_nContrast = nContrast;
		Nextion::SetBacklight((nContrast * 100) / 0xFF);
	}

	uint8_t GetContrast() const {
		return m_nContrast;
	}

	void PrintInfo() {
		Nextion::PrintInfo();
	}

	void Run() {
		Nextion::Run();
	}

	static Display* Get() {
		return s_pThis;
	}

private:
	bool m_bSleep;
	bool m_bClearEndOfLine { false };
	uint8_t m_nContrast;
	static Display *s_pThis;
};

#endif /* CUSTOM_DISPLAY_H_ */
