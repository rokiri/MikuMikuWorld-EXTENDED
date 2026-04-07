#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "Text.h"
#include "Utilities.h"

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
		using MapType = std::unordered_map<std::string_view, TranslationString>;

		std::vector<Language> languages;
		MapType currentTranslation;

		void scanLanguages();
		bool supportLanguage(const std::string& locale);
		bool setLanguage(const std::string& locale);

		MapType::iterator insertText(std::string text, TranslationString str);
		MapType::iterator updateText(std::string_view text, TranslationString str);

	  private:
		void readLanguageFile(const std::string& filename);

		std::vector<char> keyStorage;
	};

	Localization& getLocalization();
	const TranslationString& localize(std::string_view text);
	template <typename StrFactory, typename... Args>
	inline auto localizeOrInsert(std::string_view text, StrFactory&& f, Args&&... args) ->
	    typename std::enable_if_t<
	        std::is_constructible_v<std::string, std::invoke_result_t<StrFactory, Args...>>,
	        const TranslationString&>
	{
		auto& localizer = getLocalization();
		auto it = localizer.currentTranslation.find(text);
		if (it != localizer.currentTranslation.end())
			return it->second;
		std::string str(text),
		    val = std::invoke(std::forward<StrFactory>(f), std::forward<Args>(args)...);
		return localizer.insertText(std::move(str), { std::move(val) })->second;
	}

	MMW_TEXT_TYPE insertModeTexts[]{ Text::select, Text::tap,      Text::hold,  Text::holdStep,
		                             Text::flick,  Text::critical, Text::trace, Text::guide,
		                             Text::damage, Text::dummy,    Text::bpm,   Text::timeSignature,
		                             Text::hiSpeed };

	MMW_TEXT_TYPE flickTypeTexts[]{ Text::none, Text::defaultValue, Text::left,     Text::right,
		                            Text::down, Text::downLeft,     Text::downRight };

	MMW_TEXT_TYPE easeTypeTexts[]{ Text::linear,    Text::easeIn,    Text::easeOut,
		                           Text::easeInOut, Text::easeOutIn, Text::easeNone };

	MMW_TEXT_TYPE soundEffectTexts[]{ Text::sfxDefault,  Text::sfxNone,      Text::sfxTap,
		                              Text::sfxFlick,    Text::sfxTrace,     Text::sfxTick,
		                              Text::sfxCritTap,  Text::sfxCritFlick, Text::sfxCritTrace,
		                              Text::sfxCritTick, Text::sfxDamage };

	MMW_TEXT_TYPE stepTypeTexts[]{ Text::normal, Text::hidden, Text::skip };

	MMW_TEXT_TYPE guideColorTexts[]{ Text::guideGreen, Text::guideYellow };

	MMW_TEXT_TYPE guideColorAllTexts[]{ Text::guideNeutral, Text::guideRed,    Text::guideGreen,
		                                Text::guideBlue,    Text::guideYellow, Text::guidePurple,
		                                Text::guideCyan,    Text::guideBlack };

	MMW_TEXT_TYPE fadeTypeTexts[]{ Text::fadeOut, Text::fadeNone, Text::fadeIn, Text::fadeCustom,
		                           Text::fadeClassic };

	MMW_TEXT_TYPE snapModeTexts[]{ Text::snapModeRelative, Text::snapModeAbsolute,
		                           Text::snapModeIndividualAbsolute };

	MMW_TEXT_TYPE holdJointTypeTexts[]{ Text::normal, Text::trace, Text::hidden };

	MMW_TEXT_TYPE baseThemeTexts[]{ Text::themeDark, Text::themeLight };

	MMW_TEXT_TYPE hispeedEaseTypeTexts[]{ Text::hiSpeedEaseNone, Text::hiSpeedEaseLinear };

	MMW_TEXT_TYPE skillEffectTexts[]{ Text::skillEffectScore, Text::skillEffectHeal,
		                              Text::skillEffectPerfect };

	MMW_TEXT_TYPE holdLayerTexts[]{ Text::holdLayerTop, Text::holdLayerBottom };
}