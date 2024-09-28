/**
 * @file dmx_config.h
 *
 */
/* Copyright (C) 2023-2024 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef H3_SINGLE_DMX_CONFIG_H_
#define H3_SINGLE_DMX_CONFIG_H_

#include "h3_board.h"

#define DMX_MAX_PORTS  1

namespace dmx {
namespace config {
namespace max {
	static const uint32_t PORTS = DMX_MAX_PORTS;
}  // namespace max
}  // namespace config
}  // namespace dmx

namespace dmx {
namespace buffer {
static constexpr auto SIZE = 516;
static constexpr auto INDEX_ENTRIES = (1U << 1);
static constexpr auto INDEX_MASK = (INDEX_ENTRIES - 1);
}  // namespace buffer
}  // namespace dmx

#endif /* H3_SINGLE_DMX_CONFIG_H_ */
