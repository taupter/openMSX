#ifndef MSXHBI55_HH
#define MSXHBI55_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "SRAM.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXHBI55 final : public MSXDevice, public I8255Interface
{
public:
	explicit MSXHBI55(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	[[nodiscard]] uint8_t readA (EmuTime time) override;
	[[nodiscard]] uint8_t readB (EmuTime time) override;
	[[nodiscard]] uint4_t readC0(EmuTime time) override;
	[[nodiscard]] uint4_t readC1(EmuTime time) override;
	[[nodiscard]] uint8_t peekA (EmuTime time) const override;
	[[nodiscard]] uint8_t peekB (EmuTime time) const override;
	[[nodiscard]] uint4_t peekC0(EmuTime time) const override;
	[[nodiscard]] uint4_t peekC1(EmuTime time) const override;
	void writeA (uint8_t value, EmuTime time) override;
	void writeB (uint8_t value, EmuTime time) override;
	void writeC0(uint4_t value, EmuTime time) override;
	void writeC1(uint4_t value, EmuTime time) override;

	void writeStuff();
	uint8_t readStuff() const;

private:
	I8255 i8255;
	SRAM sram;
	uint8_t lastC; // hack to approach a 'floating value'
};
SERIALIZE_CLASS_VERSION(MSXHBI55, 2);

} // namespace openmsx

#endif
