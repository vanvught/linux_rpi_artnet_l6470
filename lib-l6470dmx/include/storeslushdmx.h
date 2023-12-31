/**
 * @file storeslushdmx.h
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

#ifndef STORESLUSHDMX_H_
#define STORESLUSHDMX_H_

#include "slushdmxparams.h"
#include "configstore.h"

class StoreSlushDmx {
public:
	static StoreSlushDmx& Get() {
		static StoreSlushDmx instance;
		return instance;
	}

	static void Update(const struct slushdmxparams::Params *pSlushDmxParams) {
		Get().IUpdate(pSlushDmxParams);
	}

	static void Copy(struct slushdmxparams::Params *pSlushDmxParams) {
		Get().ICopy(pSlushDmxParams);
	}

	static void Update(uint32_t nMotorIndex, const struct slushdmxparams::Params *ptSlushDmxParams) {
		Get().IUpdate(nMotorIndex, ptSlushDmxParams);
	}

	static void Copy(uint32_t nMotorIndex, struct slushdmxparams::Params *ptSlushDmxParams) {
		Get().IUpdate(nMotorIndex, ptSlushDmxParams);
	}

private:
	void IUpdate(const struct slushdmxparams::Params *pSlushDmxParams) {
		ConfigStore::Get()->Update(configstore::Store::SLUSH, pSlushDmxParams, sizeof(struct slushdmxparams::Params));
	}

	void ICopy(struct slushdmxparams::Params *pSlushDmxParams) {
		ConfigStore::Get()->Copy(configstore::Store::SLUSH, pSlushDmxParams, sizeof(struct slushdmxparams::Params));
	}

	void IUpdate(__attribute__((unused)) uint32_t nMotorIndex, __attribute__((unused)) const struct slushdmxparams::Params *ptSlushDmxParams) { }

	void ICopy(__attribute__((unused)) uint32_t nMotorIndex, __attribute__((unused)) struct slushdmxparams::Params *ptSlushDmxParams) { }
};

#endif /* STORESLUSHDMX_H_ */
