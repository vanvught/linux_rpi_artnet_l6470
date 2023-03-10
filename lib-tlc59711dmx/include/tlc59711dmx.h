/**
 * @file tlc59711dmx.h
 *
 */
/* Copyright (C) 2018-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef TLC59711DMX_H_
#define TLC59711DMX_H_

#include <cstdint>

#include "lightset.h"

#include "tlc59711.h"
#include "tlc59711dmxstore.h"

namespace tlc59711 {
enum class Type {
	RGB,
	RGBW,
	UNDEFINED
};
}  // namespace tlc59711

class TLC59711Dmx final: public LightSet {
public:
	TLC59711Dmx();
	~TLC59711Dmx() override;

	void Start(uint32_t nPortIndex = 0) override;
	void Stop(uint32_t nPortIndex = 0) override;

	void SetData(uint32_t nPortIndex, const uint8_t *pDmxData, uint32_t nLength) override;

	void Blackout(bool bBlackout) override;

	void Print() override;

	void SetLEDType(tlc59711::Type type);
	tlc59711::Type GetLEDType() {
		return m_LEDType;
	}

	void SetLEDCount(uint8_t nLEDCount);
	uint8_t GetLEDCount() const {
		return m_nLEDCount;
	}

	void SetSpiSpeedHz(uint32_t nSpiSpeedHz);
	uint32_t GetSpiSpeedHz() const {
		return m_nSpiSpeedHz;
	}

	void SetTLC59711DmxStore(TLC59711DmxStore *pTLC59711Store) {
		m_pTLC59711DmxStore = pTLC59711Store;
	}

public: // RDM
	bool SetDmxStartAddress(uint16_t nDmxStartAddress) override;

	uint16_t GetDmxStartAddress() override {
		return m_nDmxStartAddress;
	}

	uint16_t GetDmxFootprint() override {
		return m_nDmxFootprint;
	}

	bool GetSlotInfo(uint16_t nSlotOffset, lightset::SlotInfo &tSlotInfo) override;

public:
	uint8_t GetBoardInstances() const {
		return m_nBoardInstances;
	}

private:
	void Initialize();
	void UpdateMembers();

private:
	uint16_t m_nDmxStartAddress { 1 };
	uint16_t m_nDmxFootprint;
	uint8_t m_nBoardInstances { 1 };
	bool m_bIsStarted { false };
	bool m_bBlackout { false };
	TLC59711 *m_pTLC59711 { nullptr };
	uint32_t m_nSpiSpeedHz { 0 };
	tlc59711::Type m_LEDType {tlc59711::Type::RGB };
	uint8_t m_nLEDCount;

	TLC59711DmxStore *m_pTLC59711DmxStore { nullptr };
};

#endif /* TLC59711DMX_H_ */
