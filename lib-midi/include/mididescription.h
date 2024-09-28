/**
 * @file mididescription.h
 */
/* Copyright (C) 2020 by Arjan van Vught mailto:info@gd32-dmx.org
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

#ifndef MIDIDESCRIPTION_H_
#define MIDIDESCRIPTION_H_

#include <cstdint>

#include "midi.h"

class MidiDescription {
public:
	static const char *GetType(midi::Types tType);
	static const char *GetControlChange(midi::control::Change tControlChange);
	static const char *GetControlFunction(midi::control::Function tControlFunction);
	static const char *GetKeyName(uint8_t nKey);
	static const char *GetDrumKitName(uint8_t nDrumKit);
	static const char *GetInstrumentName(uint8_t nInstrumentName);
};

#endif /* MIDIDESCRIPTION_H_ */
