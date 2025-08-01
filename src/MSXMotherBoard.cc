#include "MSXMotherBoard.hh"

#include "BooleanSetting.hh"
#include "CartridgeSlotManager.hh"
#include "CassettePort.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "ConfigException.hh"
#include "Debugger.hh"
#include "DeviceFactory.hh"
#include "EventDelay.hh"
#include "EventDistributor.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "GlobalCliComm.hh"
#include "GlobalSettings.hh"
#include "HardwareConfig.hh"
#include "InfoTopic.hh"
#include "JoystickPort.hh"
#include "LedStatus.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXCliComm.hh"
#include "MSXCommandController.hh"
#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXEventDistributor.hh"
#include "MSXMapperIO.hh"
#include "MSXMixer.hh"
#include "Observer.hh"
#include "PanasonicMemory.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
#include "Reactor.hh"
#include "RealTime.hh"
#include "RenShaTurbo.hh"
#include "ReverseManager.hh"
#include "Schedulable.hh"
#include "Scheduler.hh"
#include "SimpleDebuggable.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "serialize_stl.hh"

#include "ScopedAssign.hh"
#include "one_of.hh"
#include "stl.hh"
#include "unreachable.hh"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>

namespace openmsx {

class AddRemoveUpdate
{
public:
	explicit AddRemoveUpdate(MSXMotherBoard& motherBoard);
	AddRemoveUpdate(const AddRemoveUpdate&) = delete;
	AddRemoveUpdate(AddRemoveUpdate&&) = delete;
	AddRemoveUpdate& operator=(const AddRemoveUpdate&) = delete;
	AddRemoveUpdate& operator=(AddRemoveUpdate&&) = delete;
	~AddRemoveUpdate();
private:
	MSXMotherBoard& motherBoard;
};

class ResetCmd final : public RecordedCommand
{
public:
	explicit ResetCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result,
	             EmuTime time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class LoadMachineCmd final : public Command
{
public:
	explicit LoadMachineCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class ListExtCmd final : public Command
{
public:
	explicit ListExtCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class RemoveExtCmd final : public RecordedCommand
{
public:
	explicit RemoveExtCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result,
	             EmuTime time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class StoreSetupCmd final : public Command
{
public:
	explicit StoreSetupCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineNameInfo final : public InfoTopic
{
public:
	explicit MachineNameInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineTypeInfo final : public InfoTopic
{
public:
	explicit MachineTypeInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineExtensionInfo final : public InfoTopic
{
public:
	explicit MachineExtensionInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineMediaInfo final : public InfoTopic
{
public:
	explicit MachineMediaInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class DeviceInfo final : public InfoTopic
{
public:
	explicit DeviceInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class FastForwardHelper final : private Schedulable
{
public:
	explicit FastForwardHelper(MSXMotherBoard& motherBoard);
	void setTarget(EmuTime targetTime);
private:
	void executeUntil(EmuTime time) override;
	MSXMotherBoard& motherBoard;
};

class JoyPortDebuggable final : public SimpleDebuggable
{
public:
	explicit JoyPortDebuggable(MSXMotherBoard& motherBoard);
	[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
	void write(unsigned address, uint8_t value) override;
};

class SettingObserver final : public Observer<Setting>
{
public:
	explicit SettingObserver(MSXMotherBoard& motherBoard);
	void update(const Setting& setting) noexcept override;
private:
	MSXMotherBoard& motherBoard;
};


static unsigned machineIDCounter = 0;

MSXMotherBoard::MSXMotherBoard(Reactor& reactor_)
	: reactor(reactor_)
	, machineID(strCat("machine", ++machineIDCounter))
	, msxCliComm(std::make_unique<MSXCliComm>(*this, reactor.getGlobalCliComm()))
	, msxEventDistributor(std::make_unique<MSXEventDistributor>())
	, stateChangeDistributor(std::make_unique<StateChangeDistributor>())
	, msxCommandController(std::make_unique<MSXCommandController>(
		reactor.getGlobalCommandController(), reactor,
		*this, *msxEventDistributor, machineID))
	, scheduler(std::make_unique<Scheduler>())
	, msxMixer(std::make_unique<MSXMixer>(
		reactor.getMixer(), *this,
		reactor.getGlobalSettings()))
	, videoSourceSetting(*msxCommandController)
	, suppressMessagesSetting(*msxCommandController, "suppressmessages",
		"Suppress info, warning and error messages for this machine. "
		"Intended use is for scripts that create temporary machines "
		"of which you don't want to see warning messages about blank "
		"SRAM content or PSG port directions for instance.",
		false, Setting::Save::NO)
	, fastForwardHelper(std::make_unique<FastForwardHelper>(*this))
	, settingObserver(std::make_unique<SettingObserver>(*this))
	, powerSetting(reactor.getGlobalSettings().getPowerSetting())
{
	slotManager = std::make_unique<CartridgeSlotManager>(*this);
	reverseManager = std::make_unique<ReverseManager>(*this);
	resetCommand = std::make_unique<ResetCmd>(*this);
	loadMachineCommand = std::make_unique<LoadMachineCmd>(*this);
	listExtCommand = std::make_unique<ListExtCmd>(*this);
	extCommand = std::make_unique<ExtCmd>(*this, "ext");
	removeExtCommand = std::make_unique<RemoveExtCmd>(*this);
	storeSetupCommand = std::make_unique<StoreSetupCmd>(*this);
	machineNameInfo = std::make_unique<MachineNameInfo>(*this);
	machineTypeInfo = std::make_unique<MachineTypeInfo>(*this);
	machineExtensionInfo = std::make_unique<MachineExtensionInfo>(*this);
	machineMediaInfo = std::make_unique<MachineMediaInfo>(*this);
	deviceInfo = std::make_unique<DeviceInfo>(*this);
	debugger = std::make_unique<Debugger>(*this);

	// Do this before machine-specific settings are created, otherwise
	// a setting-info CliComm message is send with a machine id that hasn't
	// been announced yet over CliComm.
	addRemoveUpdate = std::make_unique<AddRemoveUpdate>(*this);

	// TODO: Initialization of this field cannot be done much earlier because
	//       EventDelay creates a setting, calling getMSXCliComm()
	//       on MSXMotherBoard, so "pimpl" has to be set up already.
	eventDelay = std::make_unique<EventDelay>(
		*scheduler, *msxCommandController,
		reactor.getEventDistributor(), *msxEventDistributor,
		*reverseManager);
	realTime = std::make_unique<RealTime>(
		*this, reactor.getGlobalSettings(), *eventDelay);

	powerSetting.attach(*settingObserver);
	suppressMessagesSetting.attach(*settingObserver);
}

MSXMotherBoard::~MSXMotherBoard()
{
	suppressMessagesSetting.detach(*settingObserver);
	powerSetting.detach(*settingObserver);
	deleteMachine();

	assert(mapperIOCounter == 0);
	assert(availableDevices.empty());
	assert(extensions.empty());
	assert(!machineConfig2);
	assert(!getMachineConfig());
}

void MSXMotherBoard::deleteMachine()
{
	while (!extensions.empty()) {
		try {
			removeExtension(*extensions.back());
		} catch (MSXException& e) {
			std::cerr << "Internal error: failed to remove "
			             "extension while deleting a machine: "
			          << e.getMessage() << '\n';
			assert(false);
		}
	}

	machineConfig2.reset();
	machineConfig = nullptr;
}

void MSXMotherBoard::setMachineConfig(HardwareConfig* machineConfig_)
{
	assert(!getMachineConfig());
	machineConfig = machineConfig_;

	// make sure the CPU gets instantiated from the main thread
	assert(!msxCpu);
	msxCpu = std::make_unique<MSXCPU>(*this);
	msxCpuInterface = std::make_unique<MSXCPUInterface>(*this);
}

std::string_view MSXMotherBoard::getMachineType() const
{
	if (const HardwareConfig* machine = getMachineConfig()) {
		if (const auto* info = machine->getConfig().findChild("info")) {
			if (const auto* type = info->findChild("type")) {
				return type->getData();
			}
		}
	}
	return "";
}

bool MSXMotherBoard::isTurboR() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	return config->getConfig().getChild("devices").findChild("S1990") != nullptr;
}

bool MSXMotherBoard::hasToshibaEngine() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	const XMLElement& devices = config->getConfig().getChild("devices");
	return devices.findChild("T7775") != nullptr ||
	       devices.findChild("T7937") != nullptr ||
		   devices.findChild("T9763") != nullptr ||
		   devices.findChild("T9769") != nullptr;
}

std::string MSXMotherBoard::loadMachine(const std::string& machine)
{
	assert(machineName.empty());
	assert(extensions.empty());
	assert(!machineConfig2);
	assert(!getMachineConfig());

	try {
		machineConfig2 = HardwareConfig::createMachineConfig(*this, machine);
		setMachineConfig(machineConfig2.get());
	} catch (FileException& e) {
		throw MSXException("Machine \"", machine, "\" not found: ",
		                   e.getMessage());
	} catch (MSXException& e) {
		throw MSXException("Error in \"", machine, "\" machine: ",
		                   e.getMessage());
	}
	try {
		machineConfig->parseSlots();
		machineConfig->createDevices();
	} catch (MSXException& e) {
		throw MSXException("Error in \"", machine, "\" machine: ",
		                   e.getMessage());
	}
	if (powerSetting.getBoolean()) {
		powerUp();
	}
	machineName = machine;
	return machineName;
}

void MSXMotherBoard::storeAsSetup(const std::string& filename, SetupDepth depth)
{
	// level 0: don't do anything. Added as convenience.
	if (depth == SetupDepth::NONE) return;

	XmlOutputArchive out(filename);

	if (depth == SetupDepth::COMPLETE_STATE) {
		// level 5: just save state to given file
		out.serialize("machine", this);
		out.close();
		return;
	}

	// level 1: create new board based on current board of this machine
	auto newBoard = reactor.createEmptyMotherBoard();
	newBoard->loadMachine(machineName);
	auto newTime = newBoard->getCurrentTime();

	if (depth >= SetupDepth::EXTENSIONS) {
		// level 2: add the extensions of the current board to the new board

		// suppress any messages from this temporary board
		newBoard->getMSXCliComm().setSuppressMessages(true);

		for (auto& extension: extensions) {
			if (extension->getType() == HardwareConfig::Type::EXTENSION) {
				auto& configName = extension->getConfigName();
				auto slot = slotManager->findSlotWith(*extension);
				const std::string slotSpec = slot ? std::string(1, char('a' + *slot)) : "any";
				// TODO: a bit weird that we need to convert the slot
				// spec into a string and then parse it again deep down
				// in loadExtension...
				auto extConfig = newBoard->loadExtension(configName, slotSpec);
				newBoard->insertExtension(configName, std::move(extConfig));
			}
		}
	}

	if (depth >= SetupDepth::CONNECTORS) {
		// level 3: add the connectors/pluggables of the current board to the new board
		auto& newBoardPluggingController = newBoard->getPluggingController();
		for (const auto* connector: getPluggingController().getConnectors()) {
			const auto& plugged = connector->getPlugged();
			const auto& pluggedName = plugged.getName();
			if (!pluggedName.empty()) {
				const auto& connectorName = connector->getName();
				if (auto* newBoardConnector = newBoardPluggingController.findConnector(connectorName)) {
					if (auto* newBoardPluggable = newBoardPluggingController.findPluggable(pluggedName)) {
						newBoardConnector->plug(*newBoardPluggable, newTime);
					}
				}
			}
		}
	}

	if (depth >= SetupDepth::MEDIA) {
		// level 4: add the inserted media of the current board to the new board
		for (const auto& oldMedia : getMediaProviders()) {
			if (auto* newMediaProvider = newBoard->findMediaProvider(oldMedia.name)) {
				TclObject info;
				oldMedia.provider->getMediaInfo(info);
				newMediaProvider->setMedia(info, newTime);
			}
		}
	}

	out.serialize("machine", newBoard);
	out.close();
}

std::unique_ptr<HardwareConfig> MSXMotherBoard::loadExtension(std::string_view name, std::string_view slotName)
{
	try {
		return HardwareConfig::createExtensionConfig(
			*this, std::string(name), slotName);
	} catch (FileException& e) {
		throw MSXException(
			"Extension \"", name, "\" not found: ", e.getMessage());
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"", name, "\" extension: ", e.getMessage());
	}
}

std::string MSXMotherBoard::insertExtension(
	std::string_view name, std::unique_ptr<HardwareConfig> extension)
{
	try {
		extension->parseSlots();
		extension->createDevices();
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"", name, "\" extension: ", e.getMessage());
	}
	std::string result = extension->getName();
	extensions.push_back(std::move(extension));
	getMSXCliComm().update(CliComm::UpdateType::EXTENSION, result, "add");
	return result;
}

HardwareConfig* MSXMotherBoard::findExtension(std::string_view extensionName)
{
	auto it = std::ranges::find(extensions, extensionName, &HardwareConfig::getName);
	return (it != end(extensions)) ? it->get() : nullptr;
}

void MSXMotherBoard::removeExtension(const HardwareConfig& extension)
{
	extension.testRemove();
	getMSXCliComm().update(CliComm::UpdateType::EXTENSION, extension.getName(), "remove");
	auto it = rfind_unguarded(extensions, &extension,
	                          [](auto& e) { return e.get(); });
	extensions.erase(it);
}

MSXCliComm& MSXMotherBoard::getMSXCliComm()
{
	return *msxCliComm;
}

PluggingController& MSXMotherBoard::getPluggingController()
{
	assert(getMachineConfig()); // needed for PluggableFactory::createAll()
	if (!pluggingController) {
		pluggingController = std::make_unique<PluggingController>(*this);
	}
	return *pluggingController;
}

MSXCPU& MSXMotherBoard::getCPU()
{
	assert(getMachineConfig()); // because CPU needs to know if we're
	                            // emulating turbor or not
	return *msxCpu;
}

MSXCPUInterface& MSXMotherBoard::getCPUInterface()
{
	assert(getMachineConfig());
	return *msxCpuInterface;
}

PanasonicMemory& MSXMotherBoard::getPanasonicMemory()
{
	if (!panasonicMemory) {
		panasonicMemory = std::make_unique<PanasonicMemory>(*this);
	}
	return *panasonicMemory;
}

MSXDeviceSwitch& MSXMotherBoard::getDeviceSwitch()
{
	if (!deviceSwitch) {
		deviceSwitch = DeviceFactory::createDeviceSwitch(*getMachineConfig());
	}
	return *deviceSwitch;
}

CassettePortInterface& MSXMotherBoard::getCassettePort()
{
	if (!cassettePort) {
		assert(getMachineConfig());
		if (getMachineConfig()->getConfig().findChild("CassettePort")) {
			cassettePort = std::make_unique<CassettePort>(*getMachineConfig());
		} else {
			cassettePort = std::make_unique<DummyCassettePort>(*this);
		}
	}
	return *cassettePort;
}

JoystickPortIf& MSXMotherBoard::getJoystickPort(unsigned port)
{
	assert(port < 2);
	if (!joystickPort[0]) {
		assert(getMachineConfig());
		// some MSX machines only have 1 instead of 2 joystick ports
		std::string_view ports = getMachineConfig()->getConfig().getChildData(
			"JoystickPorts", "AB");
		if (ports != one_of("AB", "", "A", "B")) {
			throw ConfigException(
				"Invalid JoystickPorts specification, "
				"should be one of '', 'A', 'B' or 'AB'.");
		}
		PluggingController& ctrl = getPluggingController();
		if (ports == one_of("AB", "A")) {
			joystickPort[0] = std::make_unique<JoystickPort>(
				ctrl, "joyporta", "MSX Joystick port A");
		} else {
			joystickPort[0] = std::make_unique<DummyJoystickPort>();
		}
		if (ports == one_of("AB", "B")) {
			joystickPort[1] = std::make_unique<JoystickPort>(
				ctrl, "joyportb", "MSX Joystick port B");
		} else {
			joystickPort[1] = std::make_unique<DummyJoystickPort>();
		}
		joyPortDebuggable = std::make_unique<JoyPortDebuggable>(*this);
	}
	return *joystickPort[port];
}

RenShaTurbo& MSXMotherBoard::getRenShaTurbo()
{
	if (!renShaTurbo) {
		assert(getMachineConfig());
		renShaTurbo = std::make_unique<RenShaTurbo>(
			*this,
			getMachineConfig()->getConfig());
	}
	return *renShaTurbo;
}

LedStatus& MSXMotherBoard::getLedStatus()
{
	if (!ledStatus) {
		(void)getMSXCliComm(); // force init, to be on the safe side
		ledStatus = std::make_unique<LedStatus>(
			reactor.getRTScheduler(),
			*msxCommandController,
			*msxCliComm);
	}
	return *ledStatus;
}

CommandController& MSXMotherBoard::getCommandController()
{
	return *msxCommandController;
}

InfoCommand& MSXMotherBoard::getMachineInfoCommand()
{
	return msxCommandController->getMachineInfoCommand();
}

EmuTime MSXMotherBoard::getCurrentTime() const
{
	return scheduler->getCurrentTime();
}

bool MSXMotherBoard::execute()
{
	if (!powered) {
		return false;
	}
	assert(getMachineConfig()); // otherwise powered cannot be true

	getCPU().execute(false);
	return true;
}

void MSXMotherBoard::fastForward(EmuTime time, bool fast)
{
	assert(powered);
	assert(getMachineConfig());

	if (time <= getCurrentTime()) return;

	ScopedAssign sa(fastForwarding, fast);
	realTime->disable();
	msxMixer->mute();
	fastForwardHelper->setTarget(time);
	while (time > getCurrentTime()) {
		// note: this can run (slightly) past the requested time
		getCPU().execute(true); // fast-forward mode
	}
	realTime->enable();
	msxMixer->unmute();
}

void MSXMotherBoard::pause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(true);
	}
	msxMixer->mute();
}

void MSXMotherBoard::unpause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(false);
	}
	msxMixer->unmute();
}

void MSXMotherBoard::addDevice(MSXDevice& device)
{
	availableDevices.push_back(&device);
}

void MSXMotherBoard::removeDevice(MSXDevice& device)
{
	move_pop_back(availableDevices, rfind_unguarded(availableDevices, &device));
}

void MSXMotherBoard::doReset()
{
	if (!powered) return;
	assert(getMachineConfig());

	EmuTime time = getCurrentTime();
	getCPUInterface().reset();
	for (auto& d : availableDevices) {
		d->reset(time);
	}
	getCPU().doReset(time);
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	reactor.getEventDistributor().distributeEvent(BootEvent());
}

uint8_t MSXMotherBoard::readIRQVector() const
{
	uint8_t result = 0xff;
	for (auto& d : availableDevices) {
		result &= d->readIRQVector();
	}
	return result;
}

void MSXMotherBoard::powerUp()
{
	if (powered) return;
	if (!getMachineConfig()) return;

	powered = true;
	// TODO: If our "powered" field is always equal to the power setting,
	//       there is not really a point in keeping it.
	// TODO: assert disabled see note in Reactor::run() where this method
	//       is called
	//assert(powerSetting.getBoolean() == powered);
	powerSetting.setBoolean(true);
	// TODO: We could make the power LED a device, so we don't have to handle
	//       it separately here.
	getLedStatus().setLed(LedStatus::POWER, true);

	EmuTime time = getCurrentTime();
	getCPUInterface().reset();
	for (auto& d : availableDevices) {
		d->powerUp(time);
	}
	getCPU().doReset(time);
	msxMixer->unmute();
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	reactor.getEventDistributor().distributeEvent(BootEvent());
}

void MSXMotherBoard::powerDown()
{
	if (!powered) return;

	powered = false;
	// TODO: This assertion fails in 1 case: when quitting with a running MSX.
	//       How do we want the Reactor to shutdown: immediately or after
	//       handling all pending commands/events/updates?
	//assert(powerSetting.getBoolean() == powered);
	powerSetting.setBoolean(false);
	getLedStatus().setLed(LedStatus::POWER, false);

	msxMixer->mute();

	EmuTime time = getCurrentTime();
	for (auto& d : availableDevices) {
		d->powerDown(time);
	}
}

void MSXMotherBoard::activate(bool active_)
{
	active = active_;
	auto event = active ? Event(MachineActivatedEvent())
	                    : Event(MachineDeactivatedEvent());
	msxEventDistributor->distributeEvent(event, scheduler->getCurrentTime());
	if (active) {
		realTime->resync();
	}
}

void MSXMotherBoard::exitCPULoopAsync()
{
	if (getMachineConfig()) {
		getCPU().exitCPULoopAsync();
	}
}

void MSXMotherBoard::exitCPULoopSync()
{
	getCPU().exitCPULoopSync();
}

MSXDevice* MSXMotherBoard::findDevice(std::string_view name)
{
	auto it = std::ranges::find(availableDevices, name, &MSXDevice::getName);
	return (it != end(availableDevices)) ? *it : nullptr;
}

MSXMapperIO& MSXMotherBoard::createMapperIO()
{
	if (mapperIOCounter == 0) {
		mapperIO = DeviceFactory::createMapperIO(*getMachineConfig());
		getCPUInterface().register_IO_InOut_range(0xfc, 4, mapperIO.get());
	}
	++mapperIOCounter;
	return *mapperIO;
}

void MSXMotherBoard::destroyMapperIO()
{
	assert(mapperIO);
	assert(mapperIOCounter);
	--mapperIOCounter;
	if (mapperIOCounter == 0) {
		getCPUInterface().unregister_IO_InOut_range(0xfc, 4, mapperIO.get());
		mapperIO.reset();
	}
}

std::string MSXMotherBoard::getUserName(const std::string& hwName)
{
	auto& s = userNames[hwName];
	unsigned n = 0;
	std::string userName;
	do {
		userName = strCat("untitled", ++n);
	} while (contains(s, userName));
	s.push_back(userName);
	return userName;
}

void MSXMotherBoard::freeUserName(const std::string& hwName, const std::string& userName)
{
	auto& s = userNames[hwName];
	move_pop_back(s, rfind_unguarded(s, userName));
}

void MSXMotherBoard::registerMediaProvider(std::string_view name, MediaProvider& provider)
{
	assert(!contains(mediaProviders, name, &MediaProviderInfo::name));
	assert(!contains(mediaProviders, &provider, &MediaProviderInfo::provider));
	mediaProviders.emplace_back(name, &provider);
}

void MSXMotherBoard::unregisterMediaProvider(MediaProvider& provider)
{
	move_pop_back(mediaProviders,
	              rfind_unguarded(mediaProviders, &provider, &MediaProviderInfo::provider));
}

MediaProvider* MSXMotherBoard::findMediaProvider(std::string_view name) const
{
	auto it = std::ranges::find(mediaProviders, name, &MediaProviderInfo::name);
	return (it != mediaProviders.end()) ? it->provider : nullptr;
}


// AddRemoveUpdate

AddRemoveUpdate::AddRemoveUpdate(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	motherBoard.getReactor().getGlobalCliComm().update(
		CliComm::UpdateType::HARDWARE, motherBoard.getMachineID(), "add");
}

AddRemoveUpdate::~AddRemoveUpdate()
{
	motherBoard.getReactor().getGlobalCliComm().update(
		CliComm::UpdateType::HARDWARE, motherBoard.getMachineID(), "remove");
}


// ResetCmd
ResetCmd::ResetCmd(MSXMotherBoard& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  "reset")
	, motherBoard(motherBoard_)
{
}

void ResetCmd::execute(std::span<const TclObject> /*tokens*/, TclObject& /*result*/,
                       EmuTime /*time*/)
{
	motherBoard.doReset();
}

std::string ResetCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Resets the MSX.";
}


// LoadMachineCmd
LoadMachineCmd::LoadMachineCmd(MSXMotherBoard& motherBoard_)
	: Command(motherBoard_.getCommandController(), "load_machine")
	, motherBoard(motherBoard_)
{
	// The load_machine command should always directly follow a
	// create_machine command:
	// - It's not allowed to use load_machine on a machine that has
	//   already a machine configuration loaded earlier.
	// - We also disallow executing most machine-specific commands on an
	//   'empty machine' (an 'empty machine', is a machine returned by
	//   create_machine before the load_machine command is executed, so a
	//   machine without a machine configuration). The only exception is
	//   this load_machine command and machine_info.
	//
	// So if the only allowed command on an empty machine is
	// 'load_machine', (and an empty machine by itself isn't very useful),
	// then why isn't create_machine and load_machine merged into a single
	// command? The only reason for this is that load_machine sends out
	// events (machine specific) and maybe you already want to know the ID
	// of the new machine (this is returned by create_machine) before those
	// events will be send.
	//
	// Why not allow all commands on an empty machine? In the past we did
	// allow this, though it often was the source of bugs. We could in each
	// command (when needed) check for an empty machine and then return
	// some dummy/empty result or some error. But because I can't think of
	// any really useful command for an empty machine, it seemed easier to
	// just disallow most commands.
	setAllowedInEmptyMachine(true);
}

void LoadMachineCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 2, "machine");
	if (motherBoard.getMachineConfig()) {
		throw CommandException("Already loaded a config in this machine.");
	}
	result = motherBoard.loadMachine(std::string(tokens[1].getString()));
}

std::string LoadMachineCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Load a msx machine configuration into an empty machine.";
}

