#include <filesystem>
#include "Application.h"
#include "Localization.h"
#include "IO.h"
#include "PlatformIO.h"
#include "Text.h"

namespace fs = std::filesystem;

namespace MikuMikuWorld
{
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
			std::string_view code = filename;
			code.remove_suffix(std::strlen(".csv"));
			languages.push_back({ std::string(code), langName });
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
		Application& app = Application::getInstance();
		FilePath path = app.getResourcePath("i18n", langCode).replace_extension(".csv");
		if (!fs::exists(path) || !fs::is_regular_file(path))
			return false;
		currentTranslation.clear();
		keyStorage.clear();
		readLanguageFile(IO::toString(path));
		// Since not all text are translated, we use english as filler for missing keys
		if (langCode != "en" && supportLanguage("en"))
			readLanguageFile(IO::toString(app.getResourcePath("i18n", "en.csv")));
		return true;
	}

	auto Localization::insertText(std::string text, TranslationString str)
	    -> Localization::MapType::iterator
	{
		if ((text.size() + keyStorage.size()) > keyStorage.capacity())
		{
			std::vector<char> newStorage;
			newStorage.reserve(std::max<size_t>(keyStorage.size() * 1.5, 4096));
			std::copy(keyStorage.begin(), keyStorage.end(), std::back_inserter(newStorage));
			Localization::MapType newTranslation;
			newTranslation.reserve(std::max<size_t>(currentTranslation.size(), 320));
			for (auto&& [key, value] : currentTranslation)
			{
				ptrdiff_t offset = key.data() - keyStorage.data();
				newTranslation.insert(
				    { std::string_view(newStorage.data() + offset, key.size()), std::move(value) });
			}
			keyStorage = std::move(newStorage);
			currentTranslation = std::move(newTranslation);
		}
		size_t cur = keyStorage.size();
		keyStorage.insert(keyStorage.end(), text.begin(), text.end());
		std::string_view key = { keyStorage.data() + cur, text.size() };
		auto&& [it, inserted] = currentTranslation.try_emplace(key, std::move(str));
		if (!inserted)
			keyStorage.erase(keyStorage.begin() + cur, keyStorage.end());
		return it;
	}

	Localization::MapType::iterator Localization::updateText(std::string_view text,
	                                                         TranslationString str)
	{
		auto& trans = getLocalization().currentTranslation;
		auto it = trans.find(text);
		if (it == trans.end())
			return insertText(std::string(text), std::move(str));
		it->second = std::move(str);
		return it;
	}

	void Localization::readLanguageFile(const std::string& filename)
	{
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
			else
				insertText(std::move(key), { std::move(value) });
		}
	}

	Localization& getLocalization() { return Application::instance->localization; }

	const TranslationString& localize(std::string_view text)
	{
		auto& localizer = getLocalization();
		auto it = localizer.currentTranslation.find(text);
		if (it != localizer.currentTranslation.end())
			return it->second;
#ifdef _DEBUG
		printf("Translation key for '%.*s' not found!\n", unsigned(text.size()), text.data());
#endif
		std::string str(text);
		return localizer.insertText(str, { str })->second;
	}

	// Size checks
	// We don't put this in the header to avoid recursive include
	static_assert(std::size(insertModeTexts) == size_t(InsertMode::InsertModeMax));
	static_assert(std::size(snapModeTexts) == size_t(SnapMode::SnapModeMax));
	static_assert(std::size(flickTypeTexts) == size_t(FlickType::FlickTypeCount));
	static_assert(std::size(easeTypeTexts) == size_t(EaseType::EaseTypeCount));
	static_assert(std::size(holdJointTypeTexts) == size_t(EditHoldJointType::JointTypeCount));
	static_assert(std::size(stepTypeTexts) == size_t(EditHoldStepType::HoldStepTypeCount));
	static_assert(std::size(guideColorAllTexts) == size_t(GuideColor::GuideColorCount));
	static_assert(std::size(fadeTypeTexts) == size_t(FadeType::FadeTypeCount));
	static_assert(std::size(baseThemeTexts) == size_t(BaseTheme::BASE_THEME_MAX));
	static_assert(std::size(hispeedEaseTypeTexts) == size_t(HiSpeedEaseType::EaseTypeCount));
}
