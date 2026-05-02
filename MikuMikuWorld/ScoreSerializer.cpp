#include "ScoreSerializer.h"
#include "Localization.h"
#include "Constants.h"
#include "IO.h"
#include "File.h"
#include <array>
#include <algorithm>

namespace MikuMikuWorld
{
	SerializeFormat ScoreSerializeController::toSerializeFormat(const std::string_view& filename)
	{
		const auto hasExtension = IO::endsWith;
		if (hasExtension(filename, CC_MMWS_EXTENSION) || hasExtension(filename, UC_MMWS_EXTENSION))
		{
			return SerializeFormat::NativeFormat;
		}
		else if (hasExtension(filename, MMWS_EXTENSION))
		{
			return SerializeFormat::LegacyNativeFormat;
		}
		else if (hasExtension(filename, SUS_EXTENSION))
		{
			return SerializeFormat::SusFormat;
		}
		else if (hasExtension(filename, USC_EXTENSION))
		{
			return SerializeFormat::UscFormat;
		}
		else if (hasExtension(filename, JSON_EXTENSION) ||
		         hasExtension(filename, GZ_JSON_EXTENSION))
		{
			return SerializeFormat::LvlDataFormat;
		}
		return SerializeFormat::FormatCount;
	}

	bool ScoreSerializeController::isValidFormat(SerializeFormat format)
	{
		return static_cast<int>(format) >= 0 &&
		       static_cast<int>(format) < static_cast<int>(SerializeFormat::FormatCount);
	}

	IO::FileDialogFilter ScoreSerializeController::getFormatFilter(SerializeFormat format)
	{
		switch (format)
		{
		case SerializeFormat::NativeFormat:
			return IO::mmwsNativeFilter;
		case SerializeFormat::LegacyNativeFormat:
			return IO::mmwsLegacyFilter;
		case SerializeFormat::SusFormat:
			return IO::susFilter;
		case SerializeFormat::UscFormat:
			return IO::uscFilter;
		case SerializeFormat::LvlDataFormat:
			return IO::lvlDatFilter;
		default:
			return IO::allFilter;
		}
	}

	std::string ScoreSerializeController::getFormatDefaultExtension(SerializeFormat format)
	{
		std::string extension;
		switch (format)
		{
		case SerializeFormat::NativeFormat:
			extension = UC_MMWS_EXTENSION;
			break;
		case SerializeFormat::LegacyNativeFormat:
			extension = MMWS_EXTENSION;
			break;
		case SerializeFormat::SusFormat:
			extension = SUS_EXTENSION;
			break;
		case SerializeFormat::UscFormat:
			extension = USC_EXTENSION;
			break;
		case SerializeFormat::LvlDataFormat:
			extension = GZ_JSON_EXTENSION;
			break;
		default:
			return "";
		}
		extension.erase(extension.begin()); // remove the initial '.'
		return extension;
	}

	SerializingScore& ScoreSerializeController::getSerializeScore() { return score; }

	const std::string& ScoreSerializeController::getFilename() const { return filename; }

	const std::string& ScoreSerializeController::getName() const { return name; }

	const std::string& ScoreSerializeController::getErrorMessage() const { return errorMessage; }
}