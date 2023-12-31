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

#ifndef SLUSHDMXPARAMS_H_
#define SLUSHDMXPARAMS_H_

#include <cstdint>

#include "slushdmx.h"

namespace slushdmxparams {
struct Params {
	uint32_t nSetList;
	uint8_t nUseSpiBusy;
	uint16_t nDmxStartAddressPortA;
	uint8_t nDmxFootprintPortA;
	uint16_t nDmxStartAddressPortB;
	uint8_t nDmxFootprintPortB;
} __attribute__((packed));

struct Mask {
	static constexpr uint32_t USE_SPI_BUSY = (1U << 0);
	static constexpr uint32_t START_ADDRESS_PORT_A = (1U << 1);
	static constexpr uint32_t FOOTPRINT_PORT_A = (1U << 2);
	static constexpr uint32_t START_ADDRESS_PORT_B = (1U << 3);
	static constexpr uint32_t FOOTPRINT_PORT_B = (1U << 4);
};
}  // namespace slushdmxparams

class SlushDmxParams {
public:
	SlushDmxParams();

	void Load();
	void Load(const char *pBuffer, uint32_t nLength);

	void Set(SlushDmx *pSlushDmx);

	void Builder(const struct slushdmxparams::Params *ptSlushDmxParams, char *pBuffer, uint32_t nLength, uint32_t& nSize);
	void Save(char *pBuffer, uint32_t nLength, uint32_t& nSize);

    static void staticCallbackFunction(void *p, const char *s);

private:
	void Dump();
    void callbackFunction(const char *pLine);
    bool isMaskSet(uint32_t nMask) const {
    	return (m_Params.nSetList & nMask) == nMask;
    }

private:
    slushdmxparams::Params m_Params;
    char m_aFileName[16];
};

#endif /* SLUSHDMXPARAMS_H_ */
