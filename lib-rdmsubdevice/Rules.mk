EXTRA_INCLUDES+=../lib-rdm/include ../lib-configstore/include ../lib-lightset/include ../lib-properties/include ../lib-device/include

ifneq ($(MAKE_FLAGS),)
	ifeq ($(findstring ENABLE_RDM_SUBDEVICES,$(MAKE_FLAGS)), ENABLE_RDM_SUBDEVICES)
  	ifeq ($(findstring CONFIG_RDM_SUBDEVICES_USE_SPI,$(MAKE_FLAGS)), CONFIG_RDM_SUBDEVICES_USE_SPI)
  		EXTRA_SRCDIR+=src/spi
  	endif
  	ifeq ($(findstring CONFIG_RDM_SUBDEVICES_USE_I2C,$(MAKE_FLAGS)), CONFIG_RDM_SUBDEVICES_USE_I2C)
  		EXTRA_SRCDIR+=src/i2c
  	endif
	endif
	ifneq (,$(findstring CONFIG_STORE_USE_ROM,$(MAKE_FLAGS)))
		EXTRA_INCLUDES+=../lib-flashcode/include
	endif
else
	DEFINES+=ENABLE_RDM_SUBDEVICES CONFIG_RDM_SUBDEVICES_USE_I2C CONFIG_RDM_SUBDEVICES_USE_SPI
	EXTRA_SRCDIR+=src/i2c src/spi
endif