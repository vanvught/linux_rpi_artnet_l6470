/**
 * @file hardware.h
 *
 */
/* Copyright (C) 2020-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef H3_HARDWARE_H_
#define H3_HARDWARE_H_

#include <cstdint>
#include <time.h>
#include <uuid/uuid.h>

#include "hwclock.h"

#include "h3.h"
#include "h3_watchdog.h"
#include "h3_thermal.h"

#if defined (DEBUG_STACK)
 void stack_debug_run();
#endif

extern "C" {
uint32_t hardware_uptime_seconds(void);
void hardware_led_set(int);
}

class Hardware {
public:
	Hardware();

	void GetUuid(uuid_t out);
	const char *GetMachine(uint8_t &nLength);
	const char *GetSysName(uint8_t &nLength);
	const char *GetBoardName(uint8_t &nLength);
	const char *GetCpuName(uint8_t &nLength);
	const char *GetSocName(uint8_t &nLength);

	uint32_t GetReleaseId() const {
		return 0;	// TODO U-Boot version
	}

	uint32_t GetBoardId() {
	#if defined(ORANGE_PI)
		return 0;
	#elif defined(ORANGE_PI_ONE)
		return 1;
	#else
	 #error Platform not supported
	#endif
	}

	float GetCoreTemperature() {
		return static_cast<float>(h3_thermal_gettemp());
	}

	float GetCoreTemperatureMin() {
		return -40.0f;
	}

	float GetCoreTemperatureMax() {
		return static_cast<float>(h3_thermal_getalarm());
	}

	bool Reboot();

	bool PowerOff() const {
		return false;
	}

	bool SetTime(const struct tm *pTime);
	void GetTime(struct tm *pTime);

	time_t GetTime() {
		return time(nullptr);
	}

	uint32_t GetUpTime() {
		return hardware_uptime_seconds();
	}

	uint32_t Micros() {
		return H3_TIMER->AVS_CNT1;
	}

	uint32_t Millis() {
		return H3_TIMER->AVS_CNT0;
	}

	void WatchdogInit() {
		m_bIsWatchdog = true;
		h3_watchdog_enable();
	}

	void WatchdogFeed() {
		h3_watchdog_restart();
	}

	void WatchdogStop() {
		m_bIsWatchdog = false;
		h3_watchdog_disable();
	}

	bool IsWatchdog() const {
		return m_bIsWatchdog;
	}

	hardware::BootDevice GetBootDevice() const {
		return static_cast<hardware::BootDevice>(h3_get_boot_device());
	}

	const char *GetWebsiteUrl() const {
		return "www.orangepi-dmx.org";
	}

	void SetModeWithLock(hardware::ledblink::Mode mode, bool doLock);
	void SetMode(hardware::ledblink::Mode mode);
	hardware::ledblink::Mode GetMode() const {
		return m_Mode;
	}

	void Run() {
		if (__builtin_expect (m_nTicksPerSecond == 0, 0)) {
			return;
		}

		const auto nMicros = H3_TIMER->AVS_CNT1;

		if (__builtin_expect ((nMicros - m_nMicrosPrevious < m_nTicksPerSecond), 0)) {
			return;
		}

		m_nMicrosPrevious = nMicros;

		m_nToggleLed ^= 0x1;
		hardware_led_set(m_nToggleLed);

#if defined (DEBUG_STACK)
		stack_debug_run();
#endif
	}

	static Hardware *Get() {
		return s_pThis;
	}

private:
	void RebootHandler();

	void SetFrequency(uint32_t nFreqHz) {
		switch (nFreqHz) {
		case 0:
			m_nTicksPerSecond = 0;
			hardware_led_set(0);
			break;
		case 1:
			m_nTicksPerSecond = (1000000 / 1);
			break;
		case 3:
			m_nTicksPerSecond = (1000000 / 3);
			break;
		case 5:
			m_nTicksPerSecond = (1000000 / 5);
			break;
		case 255:
			m_nTicksPerSecond = 0;
			hardware_led_set(1);
			break;
		default:
			m_nTicksPerSecond = (1000000 / nFreqHz);
			break;
		}
	}

private:
#if !defined(DISABLE_RTC)
	HwClock m_HwClock;
#endif
	bool m_bIsWatchdog { false };

	hardware::ledblink::Mode m_Mode { hardware::ledblink::Mode::UNKNOWN };
	bool m_doLock { false };
	uint32_t m_nTicksPerSecond { 0 };
	int32_t m_nToggleLed { 0 };
	uint32_t m_nMicrosPrevious { 0 };

	static Hardware *s_pThis;
};

#endif /* H3_HARDWARE_H_ */
