#pragma once

#include "Score.h"
#include "File.h"
#include "Utilities.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace MikuMikuWorld
{
	struct SerializingScore
	{
		Score score;
		ScoreMetadata metadata;
	};
	class ScoreSerializer
	{
	  public:
		virtual void serialize(const SerializingScore& scoreData, std::string filename) = 0;
		virtual SerializingScore deserialize(std::string filename) = 0;

		ScoreSerializer() {}
		virtual ~ScoreSerializer() {};
	};

	enum class SerializeFormat
	{
		NativeFormat,
		LegacyNativeFormat,
		SusFormat,
		UscFormat,
		LvlDataFormat,
		FormatCount
	};

	enum class SerializeResult
	{
		None,
		Cancel,
		SerializeSuccess,
		DeserializeSuccess,
		Error,
		PartialSerializeSuccess,
		PartialDeserializeSuccess,
	};

	class ScoreSerializeController
	{
	  protected:
		ScoreSerializeController() = default;

	  public:
		SerializingScore& getSerializeScore();
		const std::string& getFilename() const;
		const std::string& getName() const;
		const std::string& getErrorMessage() const;

		virtual SerializeResult update() = 0;
		virtual ~ScoreSerializeController() {};

		static SerializeFormat toSerializeFormat(const std::string_view& filename);
		static bool isValidFormat(SerializeFormat format);
		static IO::FileDialogFilter getFormatFilter(SerializeFormat format);
		static std::string getFormatDefaultExtension(SerializeFormat format);

	  protected:
		std::string errorMessage;
		std::string filename;
		std::string name;
		SerializingScore score;
	};

	class PartialScoreDeserializeError : public std::runtime_error
	{
	  public:
		PartialScoreDeserializeError(SerializingScore result, const std::string& message)
		    : partialResult(std::move(result)), std::runtime_error(message)
		{
		}

		inline SerializingScore& getResult() { return partialResult; }

	  private:
		SerializingScore partialResult;
	};
}