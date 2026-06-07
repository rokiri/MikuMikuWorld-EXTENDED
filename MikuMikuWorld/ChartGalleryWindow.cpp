#include "ChartGalleryWindow.h"
#include "Application.h"
#include "File.h"
#include "NativeScoreSerializer.h"
#include "ScoreStats.h"
#include "Audio/Sound.h"
#include "UI.h"
#include "IconsFontAwesome5.h"
#include "Rendering/Texture.h"
#include "Localization.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <Windows.h>
#include "json.hpp"

namespace
{
	uint64_t fileTimeToUint64(const FILETIME& fileTime)
	{
		ULARGE_INTEGER value{};
		value.LowPart = fileTime.dwLowDateTime;
		value.HighPart = fileTime.dwHighDateTime;
		return value.QuadPart;
	}

	void getFileTimes(const std::string& filepath, uint64_t& createdTime, uint64_t& modifiedTime)
	{
		WIN32_FILE_ATTRIBUTE_DATA fileData{};
		if (!GetFileAttributesExW(IO::mbToWideStr(filepath).c_str(), GetFileExInfoStandard, &fileData))
			return;

		createdTime = fileTimeToUint64(fileData.ftCreationTime);
		modifiedTime = fileTimeToUint64(fileData.ftLastWriteTime);
	}

	std::string toLowerAscii(std::string s)
	{
		for (char& c : s) {
			if (c >= 'A' && c <= 'Z')
				c += ('a' - 'A');
		}
		return s;
	}

	int compareTextForSort(const std::string& left, const std::string& right)
	{
		std::wstring leftWide = IO::mbToWideStr(left);
		std::wstring rightWide = IO::mbToWideStr(right);
		int result = CompareStringEx(
			LOCALE_NAME_USER_DEFAULT,
			NORM_IGNORECASE | NORM_IGNOREWIDTH | NORM_IGNOREKANATYPE,
			leftWide.c_str(),
			-1,
			rightWide.c_str(),
			-1,
			nullptr,
			nullptr,
			0);

		if (result == CSTR_LESS_THAN) return -1;
		if (result == CSTR_GREATER_THAN) return 1;
		if (result == CSTR_EQUAL) return 0;

		std::string leftKey = toLowerAscii(left);
		std::string rightKey = toLowerAscii(right);
		if (leftKey < rightKey) return -1;
		if (leftKey > rightKey) return 1;
		if (left < right) return -1;
		if (left > right) return 1;
		return 0;
	}
}

namespace MikuMikuWorld
{
	void ChartGalleryWindow::loadGalleryData()
	{
		std::string path = Application::getAppDir() + "gallery_config.json";
		std::ifstream file(IO::mbToWideStr(path)); 
		if (file.is_open()) {
			nlohmann::json j;
			try {
				file >> j;
				if (j.contains("charts")) {
					for (auto& [filepath, data] : j["charts"].items()) {
						galleryStates[filepath] = {
							data.value("isFavorite", false),
							data.value("folder", "-")
						};
					}
				}
				if (j.contains("folders")) {
					customFolders.clear();
					for (auto& folder : j["folders"]) {
						customFolders.push_back(folder.get<std::string>());
					}
				}
				if (j.contains("searchPaths")) {
					searchPaths.clear();
					for (auto& p : j["searchPaths"]) {
						searchPaths.push_back(p.get<std::string>());
					}
				} else if (j.contains("searchPath")) {
					searchPaths.push_back(j["searchPath"].get<std::string>());
				}
				if (j.contains("isSearchPathsOpen")) {
					isSearchPathsOpen = j["isSearchPathsOpen"].get<bool>();
				}
			} catch (...) {}
		}
	}

	void ChartGalleryWindow::saveGalleryData()
	{
		nlohmann::json j;
		for (const auto& [filepath, state] : galleryStates) {
			if (state.isFavorite || state.folder != "-") {
				j["charts"][filepath] = {
					{"isFavorite", state.isFavorite},
					{"folder", state.folder}
				};
			}
		}
		j["folders"] = customFolders;
		j["searchPaths"] = searchPaths;
		j["isSearchPathsOpen"] = isSearchPathsOpen;
		std::string path = Application::getAppDir() + "gallery_config.json";
		std::ofstream file(IO::mbToWideStr(path));
		if (file.is_open()) {
			file << j.dump(4);
		}
	}

	std::shared_ptr<GalleryItem> ChartGalleryWindow::loadItemInfo(const std::string& filepath)
	{
		auto item = std::make_shared<GalleryItem>();
		item->filepath = filepath;
		item->filename = IO::File::getFilenameWithoutExtension(filepath);
		item->extension = IO::File::getFileExtension(filepath);
		item->title = "-"; 
		item->artist = "-";
		item->author = "-";
		item->folder = "-";
		getFileTimes(filepath, item->createdTime, item->modifiedTime);

		if (galleryStates.count(filepath)) {
			item->isFavorite = galleryStates[filepath].isFavorite;
			item->folder = galleryStates[filepath].folder;
		}

		try {
			Score tempScore = NativeScoreSerializer().deserialize(filepath);
			if (!tempScore.metadata.title.empty()) item->title = tempScore.metadata.title;
			if (!tempScore.metadata.artist.empty()) item->artist = tempScore.metadata.artist;
			if (!tempScore.metadata.author.empty()) item->author = tempScore.metadata.author;
			ScoreStats stats{};
			stats.calculateStats(tempScore);
			item->totalCombo = stats.getCombo();

			if (!tempScore.metadata.musicFile.empty()) {
				std::string absAudioPath = tempScore.metadata.musicFile;
				if (!IO::File::exists(absAudioPath)) {
					absAudioPath = IO::File::getFilepath(filepath) + tempScore.metadata.musicFile;
				}
				if (IO::File::exists(absAudioPath)) {
					Audio::SoundBuffer tempBuffer;
					if (Audio::decodeAudioFile(absAudioPath, tempBuffer).isOk()) {
						if (tempBuffer.sampleRate > 0) {
							double totalSeconds = static_cast<double>(tempBuffer.frameCount) / tempBuffer.sampleRate;
							int minutes = static_cast<int>(totalSeconds / 60);
							double seconds = totalSeconds - minutes * 60;
							std::ostringstream timeStream;
							timeStream << std::setfill('0') << std::setw(2) << minutes << ":"
									   << std::setfill('0') << std::fixed << std::setprecision(3) << seconds;
							item->lengthStr = timeStream.str();
						}
						tempBuffer.dispose();
					}
				}
			}

			item->jacketPath = tempScore.metadata.jacketFile;
			std::string absJacketPath = item->jacketPath;
			if (!absJacketPath.empty()) {
				if (!IO::File::exists(absJacketPath)) absJacketPath = IO::File::getFilepath(filepath) + item->jacketPath;
				if (IO::File::exists(absJacketPath)) item->texture = std::make_shared<Texture>(absJacketPath);
			}
		} catch (...) {}
		return item;
	}

