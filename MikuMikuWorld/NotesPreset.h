#pragma once
#include "Constants.h"
#include "ScoreContext.h"
#include <atomic>
#include <unordered_set>

namespace MikuMikuWorld
{
	class NotesPreset
	{
	  private:
		id_t ID;
		std::string filename;

	  public:
		NotesPreset(id_t id, std::string name);

		std::string name;
		std::string description;

		nlohmann::json data;

		inline std::string getName() const { return name; };
		inline std::string getFilename() const { return filename; }
		inline id_t getID() const { return ID; };

		Result read(const FilePath& filepath);
		void write(FilePath filepath);
	};

	enum class UpdateMode
	{
		Create,
		Delete
	};

	class PresetManager
	{
	  private:
		std::atomic<int> nextPresetID;
		std::vector<int> createPresets;
		std::vector<std::string> deletePresets;

	  public:
		std::unordered_map<int, NotesPreset> presets;

		void loadPresets(const FilePath& libPath);
		void savePresets(const FilePath& libPath);

		void createPreset(const ScoreContext& context, const std::string& name,
		                  const std::string& desc);

		void removePreset(int id);

		// Replaces illegal filesystem characters with '_'
		std::string fixFilename(const std::string& name);

		void applyPreset(int presetId, PasteData& pasteData);
	};
}
