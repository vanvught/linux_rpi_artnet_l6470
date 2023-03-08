/**
 * @file serial.h
 *
 */
/* Copyright (C) 2020 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef SERIAL_H_
#define SERIAL_H_

#include <cstdint>

#include "hal_uart.h"
#include "hal_spi.h"
#include "hal_i2c.h"

namespace serial {
enum type : uint8_t {
	UART, SPI, I2C, UNDEFINED
};
namespace uart {
enum parity : uint8_t {
	NONE, ODD, EVEN, UNDEFINED
};
}  // namespace uart
namespace i2c {
enum speed : uint8_t {
	NORMAL, FAST, UNDEFINED
};
}  // namespace i2c
}  // namespace serial

class Serial {
public:
	Serial();
	~Serial();

	void SetType(serial::type tType = serial::type::UART) {
		if (tType < serial::type::UNDEFINED) {
			m_tType = serial::type::UART;
		}
	}

	serial::type GetType() {
		return m_tType;
	}

	/*
	 * UART
	 */
	void SetUartBaud(uint32_t nBaud);
	void SetUartBits(uint32_t nBits);
	void SetUartParity(serial::uart::parity tParity = serial::uart::parity::NONE);
	void SetUartStopBits(uint32_t nStopBits);

	/*
	 * SPI
	 */
	void SetSpiSpeedHz(uint32_t nSpeedHz);
	void SetSpiMode(uint32_t nMode = SPI_MODE0);

	/*
	 * I2C
	 */
	void SetI2cAddress(uint8_t nAddress);
	void SetI2cSpeedMode(serial::i2c::speed tSpeedMode);

	bool Init();

	void Print();

	/*
	 * Send the data
	 */
	void Send(const uint8_t *pData, uint32_t nLength);

	static const char *GetType(serial::type tType);
	static serial::type GetType(const char *pType);

	static const char *GetUartParity(serial::uart::parity tParity);
	static const char *GetUartParity(uint8_t nParity) {
		return GetUartParity(static_cast<serial::uart::parity>(nParity));
	}
	static serial::uart::parity GetUartParity(const char *pParity);

	static const char *GetI2cSpeedMode(serial::i2c::speed tSpeed);
	static serial::i2c::speed GetI2cSpeedMode(const char *pSpeed);

	static const char *GetI2cSpeedMode(uint32_t nSpeed);

	static Serial *Get() {
		return s_pThis;
	}

private:
	bool InitUart();
	void SendUart(const uint8_t *pData, uint32_t nLength);

	bool InitSpi();
	void SendSpi(const uint8_t *pData, uint32_t nLength);

	bool InitI2c();
	void SendI2c(const uint8_t *pData, uint32_t nLength);

private:
	serial::type m_tType{serial::type::UART};
	struct {
		uint32_t nBaud;
		uint8_t nBits;
		uint8_t nParity;
		uint8_t nStopBits;
	} m_UartConfiguration;
	struct {
		uint32_t nSpeed;
		uint8_t nMode;
	} m_SpiConfiguration;
	struct {
		uint32_t nSpeed;
		uint8_t nAddress;
	} m_I2cConfiguration;

	static Serial *s_pThis;
};

#endif /* SERIAL_H_ */
