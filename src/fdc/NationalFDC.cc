#include "NationalFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

NationalFDC::NationalFDC(DeviceConfig& config)
	: WD2793BasedFDC(config)
{
	// ROM only visible in 0x0000-0x7FFF by default
	parseRomVisibility(config, 0x0000, 0x8000);
}

byte NationalFDC::readMem(uint16_t address, EmuTime time)
{
	switch (address & 0x3FC7) {
	case 0x3F80:
		return controller.getStatusReg(time);
	case 0x3F81:
		return controller.getTrackReg(time);
	case 0x3F82:
		return controller.getSectorReg(time);
	case 0x3F83:
		return controller.getDataReg(time);
	case 0x3F84:
	case 0x3F85:
	case 0x3F86:
	case 0x3F87: {
		byte value = 0x7F;
		if (controller.getIRQ(time))  value |=  0x80;
		if (controller.getDTRQ(time)) value &= ~0x40;
		return value;
	}
	default:
		return NationalFDC::peekMem(address, time);
	}
}

byte NationalFDC::peekMem(uint16_t address, EmuTime time) const
{
	// According to atarulum:
	//  7FBC        is mirrored in 7FBC - 7FBF
	//  7FB8 - 7FBF is mirrored in 7F80 - 7FBF
	switch (address & 0x3FC7) {
	case 0x3F80:
		return controller.peekStatusReg(time);
	case 0x3F81:
		return controller.peekTrackReg(time);
	case 0x3F82:
		return controller.peekSectorReg(time);
	case 0x3F83:
		return controller.peekDataReg(time);
	case 0x3F84:
	case 0x3F85:
	case 0x3F86:
	case 0x3F87: {
		// Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
		// bit 7: intrq
		// bit 6: !dtrq
		// other bits read 1
		byte value = 0x7F;
		if (controller.peekIRQ(time))  value |=  0x80;
		if (controller.peekDTRQ(time)) value &= ~0x40;
		return value;
	}
	default:
		return MSXFDC::peekMem(address, time);
	}
}

const byte* NationalFDC::getReadCacheLine(uint16_t start) const
{
	if ((start & 0x3FC0 & CacheLine::HIGH) == (0x3F80 & CacheLine::HIGH)) {
		// FDC at 0x7FB8-0x7FBC (also mirrored)
		return nullptr;
	} else {
		return MSXFDC::getReadCacheLine(start);
	}
}

void NationalFDC::writeMem(uint16_t address, byte value, EmuTime time)
{
	switch (address & 0x3FC7) {
	case 0x3F80:
		controller.setCommandReg(value, time);
		break;
	case 0x3F81:
		controller.setTrackReg(value, time);
		break;
	case 0x3F82:
		controller.setSectorReg(value, time);
		break;
	case 0x3F83:
		controller.setDataReg(value, time);
		break;
	case 0x3F84:
	case 0x3F85:
	case 0x3F86:
	case 0x3F87:
		// bit 0 -> select drive 0
		// bit 1 -> select drive 1
		// bit 2 -> side select
		// bit 3 -> motor on
		auto drive = [&] {
			switch (value & 3) {
				case 1:  return DriveMultiplexer::Drive::A;
				case 2:  return DriveMultiplexer::Drive::B;
				default: return DriveMultiplexer::Drive::NONE;
			}
		}();
		multiplexer.selectDrive(drive, time);
		multiplexer.setSide((value & 0x04) != 0);
		multiplexer.setMotor((value & 0x08) != 0, time);
		break;
	}
}

byte* NationalFDC::getWriteCacheLine(uint16_t address)
{
	if ((address & 0x3FC0) == (0x3F80 & CacheLine::HIGH)) {
		// FDC at 0x7FB8-0x7FBC (also mirrored)
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}


template<typename Archive>
void NationalFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(NationalFDC);
REGISTER_MSXDEVICE(NationalFDC, "NationalFDC");

} // namespace openmsx
