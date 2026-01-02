#include "File.h"
#include "IO.h"
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace IO
{
	fs::path stringToPath(const std::string& str) { return fs::path(utf8ToWide(str)); }
	fs::path stringToPath(const std::wstring& str) { return fs::path(str); }

	FileDialogFilter mmwsFilter{ "MikuMikuWorld Score", "*.unchmmws;*.ccmmws;*.mmws" };
	FileDialogFilter susFilter{ "Sliding Universal Score", "*.sus" };
	FileDialogFilter uscFilter{ "Universal Sekai Chart", "*.usc" };
	FileDialogFilter lvlDatFilter{ "Sonolus Level Data", "*.json.gz;*.json" };
	FileDialogFilter imageFilter{ "Image Files", "*.jpg;*.jpeg;*.png" };
	FileDialogFilter audioFilter{ "Audio Files", "*.mp3;*.wav;*.flac;*.ogg" };
	FileDialogFilter presetFilter{ "Notes Preset", "*.json" };
	FileDialogFilter allFilter{ "All Files", "*.*" };

	File::File(const std::string& filename, FileMode mode) { open(filename, mode); }

	File::File(const std::wstring& filename, FileMode mode) { open(filename, mode); }

	File::~File()
	{
		if (stream.is_open())
			stream.close();
	}

	int File::getStreamMode(FileMode mode) const
	{
		switch (mode)
		{
		case FileMode::Read:
			return std::fstream::in;
		case FileMode::Write:
			return std::fstream::out;
		case FileMode::ReadBinary:
			return std::fstream::in | std::fstream::binary;
		case FileMode::WriteBinary:
			return std::fstream::out | std::fstream::binary;
		default:
			return 0;
		}
	}

	void File::open(const std::string& filename, FileMode mode)
	{
		openFilename = filename;
		openFilenameW = IO::utf8ToWide(filename);
		stream.open(openFilenameW, getStreamMode(mode));
	}

	void File::open(const std::wstring& filename, FileMode mode)
	{
		openFilename = IO::wideToUtf8(filename);
		openFilenameW = filename;
		stream.open(openFilenameW, getStreamMode(mode));
	}

	void File::close()
	{
		openFilename.clear();
		openFilenameW.clear();
		stream.close();
	}

	void File::flush() { stream.flush(); }

	std::vector<uint8_t> File::readAllBytes()
	{
		if (!stream.is_open())
			return {};

		size_t length = fs::file_size(openFilenameW);
		stream.seekg(0, std::ios_base::beg);

		std::vector<uint8_t> bytes(length, 0);
		stream.read((char*)bytes.data(), length);

		return bytes;
	}

	std::string File::readLine()
	{
		if (!stream.is_open())
			return {};

		std::string line;
		std::getline(stream, line);

		return line;
	}

	std::vector<std::string> File::readAllLines()
	{
		if (!stream.is_open())
			return {};

		stream.seekg(0, std::ios_base::beg);
		std::string line;
		std::vector<std::string> lines;
		while (std::getline(stream, line))
			lines.push_back(line);

		return lines;
	}

	std::string File::readAllText()
	{
		if (!stream.is_open())
			return {};

		stream.seekg(0, std::ios_base::beg);
		return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	}

	bool File::isEndofFile() const { return stream.is_open() ? stream.eof() : true; }

	void File::write(const std::string& str)
	{
		if (stream.is_open())
			stream << str;
	}

	void File::writeLine(const std::string& line)
	{
		if (stream.is_open())
			stream << line << '\n';
	}

	void File::writeAllLines(const std::vector<std::string>& lines)
	{
		if (stream.is_open())
			for (const auto& line : lines)
				stream << line << '\n';
	}

	void File::writeAllBytes(const std::vector<uint8_t>& bytes)
	{
		if (stream.is_open())
			stream.write((char*)bytes.data(), bytes.size());
	}

	// --------------------------------------------------------

	std::string File::getFilename(const std::string& filename)
	{
		return wideToUtf8(stringToPath(filename).filename());
	}

	std::string File::getFileExtension(const std::string& filename)
	{
		return wideToUtf8(stringToPath(filename).extension());
	}

	std::string File::getFilenameWithoutExtension(const std::string& filename)
	{
		return wideToUtf8(stringToPath(filename).filename().replace_extension());
	}

	std::string File::getFilepath(const std::string& filename)
	{
		return wideToUtf8(stringToPath(filename).replace_filename(""));
	}

	size_t File::getFileSize(const std::string& filename)
	{
		return fs::file_size(stringToPath(filename));
	}

	bool File::exists(const std::string& path) { return fs::exists(stringToPath(path)); }

	bool File::exists(const std::wstring& path) { return fs::exists(path); }

	FileDialogResult FileDialog::showFileDialog(DialogType type, DialogSelectType selectType)
	{
		std::wstring wTitle = utf8ToWide(title);

		OPENFILENAMEW ofn;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = reinterpret_cast<HWND>(parentWindowHandle);
		ofn.lpstrTitle = wTitle.c_str();
		ofn.nFilterIndex = filterIndex + 1;
		ofn.nFileOffset = 0;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_LONGNAMES | OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT |
		            OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

		std::wstring wDefaultExtension = utf8ToWide(defaultExtension);
		ofn.lpstrDefExt = wDefaultExtension.c_str();

		std::vector<std::wstring> ofnFilters;
		ofnFilters.reserve(filters.size());

		/*
		    since '\0' terminates the string,
		    we'll do a C# by using ' | ' then replacing it with '\0' when constructing the final
		   wide string
		*/
		std::string filtersCombined;
		for (const auto& filter : filters)
		{
			filtersCombined.append(filter.filterName)
			    .append(" (")
			    .append(filter.filterType)
			    .append(")|")
			    .append(filter.filterType)
			    .append("|");
		}

		std::wstring wFiltersCombined = utf8ToWide(filtersCombined);
		std::replace(wFiltersCombined.begin(), wFiltersCombined.end(), '|', '\0');
		ofn.lpstrFilter = wFiltersCombined.c_str();

		std::wstring wInputFilename = utf8ToWide(inputFilename);
		wchar_t ofnFilename[1024]{ 0 };

		// suppress return value not used warning
#pragma warning(suppress : 6031)
		lstrcpynW(ofnFilename, wInputFilename.c_str(), 1024);
		ofn.lpstrFile = ofnFilename;

		if (type == DialogType::Save)
		{
			ofn.Flags |= OFN_HIDEREADONLY;
			if (GetSaveFileNameW(&ofn))
			{
				outputFilename = wideToUtf8(ofn.lpstrFile);
			}
			else
			{
				// user canceled
				return FileDialogResult::Cancel;
			}
		}
		else if (GetOpenFileNameW(&ofn))
		{
			outputFilename = wideToUtf8(ofn.lpstrFile);
		}
		else
		{
			return FileDialogResult::Cancel;
		}

		return outputFilename.empty() ? FileDialogResult::Cancel : FileDialogResult::OK;
	}

	FileDialogResult FileDialog::openFile()
	{
		return showFileDialog(DialogType::Open, DialogSelectType::File);
	}

	FileDialogResult FileDialog::saveFile()
	{
		return showFileDialog(DialogType::Save, DialogSelectType::File);
	}
}
