/**
 * @file nextion.h
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

#ifndef NEXTION_H_
#define NEXTION_H_

#include <cstdint>
#include <cstdio>
#include <cassert>

namespace display {
namespace nextion {
static constexpr uint32_t BAUD = 115200;
static constexpr uint32_t COLUMNS = 32;
static constexpr uint32_t ROWS = 8;
}  // namespace nextion
}  // namespace display

class Nextion {
public:
	Nextion();

	void SetBacklight(uint32_t nBacklight);
	void SetSleep(bool bSleep);
	void TextLine(const uint32_t nLine, const char *pText, const uint32_t nLength);
	void PrintInfo() {
		printf("Nextion baud=%u\n", display::nextion::BAUD);
	}

	void Run();

private:
	uint32_t GetData();
	void SetText(const char *pObjectName, const char *pValue);
	void SetValue(const char *pObjectName, const int32_t nValue);
	void SendCommand(const char *pCommand, const uint32_t nLength);
};

#endif /* NEXTION_H_ */