void LoadMachineCmd::tabCompletion(std::vector<std::string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs("machines"));
}


// ListExtCmd
ListExtCmd::ListExtCmd(MSXMotherBoard& motherBoard_)
	: Command(motherBoard_.getCommandController(), "list_extensions")
	, motherBoard(motherBoard_)
{
}

void ListExtCmd::execute(std::span<const TclObject> /*tokens*/, TclObject& result)
{
	result.addListElements(
		std::views::transform(motherBoard.getExtensions(), &HardwareConfig::getName));
}

std::string ListExtCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Return a list of all inserted extensions.";
}


// ExtCmd
ExtCmd::ExtCmd(MSXMotherBoard& motherBoard_, std::string commandName_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  commandName_)
	, motherBoard(motherBoard_)
	, commandName(std::move(commandName_))
{
}

void ExtCmd::execute(std::span<const TclObject> tokens, TclObject& result,
                     EmuTime /*time*/)
{
	checkNumArgs(tokens, Between{2, 3}, "extension");
	if (tokens.size() == 3 && tokens[1].getString() != "insert") {
		throw SyntaxError();
	}
	try {
		auto name = tokens[tokens.size() - 1].getString();
		auto slotName = (commandName.size() == 4)
			? std::string_view(&commandName[3], 1)
			: "any";
		auto extension = motherBoard.loadExtension(name, slotName);
		if (slotName != "any") {
			const auto& manager = motherBoard.getSlotManager();
			if (const auto* extConf = manager.getConfigForSlot(commandName[3] - 'a')) {
				// still a cartridge inserted, (try to) remove it now
				motherBoard.removeExtension(*extConf);
			}
		}
		result = motherBoard.insertExtension(name, std::move(extension));
	} catch (MSXException& e) {
		throw CommandException(std::move(e).getMessage());
	}
}

