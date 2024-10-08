/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2017-2024 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <signal.h>

#include "hardware.h"
#include "network.h"
#include "networkconst.h"

#include "net/apps/mdns.h"

#include "displayudf.h"
#include "displayudfparams.h"

#include "artnetnode.h"
#include "artnetparams.h"
#include "artnetmsgconst.h"

#include "rdmdeviceresponder.h"
#include "factorydefaults.h"
#include "rdmpersonality.h"
#include "rdmdeviceparams.h"
#include "rdmsensorsparams.h"
#if defined (CONFIG_RDM_ENABLE_SUBDEVICES)
# include "rdmsubdevicesparams.h"
#endif

#include "artnetrdmresponder.h"

#include "tlc59711dmxparams.h"
#include "tlc59711dmx.h"

#if defined (NODE_SHOWFILE)
# include "showfile.h"
# include "showfileparams.h"
#endif

#include "lightsetchain.h"

#include "remoteconfig.h"
#include "remoteconfigparams.h"

#include "configstore.h"

#include "sparkfundmx.h"
#include "sparkfundmxconst.h"

#include "firmwareversion.h"
#include "software_version.h"

#include "statemachine.h"

static bool keepRunning = true;

void intHandler(int) {
    keepRunning = false;
}

namespace artnetnode {
namespace configstore {
uint32_t DMXPORT_OFFSET = 0;
}  // namespace configstore
}  // namespace artnetnode

int main(int argc, char **argv) {
    struct sigaction act;
    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, nullptr);
	Hardware hw;
	DisplayUdf display;
	ConfigStore configStore;
	Network nw(argc, argv);
	MDNS mDns;
	FirmwareVersion fw(SOFTWARE_VERSION, __DATE__, __TIME__);

	hw.Print();
	fw.Print("Art-Net 4 Stepper L6470");
	nw.Print();

	LightSet *pBoard;
	uint32_t nMotorsConnected = 0;

	display.TextStatus(SparkFunDmxConst::MSG_INIT, CONSOLE_YELLOW);

	SparkFunDmx sparkFunDmx;
	sparkFunDmx.ReadConfigFiles();

	nMotorsConnected = sparkFunDmx.GetMotorsConnected();

	pBoard = &sparkFunDmx;
	bool isLedTypeSet = false;

	TLC59711DmxParams pwmledparms;
	pwmledparms.Load();

	if ((isLedTypeSet = pwmledparms.IsSetLedType()) == true) {
		auto *pTLC59711Dmx = new TLC59711Dmx;
		assert(pTLC59711Dmx != nullptr);
		pwmledparms.Set(pTLC59711Dmx);

		display.Printf(7, "%s:%d", pwmledparms.GetType(pwmledparms.GetLedType()), pwmledparms.GetLedCount());

		auto *pChain = new LightSetChain;
		assert(pChain != nullptr);

		pChain->Add(pBoard, 0);
		pChain->Add(pTLC59711Dmx, 1);
		pChain->Dump();

		pBoard = pChain;
	}

	char aDescription[64];
	if (isLedTypeSet) {
		snprintf(aDescription, sizeof(aDescription) - 1, "Sparkfun [%d] with %s [%d]", nMotorsConnected, pwmledparms.GetType(pwmledparms.GetLedType()), pwmledparms.GetLedCount());
	} else {
		snprintf(aDescription, sizeof(aDescription) - 1, "Sparkfun [%d]", nMotorsConnected);
	}

	ArtNetNode node;

	node.SetLongName(aDescription);

	ArtNetParams artnetParams;
	artnetParams.Load();
	artnetParams.Set();

	node.SetRdm(static_cast<uint32_t>(0), true);
	node.SetOutput(pBoard);
	node.SetUniverse(0, lightset::PortDir::OUTPUT, artnetParams.GetUniverse(0));

	RDMPersonality *pRDMPersonalities[1] = { new  RDMPersonality(aDescription, pBoard)};

	ArtNetRdmResponder rdmResponder(pRDMPersonalities, 1);

	RDMSensorsParams rdmSensorsParams;
	rdmSensorsParams.Load();
	rdmSensorsParams.Set();

#if defined (CONFIG_RDM_ENABLE_SUBDEVICES)
	RDMSubDevicesParams rdmSubDevicesParams;
	rdmSubDevicesParams.Load();
	rdmSubDevicesParams.Set();
#endif

	rdmResponder.Init();

	RDMDeviceParams rdmDeviceParams;
	rdmDeviceParams.Load();
	rdmDeviceParams.Set(&rdmResponder);

	rdmResponder.Print();

	node.SetRdmResponder(&rdmResponder);
	node.Print();

	pBoard->Print();

#if defined (NODE_SHOWFILE)
	ShowFile showFile;

	ShowFileParams showFileParams;
	showFileParams.Load();
	showFileParams.Set();

	if (showFile.IsAutoStart()) {
		showFile.Play();
	}

	showFile.Print();
#endif

	display.SetTitle("Art-Net 4 L6470");
	display.Set(2, displayudf::Labels::IP);
	display.Set(3, displayudf::Labels::VERSION);
	display.Set(4, displayudf::Labels::UNIVERSE_PORT_A);
	display.Set(5, displayudf::Labels::DMX_START_ADDRESS);

	DisplayUdfParams displayUdfParams;
	displayUdfParams.Load();
	displayUdfParams.Set(&display);

	display.Show();

	RemoteConfig remoteConfig(remoteconfig::Node::ARTNET, remoteconfig::Output::STEPPER, node.GetActiveOutputPorts());

	RemoteConfigParams remoteConfigParams;
	remoteConfigParams.Load();
	remoteConfigParams.Set(&remoteConfig);

	while (configStore.Flash())
		;

	mDns.Print();

	StateMachine stateMachine;

	display.TextStatus(ArtNetMsgConst::START, CONSOLE_YELLOW);

	node.Start();

	display.TextStatus(ArtNetMsgConst::STARTED, CONSOLE_GREEN);

	while (keepRunning) {
		node.Run();
		remoteConfig.Run();
		configStore.Flash();
		display.Run();
		stateMachine.Run();
	}

	return 0;
}
