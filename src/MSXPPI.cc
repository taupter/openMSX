#include "MSXPPI.hh"
#include "LedStatus.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CassettePort.hh"
#include "RenShaTurbo.hh"
#include "GlobalSettings.hh"
#include "serialize.hh"

namespace openmsx {

MSXPPI::MSXPPI(const DeviceConfig& config)
	: MSXDevice(config)
	, cassettePort(getMotherBoard().getCassettePort())
	, renshaTurbo(getMotherBoard().getRenShaTurbo())
	, i8255(*this, getCurrentTime(), config.getGlobalSettings().getInvalidPpiModeSetting())
	, click(config)
	, keyboard(
		config.getMotherBoard(),
		config.getMotherBoard().getScheduler(),
		config.getMotherBoard().getCommandController(),
		config.getMotherBoard().getReactor().getEventDistributor(),
		config.getMotherBoard().getMSXEventDistributor(),
		config.getMotherBoard().getStateChangeDistributor(),
		Keyboard::Matrix::MSX, config)
{
	reset(getCurrentTime());
}

MSXPPI::~MSXPPI()
{
	powerDown(EmuTime::dummy());
}

void MSXPPI::reset(EmuTime time)
{
	i8255.reset(time);
	click.reset(time);
}

void MSXPPI::powerDown(EmuTime /*time*/)
{
	getLedStatus().setLed(LedStatus::CAPS, false);
}

uint8_t MSXPPI::readIO(uint16_t port, EmuTime time)
{
	return i8255.read(port & 0x03, time);
}

uint8_t MSXPPI::peekIO(uint16_t port, EmuTime time) const
{
	return i8255.peek(port & 0x03, time);
}

void MSXPPI::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	i8255.write(port & 0x03, value, time);
}


// I8255Interface

uint8_t MSXPPI::readA(EmuTime time)
{
	return peekA(time);
}
uint8_t MSXPPI::peekA(EmuTime /*time*/) const
{
	// port A is normally an output on MSX, reading from an output port
	// is handled internally in the 8255
	// TODO check this on a real MSX
	// TODO returning 0 fixes the 'get_selected_slot' script right after
	//      reset (when PPI directions are not yet set). For now this
	//      solution is good enough.
	return 0;
}
void MSXPPI::writeA(uint8_t value, EmuTime /*time*/)
{
	getCPUInterface().setPrimarySlots(value);
}

uint8_t MSXPPI::readB(EmuTime time)
{
	return peekB(time);
}
uint8_t MSXPPI::peekB(EmuTime time) const
{
	auto row = keyboard.getKeys()[selectedRow];
	if (selectedRow == 8) {
		row |= renshaTurbo.getSignal(time) ? 1 : 0;
	}
	return row;
}
void MSXPPI::writeB(uint8_t /*value*/, EmuTime /*time*/)
{
	// probably nothing happens on a real MSX
}

uint4_t MSXPPI::readC1(EmuTime time)
{
	return peekC1(time);
}
uint4_t MSXPPI::peekC1(EmuTime /*time*/) const
{
	return 15; // TODO check this
}
uint4_t MSXPPI::readC0(EmuTime time)
{
	return peekC0(time);
}
uint4_t MSXPPI::peekC0(EmuTime /*time*/) const
{
	return 15; // TODO check this
}
void MSXPPI::writeC1(uint4_t value, EmuTime time)
{
	if ((prevBits ^ value) & 1) {
		cassettePort.setMotor((value & 1) == 0, time); // 0=0n, 1=Off
	}
	if ((prevBits ^ value) & 2) {
		cassettePort.cassetteOut((value & 2) != 0, time);
	}
	if ((prevBits ^ value) & 4) {
		getLedStatus().setLed(LedStatus::CAPS, (value & 4) == 0);
	}
	if ((prevBits ^ value) & 8) {
		click.setClick((value & 8) != 0, time);
	}
	prevBits = value;
}
void MSXPPI::writeC0(uint4_t value, EmuTime /*time*/)
{
	selectedRow = value;
}


template<typename Archive>
void MSXPPI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255", i8255);

	// merge prevBits and selectedRow into one byte
	auto portC = uint8_t((prevBits << 4) | (selectedRow << 0));
	ar.serialize("portC", portC);
	if constexpr (Archive::IS_LOADER) {
		selectedRow = (portC >> 0) & 0xF;
		uint4_t bits = (portC >> 4) & 0xF;
		writeC1(bits, getCurrentTime());
	}
	ar.serialize("keyboard", keyboard);
}
INSTANTIATE_SERIALIZE_METHODS(MSXPPI);
REGISTER_MSXDEVICE(MSXPPI, "PPI");

} // namespace openmsx
