#include "File.h"
#include "IO.h"
#include "PlatformIO.h"
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace IO
{
	FileDialogFilter mmwsFilter{ "Any MikuMikuWorld Score", "*.unchmmws;*.ccmmws;*.mmws" };
	FileDialogFilter mmwsNativeFilter{ "MikuMikuWorld 4 UntitledCharts Score", "*.unchmmws" };
	FileDialogFilter mmwsLegacyFilter{ "MikuMikuWorld Score", "*.mmws" };
	FileDialogFilter susFilter{ "Sliding Universal Score", "*.sus" };
	FileDialogFilter uscFilter{ "Universal Sekai Chart", "*.usc" };
	FileDialogFilter lvlDatFilter{ "Sonolus Level Data", "*.json.gz;*.json" };
	FileDialogFilter imageFilter{ "Image Files", "*.jpg;*.jpeg;*.png" };
	FileDialogFilter audioFilter{ "Audio Files", "*.mp3;*.wav;*.flac;*.ogg" };
	FileDialogFilter presetFilter{ "Notes Preset", "*.json" };
	FileDialogFilter allFilter{ "All Files", "*.*" };

	File::File(const FilePath& filepath, FileMode mode) { open(filepath, mode); }

	File::File(const std::string& filename, FileMode mode) { open(filename, mode); }

	File::File(const std::wstring& filename, FileMode mode) { open(filename, mode); }

	File::~File()
	{
		if (stream.is_open())
		{
			stream.flush();
			stream.close();
		}
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

	void File::open(const FilePath& filepath, FileMode mode)
	{
		openFilename = toString(filepath);
		openFilenameW = toWString(filepath);
		stream.open(filepath, getStreamMode(mode));
	}

	void File::open(const std::string& filename, FileMode mode)
	{
		open(stringToPath(filename), mode);
	}

	void File::open(const std::wstring& filename, FileMode mode)
	{
		open(stringToPath(filename), mode);
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

	FilePath File::getFilename(const FilePath& path) { return path.filename(); }

	FilePath File::getFileExtension(const FilePath& path) { return path.extension(); }

	FilePath File::getFilenameWithoutExtension(const FilePath& path)
	{
		return path.filename().replace_extension();
	}

	size_t File::getFileSize(const FilePath& path) { return fs::file_size(path); }

	bool File::exists(const FilePath& path) { return fs::exists(path); }

	FileDialogFilter combineFilters(const std::string& filterName,
	                                const std::initializer_list<FileDialogFilter>& filters)
	{
		std::string filterType;
		if (!filters.size())
			return { filterName, "*.*" };
		filterType.reserve(std::accumulate(filters.begin(), filters.end(), size_t(0),
		                                   [](size_t sz, const FileDialogFilter& filter)
		                                   { return sz + filter.filterType.size() + 1; }) -
		                   1);
		auto begin = filters.begin(), end = filters.end();
		filterType += (begin++)->filterType;
		for (; begin != end; ++begin)
			filterType.append(";").append(begin->filterType);
		return { filterName, filterType };
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
