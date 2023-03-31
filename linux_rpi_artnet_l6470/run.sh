#!/bin/bash

#rm -rf ../lib-device/build_linux/
#rm -rf ../lib-device/lib_linux/

rm -rf ../lib-rdmsensor/build_linux/
rm -rf ../lib-rdmsensor/lib_linux/

rm -rf build_linux

make -j 2
retVal=$?
if [ $retVal -ne 0 ]; then
	echo "Error"
	exit $retVal
fi

sudo valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes -v ./linux_rpi_artnet_l6470 bond0
