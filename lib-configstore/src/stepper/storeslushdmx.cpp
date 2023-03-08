/**
 * @file storeslushdmx.cpp
 *
 */
/* Copyright (C) 2019-2020 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <cassert>

#include "storeslushdmx.h"

#include "slushdmxparams.h"

#include "configstore.h"

#include "debug.h"

using namespace configstore;

StoreSlushDmx *StoreSlushDmx::s_pThis = nullptr;

StoreSlushDmx::StoreSlushDmx() {
	DEBUG_ENTRY

	assert(s_pThis == nullptr);
	s_pThis = this;

	DEBUG_PRINTF("%p", reinterpret_cast<void *>(s_pThis));
	DEBUG_EXIT
}

void StoreSlushDmx::Update(const struct TSlushDmxParams *pSlushDmxParams) {
	DEBUG_ENTRY

	ConfigStore::Get()->Update(Store::SLUSH, pSlushDmxParams, sizeof(struct TSlushDmxParams));

	DEBUG_EXIT
}

void StoreSlushDmx::Copy(struct TSlushDmxParams *pSlushDmxParams) {
	DEBUG_ENTRY

	ConfigStore::Get()->Copy(Store::SLUSH, pSlushDmxParams, sizeof(struct TSlushDmxParams));

	DEBUG_EXIT
}

void StoreSlushDmx::Update(__attribute__((unused)) uint32_t nMotorIndex, __attribute__((unused)) const struct TSlushDmxParams *ptSlushDmxParams) {
	DEBUG_ENTRY

	DEBUG_EXIT
}

void StoreSlushDmx::Copy(__attribute__((unused)) uint32_t nMotorIndex, __attribute__((unused)) struct TSlushDmxParams *ptSlushDmxParams) {
	DEBUG_ENTRY

	DEBUG_EXIT
}