std::string ExtCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Insert a hardware extension.";
}

void ExtCmd::tabCompletion(std::vector<std::string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs("extensions"));
}


// RemoveExtCmd
RemoveExtCmd::RemoveExtCmd(MSXMotherBoard& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  "remove_extension")
	, motherBoard(motherBoard_)
{
}

void RemoveExtCmd::execute(std::span<const TclObject> tokens, TclObject& /*result*/,
                           EmuTime /*time*/)
{
	checkNumArgs(tokens, 2, "extension");
	std::string_view extName = tokens[1].getString();
	HardwareConfig* extension = motherBoard.findExtension(extName);
	if (!extension) {
		throw CommandException("No such extension: ", extName);
	}
	try {
		motherBoard.removeExtension(*extension);
	} catch (MSXException& e) {
		throw CommandException("Can't remove extension '", extName,
		                       "': ", e.getMessage());
	}
}

std::string RemoveExtCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Remove an extension from the MSX machine.";
}

void RemoveExtCmd::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 2) {
		completeString(tokens, std::views::transform(
			motherBoard.getExtensions(),
			[](auto& e) -> std::string_view { return e->getName(); }));
	}
}

// StoreSetupCmd

static const std::map<std::string_view, SetupDepth, std::less<>> depthOptionMap = {
	{"none"          , SetupDepth::NONE          },
	{"machine"       , SetupDepth::MACHINE       },
	{"extensions"    , SetupDepth::EXTENSIONS    },
	{"connectors"    , SetupDepth::CONNECTORS    },
	{"media"         , SetupDepth::MEDIA         },
	{"complete_state", SetupDepth::COMPLETE_STATE},
};

