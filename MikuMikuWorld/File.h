#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <numeric>

using FilePath = std::filesystem::path;
namespace IO
{
	enum class FileMode : uint8_t
	{
		Read,
		Write,
		ReadBinary,
		WriteBinary
	};

	class File
	{
	  public:
		static FilePath getFilename(const FilePath& path);
		static FilePath getFileExtension(const FilePath& path);
		static FilePath getFilenameWithoutExtension(const FilePath& path);
		static size_t getFileSize(const FilePath& path);
		static bool exists(const FilePath& path);

		void open(const FilePath& filepath, FileMode mode);
		void open(const std::string& filename, FileMode mode);
		void open(const std::wstring& filename, FileMode mode);
		void close();
		void flush();

		std::vector<uint8_t> readAllBytes();
		std::string readLine();
		std::vector<std::string> readAllLines();
		std::string readAllText();
		void write(const std::string& str);
		void writeLine(const std::string& line);
		void writeAllLines(const std::vector<std::string>& lines);
		void writeAllBytes(const std::vector<uint8_t>& bytes);
		bool isEndofFile() const;

		std::string_view getOpenFilename() const { return openFilename; }
		std::wstring_view getOpenFilenameW() const { return openFilenameW; }

		File(const FilePath& filepath, FileMode mode);
		File(const std::string& filename, FileMode mode);
		File(const std::wstring& filename, FileMode mode);
		~File();

	  private:
		std::fstream stream{};
		std::string openFilename{};
		std::wstring openFilenameW{};

		int getStreamMode(FileMode) const;
	};

	enum class FileDialogResult : uint8_t
	{
		Error,
		Cancel,
		OK
	};

	struct FileDialogFilter
	{
		std::string filterName;
		std::string filterType;
	};

	class FileDialog
	{
	  private:
		enum class DialogType : uint8_t
		{
			Open,
			Save
		};

		enum class DialogSelectType : uint8_t
		{
			File,
			Folder
		};

		FileDialogResult showFileDialog(DialogType type, DialogSelectType selectType);

	  public:
		std::string title;
		std::vector<FileDialogFilter> filters;
		std::string inputFilename;
		std::string outputFilename;
		std::string defaultExtension;
		uint32_t filterIndex = 0;
		void* parentWindowHandle = nullptr;

		FileDialogResult openFile();
		FileDialogResult saveFile();
	};

	FileDialogFilter combineFilters(const std::string& filterName,
	                                const std::initializer_list<FileDialogFilter>& filters);

	extern FileDialogFilter mmwsFilter;
	extern FileDialogFilter mmwsNativeFilter;
	extern FileDialogFilter mmwsLegacyFilter;
	extern FileDialogFilter susFilter;
	extern FileDialogFilter uscFilter;
	extern FileDialogFilter lvlDatFilter;
	extern FileDialogFilter imageFilter;
	extern FileDialogFilter audioFilter;
	extern FileDialogFilter presetFilter;
	extern FileDialogFilter allFilter;
}
