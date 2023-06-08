/*
 * rdm_manufacturer_pid.cpp
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>

#include "rdm_manufacturer_pid.h"
#include "rdm_e120.h"

#include "pixeltype.h"

namespace rdm {
using E120_MANUFACTURER_PIXEL_TYPE = ManufacturerPid<0x8500>;
using E120_MANUFACTURER_PIXEL_COUNT = ManufacturerPid<0x8501>;
using E120_MANUFACTURER_PIXEL_GROUPING_COUNT = ManufacturerPid<0x8502>;
using E120_MANUFACTURER_PIXEL_MAP = ManufacturerPid<0x8503>;

struct PixelType {
    static constexpr char description[] = "Pixel type";
};

struct PixelCount {
    static constexpr char description[] = "Pixel count";
};

struct PixelGroupingCount {
    static constexpr char description[] = "Pixel grouping count";
};

struct PixelMap {
    static constexpr char description[] = "Pixel map";
};

constexpr char PixelType::description[];
constexpr char PixelCount::description[];
constexpr char PixelGroupingCount::description[];
constexpr char PixelMap::description[];

static const constexpr ParameterDescription PARAMETER_DESCRIPTIONS[] = {
		  { E120_MANUFACTURER_PIXEL_TYPE::code,
			DEVICE_DESCRIPTION_MAX_LENGTH,
			E120_DS_ASCII,
			E120_CC_GET,
			0,
			E120_UNITS_NONE,
			E120_PREFIX_NONE,
			0,
			0,
			0,
			Description<PixelType, sizeof(PixelType::description)>::value,
			pdlParameterDescription(sizeof(PixelType::description))
		  },
		  { E120_MANUFACTURER_PIXEL_COUNT::code,
			2,
			E120_DS_UNSIGNED_DWORD,
			E120_CC_GET,
			0,
			E120_UNITS_NONE,
			E120_PREFIX_NONE,
			0,
			__builtin_bswap32(pixel::defaults::COUNT),
			__builtin_bswap32(pixel::max::ledcount::RGB),
			Description<PixelCount, sizeof(PixelCount::description)>::value,
			pdlParameterDescription(sizeof(PixelCount::description))
		  },
		  { E120_MANUFACTURER_PIXEL_GROUPING_COUNT::code,
			2,
			E120_DS_UNSIGNED_DWORD,
			E120_CC_GET,
			0,
			E120_UNITS_NONE,
			E120_PREFIX_NONE,
			0,
			__builtin_bswap32(pixel::defaults::COUNT),
			__builtin_bswap32(pixel::max::ledcount::RGB),
			Description<PixelGroupingCount, sizeof(PixelGroupingCount::description)>::value,
			pdlParameterDescription(sizeof(PixelGroupingCount::description))
		  },
		  { E120_MANUFACTURER_PIXEL_MAP::code,
			DEVICE_DESCRIPTION_MAX_LENGTH,
			E120_DS_ASCII,
			E120_CC_GET,
			0,
			E120_UNITS_NONE,
			E120_PREFIX_NONE,
			0,
			0,
			0,
			Description<PixelMap, sizeof(PixelMap::description)>::value,
			pdlParameterDescription(sizeof(PixelMap::description))
		  }
  };

static constexpr uint32_t TABLE_SIZE = sizeof(PARAMETER_DESCRIPTIONS) / sizeof(PARAMETER_DESCRIPTIONS[0]);

const ParameterDescription *getParameterDescriptions(uint32_t& nCount) {
	nCount = TABLE_SIZE;
	return PARAMETER_DESCRIPTIONS;
}

void copyParameterDescription(const uint32_t nIndex, uint8_t *pParamData) {
	assert(nIndex < TABLE_SIZE);

	const auto nSize = sizeof(struct ParameterDescription) - sizeof(const char *) - sizeof(const uint8_t);

	memcpy(pParamData, &PARAMETER_DESCRIPTIONS[nIndex], nSize);
	memcpy(&pParamData[nSize], PARAMETER_DESCRIPTIONS[nIndex].description, PARAMETER_DESCRIPTIONS[nIndex].pdl - nSize);
}

}  // namespace rdm
