/**
 * @file l6470commands.cpp
 *
 */
/* Copyright (C) 2017-2020 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <cstdio>

#include "l6470.h"
#include "l6470constants.h"

void L6470::setParam(TL6470ParamRegisters param, unsigned long value) {
	SPIXfer(param | L6470_CMD_SET_PARAM);
	paramHandler(param, value);
}

long L6470::getParam(TL6470ParamRegisters param) {
	SPIXfer(param | L6470_CMD_GET_PARAM);

	return paramHandler(param, 0);
}

long L6470::getPos() {
	long temp = getParam(L6470_PARAM_ABS_POS);
	// Since ABS_POS is a 22-bit 2's comp value, we need to check bit 21 and, if
	//  it's set, set all the bits ABOVE 21 in order for the value to maintain
	//  its appropriate sign.
	if (temp & 0x00200000) {
		temp |= static_cast<long>(0xffc00000);
	}

	return temp;
}

long L6470::getMark() {
	long temp = getParam(L6470_PARAM_MARK);
	// Since ABS_POS is a 22-bit 2's comp value, we need to check bit 21 and, if
	//  it's set, set all the bits ABOVE 21 in order for the value to maintain
	//  its appropriate sign.
	if (temp & 0x00200000) {
		temp |= static_cast<long>(0xffC00000);
	}

	return temp;
}

void L6470::run(TL6470Direction dir, float stepsPerSec) {
	SPIXfer(L6470_CMD_RUN | dir);
	unsigned long integerSpeed = spdCalc(stepsPerSec);
	if (integerSpeed > 0xFFFFF)
		integerSpeed = 0xFFFFF;

	// Now we need to push this value out to the dSPIN. The 32-bit value is
	//  stored in memory in little-endian format, but the dSPIN expects a
	//  big-endian output, so we need to reverse the uint8_t-order of the
	//  data as we're sending it out. Note that only 3 of the 4 bytes are
	//  valid here.

	// We begin by pointing bytePointer at the first uint8_t in integerSpeed.
	auto *bytePointer = reinterpret_cast<uint8_t*>(&integerSpeed);
	// Next, we'll iterate through a for loop, indexing across the bytes in
	//  integerSpeed starting with uint8_t 2 and ending with uint8_t 0.
	for (int8_t i = 2; i >= 0; i--) {
		SPIXfer(bytePointer[i]);
	}
}

void L6470::stepClock(TL6470Direction dir) {
	SPIXfer(L6470_CMD_STEP_CLOCK | dir);
}

void L6470::move(TL6470Direction dir, unsigned long numSteps) {
	SPIXfer(L6470_CMD_MOVE | dir);
	if (numSteps > 0x3FFFFF)
		numSteps = 0x3FFFFF;

	auto *bytePointer = reinterpret_cast<uint8_t*>(&numSteps);
	for (int8_t i = 2; i >= 0; i--) {
		SPIXfer(bytePointer[i]);
	}
}

void L6470::goTo(long pos) {
	SPIXfer(L6470_CMD_GOTO);
	if (pos > 0x3FFFFF)
		pos = 0x3FFFFF;

	auto *bytePointer = reinterpret_cast<uint8_t*>(&pos);
	for (int8_t i = 2; i >= 0; i--) {
		SPIXfer(bytePointer[i]);
	}
}

void L6470::goToDir(TL6470Direction dir, long pos) {
	SPIXfer(L6470_CMD_GOTO_DIR | dir);
	if (pos > 0x3FFFFF)
		pos = 0x3FFFFF;

	auto *bytePointer = reinterpret_cast<uint8_t*>(&pos);
	for (int8_t i = 2; i >= 0; i--) {
		SPIXfer(bytePointer[i]);
	}
}

void L6470::goUntil(TL6470Action action, TL6470Direction dir, float stepsPerSec) {
	SPIXfer(L6470_CMD_GO_UNTIL | action | dir);
	unsigned long integerSpeed = spdCalc(stepsPerSec);
	if (integerSpeed > 0x3FFFFF)
		integerSpeed = 0x3FFFFF;

	auto *bytePointer = reinterpret_cast<uint8_t*>(&integerSpeed);
	for (int8_t i = 2; i >= 0; i--) {
		SPIXfer(bytePointer[i]);
	}
}

void L6470::releaseSw(TL6470Action action, TL6470Direction dir) {
	SPIXfer(L6470_CMD_RELEASE_SW | action | dir);
}

void L6470::goHome() {
	SPIXfer(L6470_CMD_GO_HOME);
}

void L6470::goMark() {
	SPIXfer(L6470_CMD_GO_MARK);
}

void L6470::setMark(long newMark) {
	setParam(L6470_PARAM_MARK, static_cast<unsigned long>(newMark));
}

void L6470::setPos(long newPos) {
	setParam(L6470_PARAM_ABS_POS, static_cast<unsigned long>(newPos));
}

void L6470::resetPos() {
	SPIXfer(L6470_CMD_RESET_POS);
}

void L6470::resetDev() {
	SPIXfer(L6470_CMD_RESET_DEVICE);
}

void L6470::softStop() {
	SPIXfer(L6470_CMD_SOFT_STOP);
}

void L6470::hardStop() {
	SPIXfer(L6470_CMD_HARD_STOP);
}

void L6470::softHiZ() {
	SPIXfer(L6470_CMD_SOFT_HIZ);
}

void L6470::hardHiZ() {
	SPIXfer(L6470_CMD_HARD_HIZ);
}

int L6470::getStatus() {
	int temp = 0;

	auto *bytePointer = reinterpret_cast<uint8_t*>(&temp);
	SPIXfer(L6470_CMD_GET_STATUS);
	bytePointer[1] = SPIXfer(0);
	bytePointer[0] = SPIXfer(0);

	return temp;
}
