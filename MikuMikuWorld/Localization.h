#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "Text.h"

namespace MikuMikuWorld
{
	struct TranslationString
	{
		std::string string;

		inline operator const char*() const { return string.c_str(); }
		inline operator std::string_view() const { return string; }
	};

	struct Language
	{
		std::string code;
		std::string languageName;
	};

	struct Localization
	{
		std::vector<Language> languages;
		std::unordered_map<std::string, TranslationString> currentTranslation;

		void scanLanguages();
		bool supportLanguage(const std::string& locale);
		bool setLanguage(const std::string& locale);
	};

	const TranslationString& localize(const std::string& text);

	inline constexpr const char* insertModeTexts[]{
		Text::select,   Text::tap,           Text::hold,   Text::holdStep, Text::flick,
		Text::critical, Text::trace,         Text::guide,  Text::damage,   Text::dummy,
		Text::bpm,      Text::timeSignature, Text::hiSpeed
	};

	inline constexpr const char* flickTypeTexts[]{ Text::none,     Text::defaultValue,
		                                           Text::left,     Text::right,
		                                           Text::down,     Text::downLeft,
		                                           Text::downRight };

	inline constexpr const char* easeTypeTexts[]{ Text::linear, Text::easeIn, Text::easeOut,
		                                          Text::easeInOut, Text::easeOutIn };

	inline constexpr const char* stepTypeTexts[]{ Text::normal, Text::hidden, Text::skip };

	inline constexpr const char* guideColorTexts[]{
		//
		Text::guideNeutral, Text::guideRed,    Text::guideGreen, Text::guideBlue,
		Text::guideYellow,  Text::guidePurple, Text::guideCyan,  Text::guideBlack
	};

	inline constexpr const char* fadeTypeTexts[]{ Text::fadeOut, Text::fadeNone, Text::fadeIn };
}