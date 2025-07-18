#include "PrinterPortLogger.hh"
#include "PlugException.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "serialize.hh"

namespace openmsx {

PrinterPortLogger::PrinterPortLogger(CommandController& commandController)
	: logFilenameSetting(
		commandController, "printerlogfilename",
		"filename of the file where the printer output is logged to",
		"printer.log")
{
}

bool PrinterPortLogger::getStatus(EmuTime /*time*/)
{
	return false; // false = low = ready
}

void PrinterPortLogger::setStrobe(bool strobe, EmuTime /*time*/)
{
	if (file.is_open() && !strobe && prevStrobe) {
		// falling edge
		file.write(std::span{&toPrint, 1});
		file.flush(); // optimize when it turns out flushing
		               // every time is too slow
	}
	prevStrobe = strobe;
}

void PrinterPortLogger::writeData(uint8_t data, EmuTime /*time*/)
{
	toPrint = data;
}

void PrinterPortLogger::plugHelper(
		Connector& /*connector*/, EmuTime /*time*/)
{
	try {
		file = File(userFileContext().resolve(logFilenameSetting.getString()),
		            File::OpenMode::TRUNCATE);
	} catch (FileException& e) {
		throw PlugException("Couldn't plug printer logger: ",
		                    e.getMessage());
	}
}

void PrinterPortLogger::unplugHelper(EmuTime /*time*/)
{
	file.close();
}

std::string_view PrinterPortLogger::getName() const
{
	return "logger";
}

std::string_view PrinterPortLogger::getDescription() const
{
	return "Log everything that is sent to the printer port to a "
	       "file. The filename can be set with the "
	       "'printerlogfilename' setting.";
}

template<typename Archive>
void PrinterPortLogger::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// We don't try to resume logging to the same file.
	// And to not accidentally loose a previous log, we don't
	// overwrite that file automatically. So after savestate/loadstate,
	// you have to replug the PrinterPortLogger
}
INSTANTIATE_SERIALIZE_METHODS(PrinterPortLogger);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, PrinterPortLogger, "PrinterPortLogger");

} // namespace openmsx