StoreSetupCmd::StoreSetupCmd(MSXMotherBoard& motherBoard_)
	: Command(motherBoard_.getCommandController(), "store_setup")
	, motherBoard(motherBoard_)
{
}

void StoreSetupCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{1}, "depth ?filename?");

	auto depthArg = tokens[1].getString();
	const auto* depth = lookup(depthOptionMap, depthArg);
	if (!depth) {
		throw CommandException("Unknown depth argument: ", depthArg);
	}

	if (*depth == SetupDepth::NONE) return;

	std::string_view filenameArg;
	if (tokens.size() == 3) {
		filenameArg = tokens[2].getString();
	}

	auto filename = FileOperations::parseCommandFileArgument(
		filenameArg, Reactor::SETUP_DIR, motherBoard.getMachineName(), Reactor::SETUP_EXTENSION);

	// TODO: make parts of levels to be saved configurable?
	motherBoard.storeAsSetup(filename, *depth);

	result = filename;
}

std::string StoreSetupCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return
		"store_setup <depth> [filename]  Save setup based on this machine with given depth to indicated file.";
}

void StoreSetupCmd::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 2) {
		completeString(tokens, std::views::keys(depthOptionMap));
	} else if (tokens.size() == 3) {
		completeString(tokens, Reactor::getSetups());
	}
}


