/**
 * @file get_uptime.cpp
 *
 */
/* Copyright (C) 2024 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <cstdio>

#if defined (__APPLE__)
# include <time.h>
# include <sys/sysctl.h>
#else
# include <sys/sysinfo.h>
#endif

namespace hal {
uint32_t get_uptime() {
#if defined (__APPLE__)
	struct timeval boottime;
	size_t len = sizeof(boottime);
	int mib[2] = {CTL_KERN, KERN_BOOTTIME};

	if (sysctl(mib, 2, &boottime, &len, nullptr, 0) < 0 ) {
		return 0;
	}

	time_t bsec = boottime.tv_sec;
	time_t csec = time(nullptr);

	return difftime(csec, bsec);
#else
	struct sysinfo s_info;
	int error = sysinfo(&s_info);

	if (error != 0) {
		printf("code error = %d\n", error);
	}

	return static_cast<uint32_t>(s_info.uptime);
#endif
}
}  // namespace hal

