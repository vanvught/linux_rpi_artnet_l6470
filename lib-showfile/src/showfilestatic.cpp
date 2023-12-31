/**
 * @file showfilestatic.cpp
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

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <cassert>

#include "showfile.h"
#include "showfileconst.h"

#include "debug.h"

showfile::Formats ShowFile::GetFormat(const char *pString) {
	assert(pString != nullptr);

	for (auto i = 0; i < static_cast<int>(showfile::Formats::UNDEFINED); i++) {
		if (strcasecmp(pString, ShowFileConst::FORMAT[i]) == 0) {
			return static_cast<showfile::Formats>(i);
		}
	}

	return showfile::Formats::UNDEFINED;
}

const char *ShowFile::GetFormat(showfile::Formats Format) {
	if (Format < showfile::Formats::UNDEFINED) {
		return ShowFileConst::FORMAT[static_cast<unsigned>(Format)];
	}

	return "Unknown";
}

bool ShowFile::ShowFileNameCopyTo(char *pShowFileName, uint32_t nLength, uint32_t nShowFileNumber) {
	assert(nLength == showfile::File::NAME_LENGTH + 1);

	if (nShowFileNumber < showfile::File::MAX_NUMBER) {
		snprintf(pShowFileName, nLength, "show%.2d.txt", nShowFileNumber);
		return true;
	}

	return false;
}

bool ShowFile::CheckShowFileName(const char *pShowFileName, uint32_t &nShowFileNumber) {
	DEBUG_PRINTF("pShowFileName=[%s]", pShowFileName);

	if ((pShowFileName == nullptr) || (strlen(pShowFileName) != showfile::File::NAME_LENGTH)) {
		DEBUG_EXIT
		return false;
	}

	if (memcmp(pShowFileName, SHOWFILE_PREFIX, sizeof(SHOWFILE_PREFIX) - 1) != 0) {
		DEBUG_EXIT
		return false;
	}

	if (memcmp(&pShowFileName[showfile::File::NAME_LENGTH - sizeof(SHOWFILE_SUFFIX) + 1], SHOWFILE_SUFFIX, sizeof(SHOWFILE_SUFFIX) - 1) != 0) {
		DEBUG_EXIT
		return false;
	}

	char cDigit = pShowFileName[sizeof(SHOWFILE_PREFIX) - 1];
	DEBUG_PRINTF("cDigit=%c", cDigit);

	if (!isdigit(cDigit)) {
		DEBUG_EXIT
		return false;
	}

	nShowFileNumber = static_cast<uint32_t>(10 * (cDigit - '0'));

	cDigit = pShowFileName[sizeof(SHOWFILE_PREFIX)];
	DEBUG_PRINTF("cDigit=%c", cDigit);

	if (!isdigit(cDigit)) {
		DEBUG_EXIT
		return false;
	}

	nShowFileNumber += static_cast<uint32_t>(cDigit - '0');

	DEBUG_EXIT
	return true;
}