// MachineNameInfo

MachineNameInfo::MachineNameInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "config_name")
	, motherBoard(motherBoard_)
{
}

void MachineNameInfo::execute(std::span<const TclObject> /*tokens*/,
                              TclObject& result) const
{
	result = motherBoard.getMachineName();
}

std::string MachineNameInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the configuration name for this machine.";
}

// MachineTypeInfo

MachineTypeInfo::MachineTypeInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "type")
	, motherBoard(motherBoard_)
{
}

void MachineTypeInfo::execute(std::span<const TclObject> /*tokens*/,
                              TclObject& result) const
{
	result = motherBoard.getMachineType();
}

std::string MachineTypeInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the machine type for this machine.";
}


// MachineExtensionInfo

MachineExtensionInfo::MachineExtensionInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "extension")
	, motherBoard(motherBoard_)
{
}

void MachineExtensionInfo::execute(std::span<const TclObject> tokens,
                              TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?extension-instance-name?");
	if (tokens.size() == 2) {
		result.addListElements(
			std::views::transform(motherBoard.getExtensions(), &HardwareConfig::getName));
	} else if (tokens.size() == 3) {
		std::string_view extName = tokens[2].getString();
		const HardwareConfig* extension = motherBoard.findExtension(extName);
		if (!extension) {
			throw CommandException("No such extension: ", extName);
		}
		if (extension->getType() == HardwareConfig::Type::EXTENSION) {
			// A 'true' extension, as specified in an XML file
			result.addDictKeyValue("config", extension->getConfigName());
		} else {
			assert(extension->getType() == HardwareConfig::Type::ROM);
			// A ROM cartridge, peek into the internal config for the original filename
			const auto& filename = extension->getConfig()
				.getChild("devices").getChild("primary").getChild("secondary")
				.getChild("ROM").getChild("rom").getChildData("filename");
			result.addDictKeyValue("rom", filename);
		}
		TclObject deviceList;
		deviceList.addListElements(
			std::views::transform(extension->getDevices(), &MSXDevice::getName));
		result.addDictKeyValue("devices", deviceList);
	}
}

