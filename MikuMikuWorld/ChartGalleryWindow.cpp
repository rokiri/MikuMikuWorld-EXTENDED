#include "ChartGalleryWindow.h"
#include "Application.h"
#include "File.h"
#include "NativeScoreSerializer.h"
#include "Audio/Sound.h"
#include "UI.h"
#include "IconsFontAwesome5.h"
#include "Rendering/Texture.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include "json.hpp"

namespace MikuMikuWorld
{
	void ChartGalleryWindow::loadGalleryData()
	{
		std::string path = Application::getAppDir() + "gallery.json";
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
				if (j.contains("searchPath")) {
					strncpy(localSearchPath, j["searchPath"].get<std::string>().c_str(), sizeof(localSearchPath));
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
		j["searchPath"] = std::string(localSearchPath);
		std::string path = Application::getAppDir() + "gallery.json";
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

		if (galleryStates.count(filepath)) {
			item->isFavorite = galleryStates[filepath].isFavorite;
			item->folder = galleryStates[filepath].folder;
		}

		try {
			Score tempScore = NativeScoreSerializer().deserialize(filepath);
			if (!tempScore.metadata.title.empty()) item->title = tempScore.metadata.title;
			if (!tempScore.metadata.artist.empty()) item->artist = tempScore.metadata.artist;
			if (!tempScore.metadata.author.empty()) item->author = tempScore.metadata.author;
			item->totalCombo = tempScore.notes.size();

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

	void ChartGalleryWindow::drawGrid(std::vector<std::shared_ptr<GalleryItem>>& itemsToDraw, const char* gridId)
	{
		if (itemsToDraw.empty()) {
			ImGui::TextDisabled("  No charts to display.");
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
			// 【追加】描画の直前に galleryStates から最新の状態を反映（同期処理）
			if (galleryStates.count(item->filepath)) {
				item->isFavorite = galleryStates[item->filepath].isFavorite;
				item->folder = galleryStates[item->filepath].folder;
			}

			// 【修正】gridId を ID に含めることで、RecentとLocalのメニューを分離する
			ImGui::PushID(gridId); 
			ImGui::PushID(item->filepath.c_str());

			ImGui::BeginGroup(); 
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight), IM_COL32(35, 35, 35, 255), 5.0f);

			ImVec2 imgPos = ImVec2(cursorPos.x + padding, cursorPos.y + padding);
			if (item->texture) {
				drawList->AddImageRounded((void*)(intptr_t)item->texture->getID(), imgPos, ImVec2(imgPos.x + imageSize, imgPos.y + imageSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
			} else if (defaultIcon) {
				drawList->AddImageRounded((void*)(intptr_t)defaultIcon->getID(), imgPos, ImVec2(imgPos.x + imageSize, imgPos.y + imageSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
			}

			// --- ハートアイコン背後に薄い丸角背景を追加 ---
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

			const char* labels[] = { "File", "Format", "Title", "Artist", "Author", "Time", "Combo" };
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

			// 中央揃えの計算（文字化けを防ぐため substr は使わずクリッピングで対応）
			ImVec2 textSize = ImGui::CalcTextSize(item->title.c_str());
			float textX = titleBarStart.x + (imageSize - textSize.x) * 0.5f;
			if (textX < titleBarStart.x + 4.0f) textX = titleBarStart.x + 4.0f; // 左端に寄りすぎないよう制限

			drawList->PushClipRect(titleBarStart, titleBarEnd, true);
			drawList->AddText(ImVec2(textX, titleBarStart.y + 4.0f), IM_COL32(255, 255, 255, 255), item->title.c_str());
			drawList->PopClipRect();
			
			std::string tagText = std::string(ICON_FA_TAG " ") + item->folder;
			ImVec2 tagPos = ImVec2(imgPos.x + imageSize + 12.0f, bottomY + 4.0f);

			float trashButtonStartX = cursorPos.x + cardWidth - padding - 24.0f;
			ImVec2 clipMin = tagPos;
			ImVec2 clipMax = ImVec2(trashButtonStartX - 8.0f, tagPos.y + 20.0f); // ゴミ箱の少し手前で切る

			drawList->PushClipRect(clipMin, clipMax, true);
			drawList->AddText(tagPos, IM_COL32(153, 153, 153, 255), tagText.c_str());
			drawList->PopClipRect();

			ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + cardWidth - padding - 24.0f, bottomY - 2.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button(ICON_FA_TRASH, ImVec2(24, 24))) { itemToDelete = item->filepath; openDeletePopup = true; }
			bool trashHovered = ImGui::IsItemHovered();
			ImGui::PopStyleColor();

			ImGui::EndGroup();

			// --- 変更ここから ---
			// ポップアップ全体の余白、項目間の余白、項目自体の内側の上下余白を少し広げます
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 6.0f));

			if (ImGui::BeginPopupContextItem("ChartContextMenu"))
			{
					if (ImGui::BeginMenu("Move to Folder"))
					{
					// Uncategorized
					if (ImGui::MenuItem("- (Uncategorized)", NULL, item->folder == "-")) {
						item->folder = "-";
						galleryStates[item->filepath].folder = "-";
						saveGalleryData();
					}
					
					// 項目間の境界線も少しゆとりを持たせたい場合は Dummy を挟むと綺麗です
					ImGui::Dummy(ImVec2(0.0f, 2.0f));
					ImGui::Separator();
					ImGui::Dummy(ImVec2(0.0f, 2.0f));

					// 固定フォルダとカスタムフォルダを重複なく表示
					std::vector<std::string> displayFolders = { "Team Projects", "Personal" };
					for (const auto& f : customFolders) {
						if (f != "Team Projects" && f != "Personal") displayFolders.push_back(f);
					}

					for (const auto& folderName : displayFolders) {
						if (ImGui::MenuItem(folderName.c_str(), NULL, item->folder == folderName)) {
							item->folder = folderName;
							galleryStates[item->filepath].folder = folderName;
							saveGalleryData();
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}

			// スタイル変更を解除して元に戻します（3つPushしたので3を指定）
			ImGui::PopStyleVar(3); 
			// --- 変更ここまで ---

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

		ImGui::PopID(); // filepath の ID
		ImGui::PopID(); // gridId の ID
		}
	}

	void ChartGalleryWindow::update(const std::vector<std::string>& recentFiles)
	{
		if (!open) return;
	
		if (!recentLoaded) {
			loadGalleryData();
			for (const std::string& filepath : recentFiles) if (IO::File::exists(filepath)) recentItems.push_back(loadItemInfo(filepath));
			std::string iconPath = Application::getAppDir() + "res\\textures\\gallery\\placeholder.png";
			if (IO::File::exists(iconPath)) defaultIcon = std::make_unique<Texture>(iconPath);
			std::string appIconPath = Application::getAppDir() + "res\\mmw_icon.png";
			if (IO::File::exists(appIconPath)) {
				appIcon = std::make_unique<Texture>(appIconPath);
			}
			recentLoaded = true;
		}
	
		// 起動時の自動スキャン（既存のまま）
		if (!hasAutoScanned && strlen(localSearchPath) > 0) {
			localItems.clear();
			try {
				std::wstring wp = IO::mbToWideStr(localSearchPath);
				if (std::filesystem::exists(wp)) {
					for (const auto& e : std::filesystem::recursive_directory_iterator(wp)) {
						if (e.is_regular_file()) {
							std::string ex = e.path().extension().string();
							if (ex == ".mmws" || ex == ".ccmmws" || ex == ".unchmmws") 
								localItems.push_back(loadItemInfo(IO::wideStringToMb(e.path().wstring())));
						}
					}
				}
			} catch (...) {}
			hasAutoScanned = true;
		}
	
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
	
		if (ImGui::Begin("GalleryScreen", &open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
		{
			if (openDeletePopup) { ImGui::OpenPopup("Delete File"); openDeletePopup = false; }
			float sharedSeparatorY = 0.0f;
			if (ImGui::BeginTable("GalleryLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
				ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 200.0f);
				ImGui::TableSetupColumn("Main", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
	
				ImGui::TableSetColumnIndex(0);
				if (ImGui::BeginChild("SidebarChild", ImVec2(0, 0), false)) {
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);

					// --- 変更ここから：上部ボタンの背景を透明にする ---
					// ボタンの通常時の背景色を完全に透明（アルファ値 0.0f）に設定します
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); 
					// ホバー時（マウスカーソルを乗せた時）の色を少し薄いグレーに設定します
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
					// クリック時（アクティブ時）の色を少し濃いグレーに設定します
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));

					// フォルダー作成ボタン
					if (ImGui::Button(ICON_FA_PLUS, ImVec2(30, 30))) {
						isCreatingNewFolder = true;
						memset(folderEditBuffer, 0, sizeof(folderEditBuffer));
					}
					ImGui::SameLine();
	
					// 削除ボタン
					if (ImGui::Button(ICON_FA_TRASH, ImVec2(30, 30))) { if (activeTab >= 5) deleteFolder(activeTab); }
					ImGui::SameLine();
					// 編集ボタン
					if (ImGui::Button(ICON_FA_PEN, ImVec2(30, 30))) {
						if (activeTab >= 5) {
							editingFolderIndex = activeTab;
							std::string currentName = customFolders[activeTab - 5];
							strncpy(folderEditBuffer, currentName.c_str(), 256);
						}
					}

					// 設定した色指定を解除して元に戻します（Pushした回数と同じ「3」を指定）
					ImGui::PopStyleColor(3);

					sharedSeparatorY = ImGui::GetCursorPosY(); // ★現在のY座標を記憶
					ImGui::Separator();
					ImGui::Dummy(ImVec2(0.0f, 4.0f));
	
					auto drawSelectable = [&](const char* label, int index) {
						bool selected = (activeTab == index);
						if (editingFolderIndex == index) {
							// --- 編集モード（ここはとりあえず元のまま） ---
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
							// --- クリスタ風のカスタム描画 ---
							ImVec2 pos = ImGui::GetCursorScreenPos();
							float width = ImGui::GetContentRegionAvail().x - 8.0f; // 右側に少し隙間を空ける幅
							float height = 30.0f; // ボタンの高さ

							// 透明なボタンを配置してクリック・ホバー判定だけを吸い取る
							ImGui::InvisibleButton(label, ImVec2(width, height));
							if (ImGui::IsItemClicked()) activeTab = index;
							
							bool hovered = ImGui::IsItemHovered();
							
							// ダブルクリックによる名前編集判定
							if (hovered && ImGui::IsMouseDoubleClicked(0) && index >= 5) {
								editingFolderIndex = index;
								std::string currentName = customFolders[index - 5];
								strncpy(folderEditBuffer, currentName.c_str(), 256);
							}

							ImDrawList* drawList = ImGui::GetWindowDrawList();

							// 1. 背景色の決定と描画
							ImU32 bgColor;
							if (selected) {
								bgColor = IM_COL32(95, 110, 135, 255); // 選択時：クリスタ風の青みがかった色
							} else if (hovered) {
								bgColor = IM_COL32(85, 85, 85, 255);   // ホバー時：少し明るいグレー
							} else {
								bgColor = IM_COL32(65, 65, 65, 255);   // 通常時：グレー
							}
							// 角丸(3.0f)の矩形を描画
							drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bgColor, 3.0f);

							// 2. テキストの中央揃え描画とクリッピング
							ImVec2 textSize = ImGui::CalcTextSize(label);
							float textY = pos.y + (height - textSize.y) * 0.5f; 

							// 右端の「＞」アイコンの少し手前までをクリップ範囲に設定
							float arrowIconStartX = pos.x + width - 24.0f; 
							ImVec2 textClipMin = ImVec2(pos.x + 12.0f, pos.y);
							ImVec2 textClipMax = ImVec2(arrowIconStartX - 8.0f, pos.y + height);

							drawList->PushClipRect(textClipMin, textClipMax, true);
							drawList->AddText(ImVec2(pos.x + 12.0f, textY), IM_COL32(230, 230, 230, 255), label);
							drawList->PopClipRect();

							// 3. 右端に「＞」アイコンを描画
							const char* arrowIcon = ICON_FA_CHEVRON_RIGHT;
							ImVec2 arrowSize = ImGui::CalcTextSize(arrowIcon);
							float arrowX = pos.x + width - arrowSize.x - 12.0f;
							float arrowY = pos.y + (height - arrowSize.y) * 0.5f;
							drawList->AddText(ImVec2(arrowX, arrowY), IM_COL32(180, 180, 180, 255), arrowIcon);

							// 次のアイテムとの間に隙間を空ける
							ImGui::Dummy(ImVec2(0.0f, 4.0f)); 
						}
					};
	
					// 固定フォルダーグループ（0〜4）
					drawSelectable("  All", 0);
					drawSelectable("  Recent", 1);
					drawSelectable("  Favorites", 2);
					drawSelectable("  Team Projects", 3);
					drawSelectable("  Personal", 4);
	
					ImGui::Separator(); // システムとユーザーを分ける
					ImGui::Dummy(ImVec2(0.0f, 4.0f));

					// カスタムフォルダー
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
					
					ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 50);
					if (ImGui::Button("Create New Chart", ImVec2(-8, 40))) open = false;
				}
				ImGui::EndChild();

				ImGui::TableSetColumnIndex(1);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 0.0f));

				if (ImGui::BeginChild("MainContentChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
					std::string filterFolder = "";
					if (activeTab == 3) filterFolder = "Team Projects";
					else if (activeTab == 4) filterFolder = "Personal";
					else if (activeTab >= 5 && (activeTab - 5) < (int)customFolders.size()) filterFolder = customFolders[activeTab - 5];

					if (activeTab == 0 || activeTab == 1) {
						std::vector<std::shared_ptr<GalleryItem>> recentToDraw = recentItems;
						bool hasMore = (activeTab == 0 && recentToDraw.size() > 4);
						if (hasMore) recentToDraw.resize(4);

						ImGui::SetCursorPosY(sharedSeparatorY - ImGui::GetTextLineHeight() - 6.0f);
						ImGui::Text("Recent Charts");

						// セパレータの位置はそのまま左側と完全に合わせる
						ImGui::SetCursorPosY(sharedSeparatorY);
						ImGui::Separator();

						ImGui::Dummy(ImVec2(0.0f, 10.0f)); // セパレータ下の余白

						drawGrid(recentToDraw, "RecentGrid");


						if (hasMore) {
							ImGui::SameLine(0, 16.0f);
							ImGui::BeginGroup();
							float moreWidth = 100.0f; // 幅をスリムに
							float moreHeight = 165.0f;
							ImVec2 pos = ImGui::GetCursorScreenPos();
							ImDrawList* dl = ImGui::GetWindowDrawList();
							dl->AddRectFilled(pos, ImVec2(pos.x + moreWidth, pos.y + moreHeight), IM_COL32(50, 50, 50, 255), 5.0f);
							if (ImGui::InvisibleButton("ShowMoreBtn", ImVec2(moreWidth, moreHeight))) activeTab = 1;
							if (ImGui::IsItemHovered()) dl->AddRectFilled(pos, ImVec2(pos.x + moreWidth, pos.y + moreHeight), IM_COL32(255, 255, 255, 20), 5.0f);
							
							const char* iconTxt = ICON_FA_ARROW_RIGHT;
							const char* moreTxt = "More";
							ImVec2 iconSize = ImGui::CalcTextSize(iconTxt);
							ImVec2 textSize = ImGui::CalcTextSize(moreTxt);
							float centerX = pos.x + (moreWidth / 2.0f);
							dl->AddText(ImVec2(centerX - iconSize.x / 2.0f, pos.y + (moreHeight / 2.0f) - 15), IM_COL32(200, 200, 200, 255), iconTxt);
							dl->AddText(ImVec2(centerX - textSize.x / 2.0f, pos.y + (moreHeight / 2.0f) + 5), IM_COL32(200, 200, 200, 255), moreTxt);
							ImGui::EndGroup();
						}
					}

					if (activeTab == 0) {
						ImGui::Dummy(ImVec2(0.0f, 20.0f));
						ImGui::Text("All Charts in PC"); ImGui::Separator(); ImGui::Spacing();
						ImGui::Text("Add chart folder path:");
						ImGui::SetNextItemWidth(400.0f);
						ImGui::InputText("##Path", localSearchPath, sizeof(localSearchPath));
						ImGui::SameLine();

						if (ImGui::Button("Start Scan"))
						{
							localItems.clear();
							saveGalleryData(); // パスを保存
							try {
								std::wstring wp = IO::mbToWideStr(localSearchPath);
								if (std::filesystem::exists(wp)) {
									for (const auto& e : std::filesystem::recursive_directory_iterator(wp)) {
										if (e.is_regular_file()) {
											std::string ex = e.path().extension().string();
											if (ex == ".mmws" || ex == ".ccmmws" || ex == ".unchmmws") localItems.push_back(loadItemInfo(IO::wideStringToMb(e.path().wstring())));
										}
									}
								}
							} catch (...) {}
						}
						ImGui::Spacing();
						drawGrid(localItems, "LocalGrid");
					}

					if (activeTab == 2) {
						std::vector<std::shared_ptr<GalleryItem>> favs;
						for (auto& it : recentItems) if (it->isFavorite) favs.push_back(it);
						for (auto& it : localItems) if (it->isFavorite) {
							if (std::find_if(favs.begin(), favs.end(), [&](auto& f){return f->filepath == it->filepath;}) == favs.end()) favs.push_back(it);
						}
						ImGui::Spacing(); ImGui::Text("Favorites"); ImGui::Separator(); ImGui::Spacing();
						drawGrid(favs, "FavoritesGrid");
					}

					if (activeTab >= 3) {
						std::vector<std::shared_ptr<GalleryItem>> fItems;
						for (auto& it : recentItems) if (it->folder == filterFolder) fItems.push_back(it);
						for (auto& it : localItems) if (it->folder == filterFolder) {
							if (std::find_if(fItems.begin(), fItems.end(), [&](auto& f){return f->filepath == it->filepath;}) == fItems.end()) fItems.push_back(it);
						}
						ImGui::Spacing(); ImGui::Text(filterFolder.c_str()); ImGui::Separator(); ImGui::Spacing();
						drawGrid(fItems, "CustomGrid");
					}
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::EndTable();
			}

			if (ImGui::BeginPopupModal("Delete File", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
				// 削除対象のファイルパスから情報を取得
				std::filesystem::path p = IO::mbToWideStr(itemToDelete);
				std::string filename = "";
				std::string dirStr = "";
				std::string extStr = ""; // --- 拡張子用の変数を追加 ---
				uintmax_t fileSize = 0;

				try {
					if (std::filesystem::exists(p)) {
						filename = IO::wideStringToMb(p.filename().wstring());
						dirStr = IO::wideStringToMb(p.parent_path().wstring());
						fileSize = std::filesystem::file_size(p) / 1024; // バイトをKBに変換

						// --- 追加ここから：拡張子を取得して大文字に変換 ---
						extStr = p.extension().string();
						if (!extStr.empty() && extStr[0] == '.') {
							extStr = extStr.substr(1); // 先頭のピリオドを取り除く
						}
						// 小文字を大文字に変換 (例: ccmmws -> CCMMWS)
						std::transform(extStr.begin(), extStr.end(), extStr.begin(), ::toupper);
						// --- 追加ここまで ---
					}
				} catch (...) {}

				// --- 左側：アイコンエリア ---
				ImGui::BeginGroup();
				if (appIcon) {
					// 読み込んだアプリアイコンを描画 (サイズは48x48としています)
					ImGui::Image((void*)(intptr_t)appIcon->getID(), ImVec2(48.0f, 48.0f));
				} else {
					// 万が一アイコンが読み込めなかった場合の予備
					ImGui::SetWindowFontScale(3.0f);
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), ICON_FA_FILE);
					ImGui::SetWindowFontScale(1.0f);
				}
				ImGui::EndGroup();

				ImGui::SameLine(0, 15.0f); // アイコンとテキストの間の隙間

				// --- 右側：テキストエリア ---
				ImGui::BeginGroup();
				ImGui::Text("Are you sure you want to permanently delete this file?");
				ImGui::Dummy(ImVec2(0.0f, 10.0f)); // 少し隙間を空ける

				// ファイル詳細情報
				ImGui::Text("%s", filename.c_str());

				// --- 変更ここから：固定文字から動的変数へ置き換え ---
				if (!extStr.empty()) {
					ImGui::TextDisabled("Type: %s File", extStr.c_str());
				} else {
					ImGui::TextDisabled("Type: Unknown File");
				}
				// --- 変更ここまで ---

				ImGui::TextDisabled("Size: %llu KB", fileSize);
				
				// パスが長い場合を考慮して折り返しを有効化
				ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 350.0f);
				ImGui::TextDisabled("Original Location: %s", dirStr.c_str());
				ImGui::PopTextWrapPos();

				ImGui::EndGroup();

				ImGui::Dummy(ImVec2(0.0f, 20.0f)); // ボタン群の上の隙間

				// --- 下部：ボタンエリア (右寄せ) ---
				float btnWidth = 100.0f;
				float spacing = ImGui::GetStyle().ItemSpacing.x;
				// 右端から「ボタン2つ分の幅＋ボタン間の隙間」を引いた位置へカーソルを移動
				ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (btnWidth * 2 + spacing) - 10.0f);

				if (ImGui::Button("Yes(Y)", ImVec2(btnWidth, 0))) {
					try { std::filesystem::remove(p); removeDeletedItemFromLists(itemToDelete); } catch (...) {}
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("No(N)", ImVec2(btnWidth, 0))) {
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}
}