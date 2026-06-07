#pragma once
#include "Score.h"
#include "Rendering/Texture.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <future>
#include <cstdint>

namespace MikuMikuWorld
{
	struct ChartState {
		bool isFavorite = false;
		std::string folder = "-"; 
	};

	struct GalleryItem {
		std::string filepath;
		std::string filename;
		std::string extension; 
		std::string jacketPath;
		std::string title;
		std::string artist;    
		std::string author;    
		int totalCombo = 0;    
		std::string lengthStr = "-"; 
		uint64_t modifiedTime = 0;
		uint64_t createdTime = 0;

		std::shared_ptr<Texture> texture; 
		bool isFavorite = false;
		std::string folder = "-";
	};

	class ChartGalleryWindow
	{
	private:
		bool recentLoaded = false;
		bool hasAutoScanned = false;
		int activeTab = 0;

		std::vector<std::string> searchPaths;
		char newPathBuffer[512] = "";
		std::string pathInputError = "";
		bool isSearchPathsOpen = true;
		char searchQuery[256] = "";
		int sortMode = 0;        // 0: modified, 1: created, 2: name, 3: combo
		int nameSortTarget = 0;  // 0: title, 1: file, 2: artist, 3: author
		bool sortAscending = false;
		bool folderDialogInProgress = false;
		std::future<std::string> folderDialogFuture{};

		std::unordered_map<std::string, ChartState> galleryStates;
		std::vector<std::shared_ptr<GalleryItem>> recentItems;
		std::vector<std::shared_ptr<GalleryItem>> localItems;
		std::vector<std::string> loadedRecentFiles;
		std::unique_ptr<Texture> defaultIcon = nullptr;
		std::unique_ptr<Texture> appIcon = nullptr;

		std::vector<std::string> customFolders;
		
		std::string itemToDelete = "";
		bool openDeletePopup = false;

		// --- Inline Management ---
		bool isCreatingNewFolder = false;
		int editingFolderIndex = -1; // -1: Not editing, 3+: Custom folders
		char folderEditBuffer[256] = "";

		void drawSidebar();
		void drawMainContent();
		void drawDeletePopup();

		void loadGalleryData();
		void saveGalleryData();
		std::shared_ptr<GalleryItem> loadItemInfo(const std::string& filepath);
		void drawGrid(std::vector<std::shared_ptr<GalleryItem>>& itemsToDraw, const char* gridId);
		void refreshRecentItems(const std::vector<std::string>& recentFiles);
		void sortItems(std::vector<std::shared_ptr<GalleryItem>>& items);
		void removeDeletedItemFromLists(const std::string& filepath);
		void deleteFolder(int index);
		void scanSearchPaths();
		bool addSearchPath(const std::string& path);
		void startFolderDialog();
		void pollFolderDialog();

	public:
		bool open = true;
		std::string pendingLoadScore = ""; 
		bool pendingCreateNew = false;

		void update(const std::vector<std::string>& recentFiles);
	};
}
