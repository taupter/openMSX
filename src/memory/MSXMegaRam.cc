/*
 * Adriano Camargo Rodrigues da Cunha wrote:
 *
 *  Any address inside a 8k page can change the page.  In other
 *  words:
 *
 *  for 4000h-5FFFh, mapping addresses are 4000h-5FFFh
 *  for 6000h-7FFFh, mapping addresses are 6000h-7FFFh
 *  for 8000h-9FFFh, mapping addresses are 8000h-9FFFh
 *  for A000h-BFFFh, mapping addresses are A000h-BFFFh
 *
 *  If you do an IN A,(8Eh) (the value of A register is unknown and
 *  never used) you can write on MegaRAM pages, but you can't map
 *  pages. If you do an OUT (8Eh),A (the value of A register doesn't
 *  matter) you can't write to MegaRAM pages, but you can map them.
 *
 *  Another thing: the MegaRAMs of Ademir Carchano have a mirror
 *  effect: if you map the page 0 of MegaRAM slot, you'll be
 *  accessing the same area of 8000h-BFFFh of this slot; if you map
 *  the page 3 of MegaRAM slot, you'll be accessing the same area of
 *  4000h-7FFFh of this slot. I don't know any software that makes
 *  use of this feature, except UZIX for MSX1.
 */

#include "MSXMegaRam.hh"
#include "DeviceConfig.hh"
#include "MSXException.hh"
#include "Rom.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <bit>
#include <memory>

namespace openmsx {

[[nodiscard]] static unsigned getNumBlocks(const DeviceConfig& config)
{
	int size = config.getChildDataAsInt("size", 0); // size in kB
	if (const auto* err = [&]() -> const char* {
		if (size < 0)         return "Cannot be negative.";
		if (size > (8 * 256)) return "Cannot be larger than 2048.";
		if ((size % 8) != 0)  return "Must be a multiple of 8.";
		return nullptr;
	}()) {
		throw MSXException("Invalid MegaRam size: ", size, ". ", err);
	}
	return size / 8;
}

MSXMegaRam::MSXMegaRam(DeviceConfig& config)
	: MSXDevice(config)
	, numBlocks(getNumBlocks(config)) // number of 8kB blocks
	, ram(config, getName() + " RAM", "Mega-RAM", numBlocks * 0x2000)
	, rom(config.findChild("rom")
	      ? std::make_unique<Rom>(getName() + " ROM", "Mega-RAM DiskROM", config)
	      : nullptr)
	, romBlockDebug(*this, bank, 0x0000, 0x10000, 13, 0, 3)
	, maskBlocks(narrow<byte>(std::bit_ceil(numBlocks) - 1))
{
	powerUp(EmuTime::dummy());
}

MSXMegaRam::~MSXMegaRam() = default;

void MSXMegaRam::powerUp(EmuTime time)
{
	for (auto i : xrange(byte(4))) {
		setBank(i, 0);
	}
	writeMode = false;
	ram.clear();
	reset(time);
}

void MSXMegaRam::reset(EmuTime /*time*/)
{
	// selected banks nor writeMode does change after reset
	romMode = rom != nullptr; // select rom mode if there is a rom
}

byte MSXMegaRam::readMem(uint16_t address, EmuTime /*time*/)
{
	return *MSXMegaRam::getReadCacheLine(address);
}

const byte* MSXMegaRam::getReadCacheLine(uint16_t address) const
{
	if (romMode) {
		if (address >= 0x4000 && address <= 0xBFFF) {
			return &(*rom)[address - 0x4000];
		}
		return unmappedRead.data();
	}
	unsigned block = bank[(address & 0x7FFF) / 0x2000];
	return (block < numBlocks)
	     ? &ram[(block * 0x2000) + (address & 0x1FFF)]
	     : unmappedRead.data();
}

void MSXMegaRam::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if (byte* tmp = getWriteCacheLine(address)) {
		*tmp = value;
	} else {
		assert(!romMode && !writeMode);
		setBank(narrow<byte>((address & 0x7FFF) / 0x2000), value);
	}
}

byte* MSXMegaRam::getWriteCacheLine(uint16_t address)
{
	if (romMode) return unmappedWrite.data();
	if (writeMode) {
		unsigned block = bank[(address & 0x7FFF) / 0x2000];
		return (block < numBlocks)
		     ? &ram[(block * 0x2000) + (address & 0x1FFF)]
		     : unmappedWrite.data();
	} else {
		return nullptr;
	}
}

byte MSXMegaRam::readIO(uint16_t port, EmuTime /*time*/)
{
	switch (port & 1) {
		case 0:
			// enable writing
			writeMode = true;
			romMode = false;
			break;
		case 1:
			if (rom) romMode = true;
			break;
	}
	invalidateDeviceRWCache();
	return 0xFF; // return value doesn't matter
}

byte MSXMegaRam::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return 0xFF;
}

void MSXMegaRam::writeIO(uint16_t port, byte /*value*/, EmuTime /*time*/)
{
	switch (port & 1) {
		case 0:
			// enable switching
			writeMode = false;
			romMode = false;
			break;
		case 1:
			if (rom) romMode = true;
			break;
	}
	invalidateDeviceRWCache();
}

void MSXMegaRam::setBank(byte page, byte block)
{
	bank[page] = block & maskBlocks;
	uint16_t adr = page * 0x2000;
	invalidateDeviceRWCache(adr + 0x0000, 0x2000);
	invalidateDeviceRWCache(adr + 0x8000, 0x2000);
}

template<typename Archive>
void MSXMegaRam::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ram",       ram,
	             "bank",      bank,
	             "writeMode", writeMode,
	             "romMode",   romMode);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMegaRam);
REGISTER_MSXDEVICE(MSXMegaRam, "MegaRAM");

} // namespace openmsx