	void ChartGalleryWindow::refreshRecentItems(const std::vector<std::string>& recentFiles)
	{
		if (recentFiles == loadedRecentFiles)
			return;

		loadedRecentFiles = recentFiles;
		recentItems.clear();
		for (const std::string& filepath : recentFiles)
			if (IO::File::exists(filepath))
				recentItems.push_back(loadItemInfo(filepath));
	}

	void ChartGalleryWindow::sortItems(std::vector<std::shared_ptr<GalleryItem>>& items)
	{
		auto isUnset = [](const std::string& value) {
			return value.empty() || value == "-";
		};

		auto applyDirection = [&](int comparison) {
			if (comparison == 0)
				return false;
			return sortAscending ? comparison < 0 : comparison > 0;
		};

		std::sort(items.begin(), items.end(), [&](const std::shared_ptr<GalleryItem>& a, const std::shared_ptr<GalleryItem>& b) {
			auto compareName = [&](const std::string& left, const std::string& right, const std::string& leftFallback, const std::string& rightFallback) {
				bool leftUnset = isUnset(left);
				bool rightUnset = isUnset(right);
				if (leftUnset != rightUnset)
					return !leftUnset;

				const std::string& leftValue = leftUnset ? leftFallback : left;
				const std::string& rightValue = rightUnset ? rightFallback : right;
				int valueComparison = compareTextForSort(leftValue, rightValue);
				if (valueComparison != 0)
					return applyDirection(valueComparison);

				int fallbackComparison = compareTextForSort(leftFallback, rightFallback);
				if (fallbackComparison != 0)
					return fallbackComparison < 0;

				return a->filepath < b->filepath;
			};

			auto compareNameNoUnset = [&](const std::string& left, const std::string& right) {
				int valueComparison = compareTextForSort(left, right);
				if (valueComparison != 0)
					return applyDirection(valueComparison);

				return a->filepath < b->filepath;
			};

			auto compareNumber = [&](uint64_t left, uint64_t right) {
				if (left != right)
					return sortAscending ? left < right : left > right;

				int filenameComparison = compareTextForSort(a->filename, b->filename);
				if (filenameComparison != 0)
					return filenameComparison < 0;

				return a->filepath < b->filepath;
			};

			if (sortMode == 0) return compareNumber(a->modifiedTime, b->modifiedTime);
			if (sortMode == 1) return compareNumber(a->createdTime, b->createdTime);
			if (sortMode == 3) return compareNumber(static_cast<uint64_t>(a->totalCombo), static_cast<uint64_t>(b->totalCombo));

			if (nameSortTarget == 0) return compareName(a->title, b->title, a->filename, b->filename);
			if (nameSortTarget == 1) return compareNameNoUnset(a->filename, b->filename);
			if (nameSortTarget == 2) return compareName(a->artist, b->artist, a->filename, b->filename);
			if (nameSortTarget == 3) return compareName(a->author, b->author, a->filename, b->filename);
			return a->filepath < b->filepath;
		});
	}

	void ChartGalleryWindow::removeDeletedItemFromLists(const std::string& filepath)
	{
		auto removePredicate = [&](const std::shared_ptr<GalleryItem>& item) { return item->filepath == filepath; };
		recentItems.erase(std::remove_if(recentItems.begin(), recentItems.end(), removePredicate), recentItems.end());
		localItems.erase(std::remove_if(localItems.begin(), localItems.end(), removePredicate), localItems.end());
		galleryStates.erase(filepath);
		saveGalleryData();
	}

	void ChartGalleryWindow::deleteFolder(int index)
	{
		std::string folderName = "";
		if (index == 3) folderName = "Team Projects";
		else if (index == 4) folderName = "Personal";
		else if (index >= 5 && (index - 5) < (int)customFolders.size()) folderName = customFolders[index - 5];

		if (folderName.empty()) return;

		for (auto& [path, state] : galleryStates) if (state.folder == folderName) state.folder = "-";
		for (auto& item : recentItems) if (item->folder == folderName) item->folder = "-";
		for (auto& item : localItems) if (item->folder == folderName) item->folder = "-";

		if (index >= 5) customFolders.erase(customFolders.begin() + (index - 5));

		activeTab = 0; 
		saveGalleryData();
	}