std::string MachineExtensionInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns information about the given extension instance.";
}

void MachineExtensionInfo::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, std::views::transform(
			motherBoard.getExtensions(),
			[](auto& e) -> std::string_view { return e->getName(); }));
	}
}


// MachineMediaInfo

MachineMediaInfo::MachineMediaInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "media")
	, motherBoard(motherBoard_)
{
}

void MachineMediaInfo::execute(std::span<const TclObject> tokens,
                              TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?media-slot-name?");
	const auto& providers = motherBoard.getMediaProviders();
	if (tokens.size() == 2) {
		result.addListElements(
			std::views::transform(providers, &MediaProviderInfo::name));
	} else if (tokens.size() == 3) {
		auto name = tokens[2].getString();
		if (auto it = std::ranges::find(providers, name, &MediaProviderInfo::name);
		    it != providers.end()) {
			it->provider->getMediaInfo(result);
		} else {
			throw CommandException("No info about media slot ", name);
		}
	}
}

std::string MachineMediaInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns information about the given media slot.";
}

void MachineMediaInfo::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, std::views::transform(
			motherBoard.getMediaProviders(), &MediaProviderInfo::name));
	}
}

// DeviceInfo

DeviceInfo::DeviceInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "device")
	, motherBoard(motherBoard_)
{
}

