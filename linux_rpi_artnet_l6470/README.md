Prerequisite: [C library for Broadcom BCM 2835 as used in Raspberry Pi](https://www.airspayce.com/mikem/bcm2835/)

Building the executable:

	cd linux_rpi_artnet_l6470
	make -j clean && make -j

Example output [last lines]:
	
	$TARGET [linux_rpi_artnet_l6470]
	g++    build_linux/src/main.o build_linux/lib/artnetdisplay.o build_linux/lib/rdmsoftwareversion.o -o linux_rpi_artnet_l6470 -L../lib-rdmresponder/lib_linux -L../lib-rdm/lib_linux -L../lib-rdmsensor/lib_linux -L../lib-rdmsubdevice/lib_linux -L../lib-remoteconfig/lib_linux -L../lib-artnet4/lib_linux -L../lib-artnet/lib_linux -L../lib-e131/lib_linux -L../lib-rdm/lib_linux -L../lib-dmx/lib_linux -L../lib-l6470dmx/lib_linux -L../lib-l6470/lib_linux -L../lib-tlc59711dmx/lib_linux -L../lib-tlc59711/lib_linux -L../lib-configstore/lib_linux -L../lib-network/lib_linux -L../lib-displayudf/lib_linux -L../lib-display/lib_linux -L../lib-properties/lib_linux -L../lib-lightset/lib_linux -L../lib-device/lib_linux -L../lib-hal/lib_linux -L../lib-debug/lib_linux -L../lib-bcm2835_raspbian/lib_linux  -lrdmresponder -lrdm -lrdmsensor -lrdmsubdevice -lremoteconfig -lartnet4 -lartnet -le131 -lrdm -ldmx -ll6470dmx -ll6470 -ltlc59711dmx -ltlc59711 -lconfigstore -lnetwork -ldisplayudf -ldisplay -lproperties -llightset -ldevice -lhal -ldebug -lbcm2835_raspbian -luuid -lpthread
	objdump -d linux_rpi_artnet_l6470 | c++filt > linux.lst
	
Usage: 

	./linux_rpi_artnet_l6470 ip_address|interface_name
	
Example: 

	sudo ./linux_rpi_artnet_l6470 bond0
	gpio
	SSD1306 (8,21)
	[V0.1] Raspberry Pi 3 Model B Plus V1.3 Compiled on Mar  9 202
	Art-Net 4 Node Stepper L6470
	Network init
	Network
	 Hostname  : raspberrypi-3
	 Domain    : 
	 If        : 4: bond0
	 Inet      : 192.168.2.121/24
	 Netmask   : 255.255.255.0
	 Gateway   : 192.168.2.1
	 Broadcast : 192.168.2.255
	 Mac       : b8:27:eb:96:9e:69
	 Mode      : S
	SparkFun init
	Motor 0 is connected
	Skipping Motor 1
	Skipping Motor 2
	Skipping Motor 3
	Skipping Motor 4
	Skipping Motor 5
	Skipping Motor 6
	Skipping Motor 7
	InitSwitch()
	 Motor 0
	busyCheck()
	 Motor 0
	InitPos()
	 Motor 0
	Configuring Art-Net
	RDM Device configuration
	 Manufacturer Name : www.orangepi-dmx.org
	 Manufacturer ID   : 5000
	 Serial Number     : C0A80279
	 Root label        : Raspberry Pi RDM Device
	 Product Category  : 7FFF
	 Product Detail    : 7FFF
	RDM Responder configuration
	 Protocol Version 1.0
	 DMX Address      : 1
	 DMX Footprint    : 12
	 Personality 1 of 1 [Sparkfun [1] with TLC59711 [4]]
	 Sub Devices      : 0
	 Sensors          : 1
	Art-Net 4 [1]
	 Firmware   : 1.57
	 Short name : 192.168.2.121
	 Long name  : Sparkfun [1] with TLC59711 [4]
	 Output
	  Port 0  1    HTP  Art-Net
	Bridge
	 Firmware : 1.26
	 CID      : 74e2d98b-f247-49e9-87f7-215d9c6fb82b
	SparkFun AutoDriver [0]
	 Position=0, ChipSelect=0, ResetPin=27, BusyPin=255 [SPI]
	 MinSpeed=  0, MaxSpeed=809, Acc=1004, Dec=1004
	 AccKVAL=50, DecKVAL=50, RunKVAL=50, HoldKVAL=50
	 MicroSteps=4, SwitchMode=0
	 DMX: Mode=4, StartAddress=1, FootPrint=1
	 SlotInfo: 00:0000 
	PWM parameters
	 Type  : TLC59711 [0]
	 Count : 4 RGB
	 Clock : 0 Hz Default {Default: 5000000 Hz, Maximum 10000000 Hz}
	 DMX   : StartAddress=1, FootPrint=12
	Starting Art-Net
	timer
	Art-Net started