	void ChartGalleryWindow::scanSearchPaths()
	{
		localItems.clear();
		try {
			for (const auto& pathStr : searchPaths) {
				std::wstring wp = IO::mbToWideStr(pathStr);
				if (std::filesystem::exists(wp)) {
					for (const auto& e : std::filesystem::recursive_directory_iterator(wp)) {
						if (e.is_regular_file()) {
							std::string ex = e.path().extension().string();
							if (ex == ".mmws" || ex == ".ccmmws" || ex == ".unchmmws") {
								localItems.push_back(loadItemInfo(IO::wideStringToMb(e.path().wstring())));
							}
						}
					}
				}
			}
		} catch (...) {}
	}

	bool ChartGalleryWindow::addSearchPath(const std::string& path)
	{
		pathInputError.clear();

		if (path.empty())
			return false;

		std::filesystem::path folderPath = IO::mbToWideStr(path);
		std::error_code errorCode{};
		if (!std::filesystem::exists(folderPath, errorCode) || !std::filesystem::is_directory(folderPath, errorCode)) {
			pathInputError = getString("gallery_invalid_search_path");
			return false;
		}

		if (std::find(searchPaths.begin(), searchPaths.end(), path) == searchPaths.end()) {
			searchPaths.push_back(path);
			saveGalleryData();
		}

		scanSearchPaths();
		return true;
	}

	void ChartGalleryWindow::startFolderDialog()
	{
		if (folderDialogInProgress)
			return;

		std::string title = getString("gallery_browse_folder");
		void* parentWindowHandle = Application::windowState.windowHandle;
		folderDialogInProgress = true;
		folderDialogFuture = std::async(std::launch::async, [title, parentWindowHandle]() {
			IO::FileDialog fileDialog{};
			fileDialog.title = title;
			fileDialog.parentWindowHandle = parentWindowHandle;

			if (fileDialog.openFolder() == IO::FileDialogResult::OK)
				return fileDialog.outputFilename;

			return std::string{};
		});
	}

	void ChartGalleryWindow::pollFolderDialog()
	{
		if (!folderDialogInProgress || !folderDialogFuture.valid())
			return;

		if (folderDialogFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return;

		std::string selectedPath = folderDialogFuture.get();
		folderDialogInProgress = false;

		if (!selectedPath.empty())
			addSearchPath(selectedPath);
	}

	void ChartGalleryWindow::drawGrid(std::vector<std::shared_ptr<GalleryItem>>& itemsToDraw, const char* gridId)
	{
		if (itemsToDraw.empty()) {
			ImGui::TextDisabled("%s", (std::string("  ") + getString("gallery_no_charts")).c_str());
			return;
		}

		float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		float cardWidth = 315.0f; 
		float cardHeight = 165.0f; 
		float spacing = 24.0f;
		float padding = 8.0f;
		float imageSize = 120.0f;

		for (int i = 0; i < (int)itemsToDraw.size(); ++i)
		{
			auto& item = itemsToDraw[i];
			if (galleryStates.count(item->filepath)) {
				item->isFavorite = galleryStates[item->filepath].isFavorite;
				item->folder = galleryStates[item->filepath].folder;
			}

			ImGui::PushID(gridId); 
			ImGui::PushID(item->filepath.c_str());

			ImGui::BeginGroup(); 
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(cardWidth, cardHeight));
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight), IM_COL32(35, 35, 35, 255), 5.0f);

