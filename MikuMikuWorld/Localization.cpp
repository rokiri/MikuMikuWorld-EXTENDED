#include <filesystem>
#include "Application.h"
#include "Localization.h"
#include "IO.h"
#include "PlatformIO.h"
#include "Text.h"

namespace fs = std::filesystem;

namespace MikuMikuWorld
{
	static std::unordered_map<std::string, TranslationString>
	readLanguageFile(const std::string& filename)
	{
		std::unordered_map<std::string, TranslationString> translation;
		translation.reserve(320);
		std::ifstream file(filename);
		std::string line;
		for (size_t i = 1; std::getline(file, line); i++)
		{
			auto&& [text, comment] = IO::split_first(line, "#");
			if (text.empty())
				continue;
			auto&& [key, value] = IO::split_first(text, ",");
			key = IO::trim(key);
			value = IO::trim(value);
			if (key.empty() || value.empty())
				fprintf(stderr, "Invalid translation key on line %zd of '%s'", i, filename.c_str());
			else if (translation.try_emplace(key, TranslationString{ value }).second == false)
				fprintf(stderr, "Duplicated translation key '%s' in '%s'", key.c_str(),
				        filename.c_str());
		}
		return translation;
	}

	static std::string readLanguageName(const std::string& filename)
	{
		std::string name;
		std::ifstream file(filename);
		std::string line;
		while (std::getline(file, line))
		{
			auto&& [text, comment] = IO::split_first(line, "#");
			if (text.empty())
				continue;
			auto&& [key, value] = IO::split_first(text, ",");
			key = IO::trim(key);
			value = IO::trim(value);
			if (key == Text::languageName)
				return value;
		}
		return "";
	}

	void Localization::scanLanguages()
	{
		languages.clear();
		auto i18nPath = Application::getInstance().getResourcePath("i18n");
		if (!fs::exists(i18nPath) || !fs::is_directory(i18nPath))
			return;
		for (const auto& entry : fs::directory_iterator(i18nPath))
		{
			// look only for csv files and ignore any dot files present
			auto& path = entry.path();
			std::string filename = IO::toString(path.filename());
			// auto path = entry.path().filename();
			if (!entry.is_regular_file() || IO::startsWith(filename, ".") ||
			    path.extension() != ".csv")
				continue;
			std::string langName = readLanguageName(IO::toString(path));
			if (langName.empty())
				continue;
			std::string code = IO::toString(FilePath(path).replace_extension());
			languages.push_back({ code, langName });
		}
		// languages is order by (user's language) > english > (other languages)
		std::string systemLocale = IO::getSystemLocale();
		auto it = std::find_if(languages.begin(), languages.end(), [&](const Language& language)
		                       { return language.code == systemLocale; });
		if (it != languages.end() && it->code != "en")
			std::swap(*it, languages.front());
		if (systemLocale == "en" || languages.size() == 1)
			return;
		it = std::find_if(languages.begin(), languages.end(),
		                  [](const Language& language) { return language.code == "en"; });
		if (it != languages.end())
			std::swap(*it, *(languages.begin() + 1));
	}

	bool Localization::supportLanguage(const std::string& code)
	{
		if (languages.empty())
			scanLanguages();
		if (code == "auto")
			return languages.size() != 0;
		return std::any_of(languages.begin(), languages.end(),
		                   [&](const Language& language) { return language.code == code; });
	}

	bool Localization::setLanguage(const std::string& code)
	{
		if (languages.empty())
			scanLanguages();
		if (languages.empty() && code == "auto")
			return false;
		auto& langCode = code == "auto" ? languages.front().code : code;
		FilePath path =
		    Application::getInstance().getResourcePath("i18n", langCode).replace_extension(".csv");
		if (!fs::exists(path) || !fs::is_regular_file(path))
			return false;
		currentTranslation = readLanguageFile(IO::toString(path));
		// Since not all text are translated, we use english as filler for missing keys
		if (langCode != "en" && supportLanguage("en"))
			currentTranslation.merge(readLanguageFile(
			    IO::toString(Application::getInstance().getResourcePath("i18n", "en.csv"))));
		return true;
	}

	const TranslationString& localize(const std::string& text)
	{
		auto& translation = Application::instance->localization.currentTranslation;
		auto it = translation.find(text);
		if (it == translation.end())
			it = translation.emplace(text, TranslationString{ std::string(text) }).first;
		return it->second;
	}

	// Size checks
	// We don't put this in the header to avoid recursive include
	static_assert(std::size(insertModeTexts) == size_t(InsertMode::InsertModeMax));
	static_assert(std::size(flickTypeTexts) == size_t(FlickType::FlickTypeCount));
	static_assert(std::size(easeTypeTexts) == size_t(EaseType::EaseTypeCount));
	static_assert(std::size(stepTypeTexts) == size_t(EditHoldStepType::HoldStepTypeCount));
	static_assert(std::size(guideColorTexts) == size_t(GuideColor::GuideColorCount));
	static_assert(std::size(fadeTypeTexts) == size_t(FadeType::FadeTypeCount));
}
