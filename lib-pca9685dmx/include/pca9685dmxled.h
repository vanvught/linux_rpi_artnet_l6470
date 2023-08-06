/**
 * @file pca9685dmxled.h
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

#ifndef PCA9685DMXLED_H_
#define PCA9685DMXLED_H_

#include <cstdint>

#include "lightset.h"

#include "pca9685pwmled.h"

class PCA9685DmxLed final: public LightSet {
public:
	PCA9685DmxLed();
	~PCA9685DmxLed() override;

	void Start(uint32_t nPortIndex = 0) override;
	void Stop(uint32_t nPortIndex = 0) override;

	void SetData(uint32_t nPortIndex, const uint8_t *pDmxData, uint32_t nLength, const bool doUpdate = true) override;
	void Sync(const uint32_t nPortIndex) override;
	void Sync(const bool doForce = false) override;

public: // RDM
	bool SetDmxStartAddress(uint16_t nDmxStartAddress) override;

	uint16_t GetDmxStartAddress() override {
		return m_nDmxStartAddress;
	}

	uint16_t GetDmxFootprint() override {
		return m_nDmxFootprint;
	}

	void SetSlotInfoRaw(const char *pSlotInfoRaw);

	bool GetSlotInfo(uint16_t nSlotOffset, lightset::SlotInfo &tSlotInfo) override;

public:
	uint8_t GetI2cAddress() const;
	void SetI2cAddress(uint8_t nI2cAddress);

	uint8_t GetBoardInstances() {
		return m_nBoardInstances;
	}
	void SetBoardInstances(uint8_t nBoardInstances);

	uint16_t GetPwmfrequency() {
		return m_nPwmFrequency;
	}
	void SetPwmfrequency(uint16_t nPwmfrequency);

	bool GetInvert() const;
	void SetInvert(bool bOutputInvert);

	bool GetOutDriver() const;
	void SetOutDriver(bool bOutputDriver);

	void SetDmxFootprint(uint16_t nDmxFootprint);

private:
	void Initialize();

private:
	uint16_t m_nDmxStartAddress { 1 };
	uint16_t m_nDmxFootprint { PCA9685_PWM_CHANNELS };
	uint8_t m_nI2cAddress { PCA9685_I2C_ADDRESS_DEFAULT };
	uint8_t m_nBoardInstances { 1 };
	uint16_t m_nPwmFrequency { PWMLED_DEFAULT_FREQUENCY };
	bool m_bOutputInvert { false };
	bool m_bOutputDriver { true };
	bool m_bIsStarted { false };
	PCA9685PWMLed **m_pPWMLed { nullptr };
	uint8_t *m_pDmxData { nullptr };
	char *m_pSlotInfoRaw { nullptr };
	lightset::SlotInfo *m_pSlotInfo { nullptr };
};

#endif /* PCA9685DMXLED_H_ */