			ImVec2 imgPos = ImVec2(cursorPos.x + padding, cursorPos.y + padding);
			if (item->texture) {
				// --- 変更ここから：アスペクト比を保った中央トリミング処理 ---
				ImVec2 uv0 = ImVec2(0.0f, 0.0f);
				ImVec2 uv1 = ImVec2(1.0f, 1.0f);
				float texW = (float)item->texture->getWidth();
				float texH = (float)item->texture->getHeight();
				
				if (texW > texH) {
					// 横長画像の場合：短い方(高さ)に合わせて、左右の端を均等にクロップ
					float offset = (texW - texH) / 2.0f;
					uv0.x = offset / texW;
					uv1.x = (offset + texH) / texW;
				} else if (texH > texW) {
					// 縦長画像の場合：短い方(幅)に合わせて、上下の端を均等にクロップ
					float offset = (texH - texW) / 2.0f;
					uv0.y = offset / texH;
					uv1.y = (offset + texW) / texH;
				}
				
				// 計算した切り取り範囲(uv0, uv1)を使って描画
				drawList->AddImageRounded((void*)(intptr_t)item->texture->getID(), imgPos, ImVec2(imgPos.x + imageSize, imgPos.y + imageSize), uv0, uv1, IM_COL32(255, 255, 255, 255), 4.0f);
				// --- 変更ここまで ---
			} else if (defaultIcon) {
				drawList->AddImageRounded((void*)(intptr_t)defaultIcon->getID(), imgPos, ImVec2(imgPos.x + imageSize, imgPos.y + imageSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
			}

			ImVec2 heartBtnPos = ImVec2(imgPos.x + 4, imgPos.y + 4);
			drawList->AddRectFilled(heartBtnPos, ImVec2(heartBtnPos.x + 24, heartBtnPos.y + 24), IM_COL32(0, 0, 0, 120), 4.0f);
			
			ImGui::SetCursorScreenPos(heartBtnPos);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, item->isFavorite ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
			if (ImGui::Button(ICON_FA_HEART, ImVec2(24, 24))) {
				item->isFavorite = !item->isFavorite;
				galleryStates[item->filepath].isFavorite = item->isFavorite;
				saveGalleryData();
			}
			bool favHovered = ImGui::IsItemHovered();
			ImGui::PopStyleColor(2);

			float textStartX = imgPos.x + imageSize + padding + 15.0f;
			float textStartY = cursorPos.y + padding + 2.0f;
			float labelWidth = 65.0f;
			float valueStartX = textStartX + labelWidth + 12.0f;
			drawList->AddLine(ImVec2(textStartX + labelWidth, textStartY), ImVec2(textStartX + labelWidth, textStartY + 7 * 16.5f), IM_COL32(80, 80, 80, 255));

			// ここで翻訳キーを使ってラベルを表示します
			const char* labels[] = { getString("gallery_file"), getString("gallery_format"), getString("gallery_title"), getString("gallery_artist"), getString("gallery_author"), getString("gallery_time"), getString("gallery_combo") };
			std::string values[] = { item->filename, item->extension, item->title, item->artist, item->author, item->lengthStr, std::to_string(item->totalCombo) };
			for (int row = 0; row < 7; ++row) {
				float rowY = textStartY + row * 16.5f;
				drawList->AddText(ImVec2(textStartX, rowY), IM_COL32(160, 160, 160, 255), labels[row]);
				ImVec2 clipMin(valueStartX, rowY);
				ImVec2 clipMax(cursorPos.x + cardWidth - padding, rowY + 16.0f);
				drawList->PushClipRect(clipMin, clipMax, true);
				drawList->AddText(clipMin, IM_COL32(230, 230, 230, 255), values[row].c_str());
				drawList->PopClipRect();
			}

			float bottomY = imgPos.y + imageSize + 7.5f;
			ImVec2 titleBarStart(imgPos.x, bottomY);
			ImVec2 titleBarEnd(imgPos.x + imageSize, bottomY + 22.0f);
			drawList->AddRectFilled(titleBarStart, titleBarEnd, IM_COL32(45, 45, 45, 255));

			ImVec2 textSize = ImGui::CalcTextSize(item->title.c_str());
			float textX = titleBarStart.x + (imageSize - textSize.x) * 0.5f;
			if (textX < titleBarStart.x + 4.0f) textX = titleBarStart.x + 4.0f;

			drawList->PushClipRect(titleBarStart, titleBarEnd, true);
			drawList->AddText(ImVec2(textX, titleBarStart.y + 4.0f), IM_COL32(255, 255, 255, 255), item->title.c_str());
			drawList->PopClipRect();
			
			// タグ名の表示（システムフォルダーの場合は翻訳キーを参照します）
			std::string displayFolderName = item->folder;
			if (displayFolderName == "Team Projects") displayFolderName = getString("gallery_team_projects");
			else if (displayFolderName == "Personal") displayFolderName = getString("gallery_personal");
			else if (displayFolderName == "-") displayFolderName = getString("gallery_uncategorized");
			
			std::string tagText = std::string(ICON_FA_TAG " ") + displayFolderName;
			ImVec2 tagPos = ImVec2(imgPos.x + imageSize + 12.0f, bottomY + 4.0f);

			float trashButtonStartX = cursorPos.x + cardWidth - padding - 24.0f;
			ImVec2 clipMin = tagPos;
			ImVec2 clipMax = ImVec2(trashButtonStartX - 8.0f, tagPos.y + 20.0f);

			drawList->PushClipRect(clipMin, clipMax, true);
			drawList->AddText(tagPos, IM_COL32(153, 153, 153, 255), tagText.c_str());
			drawList->PopClipRect();

			ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + cardWidth - padding - 24.0f, bottomY - 2.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button(ICON_FA_TRASH, ImVec2(24, 24))) { itemToDelete = item->filepath; openDeletePopup = true; }
			bool trashHovered = ImGui::IsItemHovered();
			ImGui::PopStyleColor();

			ImGui::EndGroup();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 6.0f));

			if (ImGui::BeginPopupContextItem("ChartContextMenu"))
			{
					if (ImGui::BeginMenu(getString("gallery_move_to_folder")))
					{
					if (ImGui::MenuItem(getString("gallery_uncategorized"), NULL, item->folder == "-")) {
						item->folder = "-";
						galleryStates[item->filepath].folder = "-";
						saveGalleryData();
					}
					
					ImGui::Dummy(ImVec2(0.0f, 2.0f));
					ImGui::Separator();
					ImGui::Dummy(ImVec2(0.0f, 2.0f));

					std::vector<std::string> displayFolders = { "Team Projects", "Personal" };
					for (const auto& f : customFolders) {
						if (f != "Team Projects" && f != "Personal") displayFolders.push_back(f);
					}

					for (const auto& folderName : displayFolders) {
						// メニュー内のシステムフォルダー名を翻訳して表示
						std::string menuDisplayName = folderName;
						if (folderName == "Team Projects") menuDisplayName = getString("gallery_team_projects");
						else if (folderName == "Personal") menuDisplayName = getString("gallery_personal");
						
						if (ImGui::MenuItem(menuDisplayName.c_str(), NULL, item->folder == folderName)) {
							item->folder = folderName;
							galleryStates[item->filepath].folder = folderName;
							saveGalleryData();
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}

			ImGui::PopStyleVar(3); 

			if (ImGui::IsItemHovered())
				drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight), IM_COL32(255, 255, 255, 15), 5.0f);

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !favHovered && !trashHovered) {
				pendingLoadScore = item->filepath;
			}

			float lastGroupX = ImGui::GetItemRectMax().x;
			float nextGroupX = lastGroupX + spacing + cardWidth;
			if (i + 1 < (int)itemsToDraw.size()) {
				if (nextGroupX < windowVisibleX) ImGui::SameLine(0, spacing);
				else ImGui::Dummy(ImVec2(0, 10.0f));
			}

		ImGui::PopID();
		ImGui::PopID();
		}
	}

	void ChartGalleryWindow::update(const std::vector<std::string>& recentFiles)
	{
		if (!open) return;
	
		if (!recentLoaded) {
			loadGalleryData();
			std::string iconPath = Application::getAppDir() + "res\\textures\\gallery\\placeholder.png";
			if (IO::File::exists(iconPath)) defaultIcon = std::make_unique<Texture>(iconPath);
			std::string appIconPath = Application::getAppDir() + "res\\mmw_icon.png";
			if (IO::File::exists(appIconPath)) {
				appIcon = std::make_unique<Texture>(appIconPath);
			}
			recentLoaded = true;
		}
		refreshRecentItems(recentFiles);
	
		if (!hasAutoScanned && !searchPaths.empty()) {
			scanSearchPaths();
			hasAutoScanned = true;
		}
	
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
	
		if (ImGui::Begin("GalleryScreen", &open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
		{
			if (openDeletePopup) { ImGui::OpenPopup(getString("gallery_delete_file")); openDeletePopup = false; }
			
			if (ImGui::BeginTable("GalleryLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
				ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 200.0f);
				ImGui::TableSetupColumn("Main", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
	
				ImGui::TableSetColumnIndex(0);
				drawSidebar(); // 切り出した関数を呼び出す
	
				ImGui::TableSetColumnIndex(1);
				drawMainContent(); // 切り出した関数を呼び出す
	
				ImGui::EndTable();
			}
	
			drawDeletePopup(); // 切り出した関数を呼び出す
		}
		ImGui::End();
	}

	void ChartGalleryWindow::drawSidebar()
	{
		if (ImGui::BeginChild("SidebarChild", ImVec2(0, 0), false)) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
	
		ImGui::Dummy(ImVec2(0.0f, 12.0f));

		// ボタンを描画して、クリックされたら true を返す共通処理
		auto drawLargeSidebarButton = [&](const char* id, const char* text, Texture* iconTex, const char* leftIconStr, const char* rightIconStr) -> bool {
			ImVec2 btnPos = ImGui::GetCursorScreenPos();
			float btnWidth = ImGui::GetContentRegionAvail().x - 8.0f;
			float btnHeight = 60.0f;

			ImGui::InvisibleButton(id, ImVec2(btnWidth, btnHeight));
			bool clicked = ImGui::IsItemClicked();

			bool hovered = ImGui::IsItemHovered();
			bool active = ImGui::IsItemActive();

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImU32 bgColor = active ? IM_COL32(50, 50, 50, 255) : (hovered ? IM_COL32(85, 85, 85, 255) : IM_COL32(65, 65, 65, 255));
			drawList->AddRectFilled(btnPos, ImVec2(btnPos.x + btnWidth, btnPos.y + btnHeight), bgColor, 4.0f);

			float iconSizeVal = 32.0f;
			float textStartX = btnPos.x + 12.0f;

			// アプリアイコン または FontAwesomeアイコンを左に描画
			if (iconTex) {
				ImVec2 iconPos = ImVec2(btnPos.x + 10.0f, btnPos.y + (btnHeight - iconSizeVal) * 0.5f);
				drawList->AddImage((void*)(intptr_t)iconTex->getID(), iconPos, ImVec2(iconPos.x + iconSizeVal, iconPos.y + iconSizeVal));
				textStartX = iconPos.x + iconSizeVal + 12.0f;
			} else if (leftIconStr) {
				ImVec2 leftIconSize = ImGui::CalcTextSize(leftIconStr);
				ImVec2 iconPos = ImVec2(btnPos.x + 16.0f, btnPos.y + (btnHeight - leftIconSize.y) * 0.5f);
				drawList->AddText(iconPos, IM_COL32(200, 200, 200, 255), leftIconStr);
				textStartX = iconPos.x + leftIconSize.x + 16.0f;
			}

			// テキストを描画
			ImVec2 textSize = ImGui::CalcTextSize(text);
			float textY = btnPos.y + (btnHeight - textSize.y) * 0.5f;
			drawList->AddText(ImVec2(textStartX, textY), IM_COL32(240, 240, 240, 255), text);

			// 右端のアイコン（+マークなど）を描画
			if (rightIconStr) {
				ImVec2 rightIconSize = ImGui::CalcTextSize(rightIconStr);
				float rightIconX = btnPos.x + btnWidth - rightIconSize.x - 12.0f;
				float rightIconY = btnPos.y + (btnHeight - rightIconSize.y) * 0.5f;
				drawList->AddText(ImVec2(rightIconX, rightIconY), IM_COL32(180, 180, 180, 255), rightIconStr);
			}

			return clicked;
		};

		if (drawLargeSidebarButton("CreateNewChartBtn", getString("gallery_create_new_chart"), appIcon.get(), nullptr, ICON_FA_PLUS)) {
			pendingCreateNew = true; // 新規作成フラグを立てる
			open = false;            // ギャラリーを閉じる
		}

		ImGui::Dummy(ImVec2(0.0f, 8.0f));

		if (drawLargeSidebarButton("ReturnToEditorBtn", getString("gallery_return_to_editor"), nullptr, ICON_FA_ARROW_LEFT, nullptr)) {
			open = false; // ギャラリーを閉じるだけ（現在の譜面を維持）
		}

			ImGui::Dummy(ImVec2(0.0f, 12.0f));
	
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); 
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
	
			if (ImGui::Button(ICON_FA_PLUS, ImVec2(30, 30))) {
				isCreatingNewFolder = true;
				memset(folderEditBuffer, 0, sizeof(folderEditBuffer));
			}
			ImGui::SameLine();
	
			if (ImGui::Button(ICON_FA_TRASH, ImVec2(30, 30))) { if (activeTab >= 5) deleteFolder(activeTab); }
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_PEN, ImVec2(30, 30))) {
				if (activeTab >= 5) {
					editingFolderIndex = activeTab;
					std::string currentName = customFolders[activeTab - 5];
					strncpy(folderEditBuffer, currentName.c_str(), 256);
				}
			}
	
			ImGui::PopStyleColor(3);
	
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0.0f, 4.0f));
	
			auto drawSelectable = [&](const char* label, int index) {
				bool selected = (activeTab == index);
				if (editingFolderIndex == index) {
					ImGui::SetNextItemWidth(-8);
					ImGui::SetKeyboardFocusHere();
					if (ImGui::InputText("##EditFolder", folderEditBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
						std::string oldName = customFolders[index - 5];
						std::string newName = folderEditBuffer;
						if (!newName.empty()) {
							customFolders[index - 5] = newName;
							for (auto& [path, state] : galleryStates) if (state.folder == oldName) state.folder = newName;
							for (auto& item : recentItems) if (item->folder == oldName) item->folder = newName;
							for (auto& item : localItems) if (item->folder == oldName) item->folder = newName;
							saveGalleryData();
						}
						editingFolderIndex = -1;
					}
					if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) editingFolderIndex = -1;
				} else {
					ImVec2 pos = ImGui::GetCursorScreenPos();
					float width = ImGui::GetContentRegionAvail().x - 8.0f;
					float height = 30.0f;
	
					ImGui::InvisibleButton(label, ImVec2(width, height));
					if (ImGui::IsItemClicked()) activeTab = index;
					
					bool hovered = ImGui::IsItemHovered();
					
					if (hovered && ImGui::IsMouseDoubleClicked(0) && index >= 5) {
						editingFolderIndex = index;
						std::string currentName = customFolders[index - 5];
						strncpy(folderEditBuffer, currentName.c_str(), 256);
					}
	
					ImDrawList* drawList = ImGui::GetWindowDrawList();
	
					ImU32 bgColor;
					if (selected) {
						bgColor = ImGui::GetColorU32(ImGuiCol_SliderGrab);
					} else if (hovered) {
						bgColor = IM_COL32(85, 85, 85, 255);   
					} else {
						bgColor = IM_COL32(65, 65, 65, 255);   
					}
	
					drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bgColor, 3.0f);
	
					ImVec2 textSize = ImGui::CalcTextSize(label);
					float textY = pos.y + (height - textSize.y) * 0.5f; 
	
					float arrowIconStartX = pos.x + width - 24.0f; 
					ImVec2 textClipMin = ImVec2(pos.x + 12.0f, pos.y);
					ImVec2 textClipMax = ImVec2(arrowIconStartX - 8.0f, pos.y + height);
	
					drawList->PushClipRect(textClipMin, textClipMax, true);
					drawList->AddText(ImVec2(pos.x + 12.0f, textY), IM_COL32(230, 230, 230, 255), label);
					drawList->PopClipRect();
	
					const char* arrowIcon = ICON_FA_CHEVRON_RIGHT;
					ImVec2 arrowSize = ImGui::CalcTextSize(arrowIcon);
					float arrowX = pos.x + width - arrowSize.x - 12.0f;
					float arrowY = pos.y + (height - arrowSize.y) * 0.5f;
					drawList->AddText(ImVec2(arrowX, arrowY), IM_COL32(180, 180, 180, 255), arrowIcon);
	
					ImGui::Dummy(ImVec2(0.0f, 4.0f)); 
				}
			};
	
			drawSelectable((std::string("  ") + getString("gallery_all")).c_str(), 0);
			drawSelectable((std::string("  ") + getString("gallery_recent")).c_str(), 1);
			drawSelectable((std::string("  ") + getString("gallery_favorites")).c_str(), 2);
			drawSelectable((std::string("  ") + getString("gallery_team_projects")).c_str(), 3);
			drawSelectable((std::string("  ") + getString("gallery_personal")).c_str(), 4);
	
			ImGui::Separator(); 
			ImGui::Dummy(ImVec2(0.0f, 4.0f));
	
			for (int i = 0; i < (int)customFolders.size(); ++i) {
				drawSelectable((std::string("  ") + customFolders[i]).c_str(), 5 + i);
			}
	
			if (isCreatingNewFolder) {
				ImGui::SetNextItemWidth(-8);
				ImGui::SetKeyboardFocusHere();
				if (ImGui::InputText("##NewFolder", folderEditBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
					if (strlen(folderEditBuffer) > 0) { customFolders.push_back(folderEditBuffer); saveGalleryData(); }
					isCreatingNewFolder = false;
				}
				if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) isCreatingNewFolder = false;
			}
		}
		ImGui::EndChild();
	}

	void ChartGalleryWindow::drawMainContent()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 0.0f));

	if (ImGui::BeginChild("MainContentChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
		pollFolderDialog();

		std::string filterFolder = "";
		if (activeTab == 3) filterFolder = "Team Projects";
		else if (activeTab == 4) filterFolder = "Personal";
		else if (activeTab >= 5 && (activeTab - 5) < (int)customFolders.size()) filterFolder = customFolders[activeTab - 5];

		if (activeTab == 0 || activeTab == 1) {
			std::vector<std::shared_ptr<GalleryItem>> recentToDraw = recentItems;
			bool hasMore = (activeTab == 0 && recentToDraw.size() > 4);
			if (hasMore) recentToDraw.resize(4);

			ImGui::Spacing(); 
			ImGui::Text("%s", getString("gallery_recent_charts"));
			ImGui::Separator();
			ImGui::Spacing();

			drawGrid(recentToDraw, "RecentGrid");

			if (hasMore) {
				ImGui::SameLine(0, 16.0f);
				ImGui::BeginGroup();
				float moreWidth = 100.0f;
				float moreHeight = 165.0f;
				ImVec2 pos = ImGui::GetCursorScreenPos();
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddRectFilled(pos, ImVec2(pos.x + moreWidth, pos.y + moreHeight), IM_COL32(50, 50, 50, 255), 5.0f);
				if (ImGui::InvisibleButton("ShowMoreBtn", ImVec2(moreWidth, moreHeight))) activeTab = 1;
				if (ImGui::IsItemHovered()) dl->AddRectFilled(pos, ImVec2(pos.x + moreWidth, pos.y + moreHeight), IM_COL32(255, 255, 255, 20), 5.0f);
				
				const char* iconTxt = ICON_FA_ARROW_RIGHT;
				const char* moreTxt = getString("gallery_more");
				ImVec2 iconSize = ImGui::CalcTextSize(iconTxt);
				ImVec2 textSize = ImGui::CalcTextSize(moreTxt);
				float centerX = pos.x + (moreWidth / 2.0f);
				dl->AddText(ImVec2(centerX - iconSize.x / 2.0f, pos.y + (moreHeight / 2.0f) - 15), IM_COL32(200, 200, 200, 255), iconTxt);
				dl->AddText(ImVec2(centerX - textSize.x / 2.0f, pos.y + (moreHeight / 2.0f) + 5), IM_COL32(200, 200, 200, 255), moreTxt);
				ImGui::EndGroup();
			}
		}
		
		if (activeTab == 0)
		{
			ImGui::Dummy(ImVec2(0.0f, 20.0f));
			ImGui::Text("%s", getString("gallery_all_charts"));
			ImGui::Separator();
			ImGui::Spacing();

			std::string toggleBtnText = isSearchPathsOpen ? (std::string(ICON_FA_CHEVRON_DOWN) + " " + getString("gallery_hide_search_paths")) : (std::string(ICON_FA_CHEVRON_RIGHT) + " " + getString("gallery_show_search_paths"));
			if (ImGui::Button(toggleBtnText.c_str()))
			{
				isSearchPathsOpen = !isSearchPathsOpen;
				saveGalleryData();
			}

			ImGui::SameLine();

			ImVec4 accentColor = ImGui::GetStyle().Colors[ImGuiCol_SliderGrab];
			ImVec4 accentHovered = accentColor; accentHovered.w = 0.8f; 
			ImVec4 accentActive = ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive]; 

			ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHovered);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentActive);

			if (ImGui::Button(getString("gallery_rescan_paths")))
			{
				saveGalleryData();
				scanSearchPaths();
			}
			ImGui::PopStyleColor(3); 

			ImGui::SameLine();

			float sortDirectionWidth = 110.0f;
			float totalWidth = 250.0f + 170.0f + 150.0f + sortDirectionWidth + 42.0f; 
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - totalWidth - 16.0f); 

			ImGui::TextDisabled(ICON_FA_SEARCH); 
			float iconWidth = ImGui::CalcTextSize(ICON_FA_SEARCH).x;
			float searchBoxWidth = 250.0f;

			ImGui::SetNextItemWidth(searchBoxWidth - iconWidth - 8.0f);
			ImGui::SameLine(); 
			ImGui::InputText("##SearchBox", searchQuery, sizeof(searchQuery));

			ImGui::SameLine(); 

			const char* sortModeOptions[] = {
				getString("gallery_modified"),
				getString("gallery_created"),
				getString("gallery_name_sort"),
				getString("gallery_combo")
			};
			ImGui::SetNextItemWidth(170.0f);
			ImGui::Combo("##SortModeBox", &sortMode, sortModeOptions, 4);

			ImGui::SameLine();

			const char* nameSortTargetOptions[] = {
				getString("gallery_title"),
				getString("gallery_file"),
				getString("gallery_artist"),
				getString("gallery_author")
			};
			ImGui::BeginDisabled(sortMode != 2);
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y + 4.0f));
			ImGui::SetNextItemWidth(150.0f);
			ImGui::Combo("##NameSortTargetBox", &nameSortTarget, nameSortTargetOptions, 4);
			ImGui::PopStyleVar();
			ImGui::EndDisabled();

			ImGui::SameLine();
			std::string sortDirectionLabel = std::string(sortAscending ? ICON_FA_SORT_AMOUNT_UP : ICON_FA_SORT_AMOUNT_DOWN) + " " + getString(sortAscending ? "gallery_sort_ascending" : "gallery_sort_descending");
			if (ImGui::Button(sortDirectionLabel.c_str(), ImVec2(sortDirectionWidth, 0.0f))) {
				sortAscending = !sortAscending;
			}
			UI::tooltip(sortAscending ? getString("gallery_sort_ascending") : getString("gallery_sort_descending"));

			ImGui::Spacing(); 

			if (isSearchPathsOpen) {
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
				ImGui::Indent();

				int pathToDeleteIndex = -1;

				for (int pIdx = 0; pIdx < (int)searchPaths.size(); ++pIdx) {
					ImGui::PushID(pIdx);
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
					
					if (ImGui::Button(ICON_FA_TRASH)) {
						pathToDeleteIndex = pIdx; 
					}
					
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ImGui::Text("%s", searchPaths[pIdx].c_str()); 
					ImGui::PopID();
				}

				if (pathToDeleteIndex != -1) {
					searchPaths.erase(searchPaths.begin() + pathToDeleteIndex);
					pathInputError.clear();
					saveGalleryData();
					scanSearchPaths();
				}

				ImGui::Dummy(ImVec2(0.0f, 4.0f));

				ImGui::Text("%s", getString("gallery_add_path"));
				ImGui::SetNextItemWidth(400.0f);
				bool addTypedPath = ImGui::InputTextWithHint("##NewPath", getString("gallery_path_input_hint"), newPathBuffer, sizeof(newPathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();
				ImGui::BeginDisabled(folderDialogInProgress);
				if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN) + " " + getString("gallery_browse_folder")).c_str()))
					startFolderDialog();
				ImGui::EndDisabled();
				UI::tooltip(getString("gallery_browse_folder"));
				if (folderDialogInProgress) {
					ImGui::SameLine();
					ImGui::TextDisabled("%s", getString("gallery_folder_dialog_open"));
				}

				if (addTypedPath)
				{
					if (addSearchPath(newPathBuffer))
						memset(newPathBuffer, 0, sizeof(newPathBuffer));
				}
				if (!pathInputError.empty()) {
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", pathInputError.c_str());
				}
				ImGui::Unindent();
				ImGui::Dummy(ImVec2(0.0f, 8.0f));
			}

			std::vector<std::shared_ptr<GalleryItem>> displayItems;
			std::string query = toLowerAscii(searchQuery);

			for (const auto& item : localItems) {
				if (query.empty()) {
					displayItems.push_back(item);
					continue;
				}
				
				std::string titleLower = toLowerAscii(item->title);
				std::string artistLower = toLowerAscii(item->artist);
				std::string filenameLower = toLowerAscii(item->filename);

				if (titleLower.find(query) != std::string::npos ||
					artistLower.find(query) != std::string::npos ||
					filenameLower.find(query) != std::string::npos) {
					displayItems.push_back(item);
				}
			}

			sortItems(displayItems);

			drawGrid(displayItems, "LocalGrid");
		}

		if (activeTab == 2) {
			std::vector<std::shared_ptr<GalleryItem>> favs;
			for (auto& it : recentItems) if (it->isFavorite) favs.push_back(it);
			for (auto& it : localItems) if (it->isFavorite) {
				if (std::find_if(favs.begin(), favs.end(), [&](auto& f){return f->filepath == it->filepath;}) == favs.end()) favs.push_back(it);
			}
			ImGui::Spacing(); ImGui::Text("%s", getString("gallery_favorites")); ImGui::Separator(); ImGui::Spacing();
			drawGrid(favs, "FavoritesGrid");
		}

		if (activeTab >= 3) {
			std::vector<std::shared_ptr<GalleryItem>> fItems;
			for (auto& it : recentItems) if (it->folder == filterFolder) fItems.push_back(it);
			for (auto& it : localItems) if (it->folder == filterFolder) {
				if (std::find_if(fItems.begin(), fItems.end(), [&](auto& f){return f->filepath == it->filepath;}) == fItems.end()) fItems.push_back(it);
			}
			
			std::string displayFilter = filterFolder;
			if (filterFolder == "Team Projects") displayFilter = getString("gallery_team_projects");
			else if (filterFolder == "Personal") displayFilter = getString("gallery_personal");
			
			ImGui::Spacing(); ImGui::Text("%s", displayFilter.c_str()); ImGui::Separator(); ImGui::Spacing();
			drawGrid(fItems, "CustomGrid");
		}
		ImGui::Dummy(ImVec2(0.0f, 24.0f));
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

	void ChartGalleryWindow::drawDeletePopup()
{
	if (ImGui::BeginPopupModal(getString("gallery_delete_file"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		std::filesystem::path p = IO::mbToWideStr(itemToDelete);
		std::string filename = "";
		std::string dirStr = "";
		std::string extStr = ""; 
		uintmax_t fileSize = 0;

		try {
			if (std::filesystem::exists(p)) {
				filename = IO::wideStringToMb(p.filename().wstring());
				dirStr = IO::wideStringToMb(p.parent_path().wstring());
				fileSize = std::filesystem::file_size(p) / 1024; 

				extStr = p.extension().string();
				if (!extStr.empty() && extStr[0] == '.') {
					extStr = extStr.substr(1); 
				}
				std::transform(extStr.begin(), extStr.end(), extStr.begin(), ::toupper);
			}
		} catch (...) {}

		ImGui::BeginGroup();
		if (appIcon) {
			ImGui::Image((void*)(intptr_t)appIcon->getID(), ImVec2(48.0f, 48.0f));
		} else {
			ImGui::SetWindowFontScale(3.0f);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), ICON_FA_FILE);
			ImGui::SetWindowFontScale(1.0f);
		}
		ImGui::EndGroup();

		ImGui::SameLine(0, 15.0f); 

		ImGui::BeginGroup();
		ImGui::Text("%s", getString("gallery_delete_confirm"));
		ImGui::Dummy(ImVec2(0.0f, 10.0f)); 

		ImGui::Text("%s", filename.c_str());

		if (!extStr.empty()) {
			ImGui::TextDisabled(getString("gallery_type_file"), extStr.c_str());
		} else {
			ImGui::TextDisabled("%s", getString("gallery_type_unknown"));
		}

		ImGui::TextDisabled(getString("gallery_size_kb"), fileSize);
		
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 350.0f);
		ImGui::TextDisabled(getString("gallery_original_location"), dirStr.c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();

		ImGui::Dummy(ImVec2(0.0f, 20.0f)); 

		float btnWidth = 100.0f;
		float spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (btnWidth * 2 + spacing) - 10.0f);

		if (ImGui::Button(getString("gallery_yes_y"), ImVec2(btnWidth, 0))) {
			try { std::filesystem::remove(p); removeDeletedItemFromLists(itemToDelete); } catch (...) {}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(getString("gallery_no_n"), ImVec2(btnWidth, 0))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}
}