void DeviceInfo::execute(std::span<const TclObject> tokens, TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?device?");
	switch (tokens.size()) {
	case 2:
		result.addListElements(
			std::views::transform(motherBoard.availableDevices,
			                [](auto& d) { return d->getName(); }));
		break;
	case 3: {
		std::string_view deviceName = tokens[2].getString();
		const MSXDevice* device = motherBoard.findDevice(deviceName);
		if (!device) {
			throw CommandException("No such device: ", deviceName);
		}
		device->getDeviceInfo(result);
		break;
	}
	}
}

std::string DeviceInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Without any arguments, returns the list of used device names.\n"
	       "With a device name as argument, returns the type (and for some "
	       "devices the subtype) of the given device.";
}

void DeviceInfo::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, std::views::transform(
			motherBoard.availableDevices,
			[](auto& d) -> std::string_view { return d->getName(); }));
	}
}


// FastForwardHelper

FastForwardHelper::FastForwardHelper(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
{
}

void FastForwardHelper::setTarget(EmuTime targetTime)
{
	setSyncPoint(targetTime);
}

void FastForwardHelper::executeUntil(EmuTime /*time*/)
{
	motherBoard.exitCPULoopSync();
}


// class JoyPortDebuggable

JoyPortDebuggable::JoyPortDebuggable(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "joystickports", "MSX Joystick Ports", 2)
{
}

uint8_t JoyPortDebuggable::read(unsigned address, EmuTime time)
{
	return getMotherBoard().getJoystickPort(address).read(time);
}

void JoyPortDebuggable::write(unsigned /*address*/, uint8_t /*value*/)
{
	// ignore
}


// class SettingObserver

SettingObserver::SettingObserver(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
}

void SettingObserver::update(const Setting& setting) noexcept
{
	if (&setting == &motherBoard.powerSetting) {
		if (motherBoard.powerSetting.getBoolean()) {
			motherBoard.powerUp();
		} else {
			motherBoard.powerDown();
		}
	} else if (&setting == &motherBoard.suppressMessagesSetting) {
		motherBoard.msxCliComm->setSuppressMessages(motherBoard.suppressMessagesSetting.getBoolean());
	} else {
		UNREACHABLE;
	}
}


// serialize
// version 1: initial version
// version 2: added reRecordCount
// version 3: removed reRecordCount (moved to ReverseManager)
// version 4: moved joystickportA/B from MSXPSG to here
// version 5: do serialize renShaTurbo
template<typename Archive>
void MSXMotherBoard::serialize(Archive& ar, unsigned version)
{
	// don't serialize:
	//    machineID, userNames, availableDevices, addRemoveUpdate,
	//    sharedStuffMap, msxCliComm, msxEventDistributor,
	//    msxCommandController, slotManager, eventDelay,
	//    debugger, msxMixer, panasonicMemory, ledStatus

	// Scheduler must come early so that devices can query current time
	ar.serialize("scheduler", *scheduler);
	// MSXMixer has already set sync points, those are invalid now
	// the following call will fix this
	if constexpr (Archive::IS_LOADER) {
		msxMixer->reInit();
	}

	ar.serialize("name", machineName);
	ar.serializeWithID("config", machineConfig2, std::ref(*this));
	assert(getMachineConfig() == machineConfig2.get());
	ar.serializeWithID("extensions", extensions, std::ref(*this));

	if (mapperIO) ar.serialize("mapperIO", *mapperIO);

	if (auto& devSwitch = getDeviceSwitch();
	    devSwitch.hasRegisteredDevices()) {
		ar.serialize("deviceSwitch", devSwitch);
	}

	if (getMachineConfig()) {
		ar.serialize("cpu", getCPU());
	}
	ar.serialize("cpuInterface", getCPUInterface());

	if (auto* port = dynamic_cast<CassettePort*>(&getCassettePort())) {
		ar.serialize("cassetteport", *port);
	}
	if (ar.versionAtLeast(version, 4)) {
		if (auto* port = dynamic_cast<JoystickPort*>(
				joystickPort[0].get())) {
			ar.serialize("joystickportA", *port);
		}
		if (auto* port = dynamic_cast<JoystickPort*>(
				joystickPort[1].get())) {
			ar.serialize("joystickportB", *port);
		}
	}
	if (ar.versionAtLeast(version, 5)) {
		if (renShaTurbo) ar.serialize("renShaTurbo", *renShaTurbo);
	}

	if constexpr (Archive::IS_LOADER) {
		powered = true; // must come before changing power setting
		powerSetting.setBoolean(true);
		getLedStatus().setLed(LedStatus::POWER, true);
		msxMixer->unmute();
	}

	if (version == 2) {
		assert(Archive::IS_LOADER);
		unsigned reRecordCount = 0; // silence warning
		ar.serialize("reRecordCount", reRecordCount);
		getReverseManager().setReRecordCount(reRecordCount);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMotherBoard)

} // namespace openmsx
