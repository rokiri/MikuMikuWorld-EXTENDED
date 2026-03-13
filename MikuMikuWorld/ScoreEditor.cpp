// Put httplib first otherwise the compiler will throw an error
#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include <cpp-httplib/httplib.h>

#include "Application.h"
#include "ScoreEditor.h"
#include "Constants.h"
#include "File.h"
#include "SUS.h"
#include "NativeScoreSerializer.h"
#include "UI.h"
#include "Text.h"
#include "Utilities.h"
#include <filesystem>
#include <fstream>

using nlohmann::json;

namespace MikuMikuWorld
{
	ScoreEditor::ScoreEditor()
	{
		const auto& config = getConfig();
		audio.initializeAudioEngine();
		audio.setMasterVolume(config.masterVolume);
		audio.setMusicVolume(config.bgmVolume);
		audio.setSoundEffectsVolume(config.seVolume);
		audio.loadSoundEffects();
		audio.setSoundEffectsProfileIndex(config.seProfileIndex);
		//timeline.setDivision(config.division);
		//timeline.setZoom(config.zoom);

		autoSavePath = Application::getInstance().getConfigPath("auto_save");
		// autoSaveTimer.reset();
		create();
	}

	void ScoreEditor::fetchUpdate()
	{
		using namespace std::chrono;
		auto& config = getConfig();
		system_clock::time_point now = system_clock::now();
		int diff = duration_cast<minutes>(now - config.lastUpdateCheck).count();
		std::cout << "Last update check: " << diff << " minutes ago" << std::endl;
		if (diff > 60)
		{
			try
			{
				httplib::Client client("https://api.github.com");

				std::cout << "Fetching new update" << std::endl;
				auto res = client.Get("/repos/UntitledCharts/MikuMikuWorld4UC/releases/latest");
				if (!res)
				{
					std::cerr << "Failed to fetch latest update: client.Get failed" << std::endl;
					return;
				}
				std::cout << "Status: " << res->status << std::endl;
				if (res->status == 200)
				{
					auto parsed = nlohmann::json::parse(res->body);
					std::string tagName = parsed["tag_name"];
					config.latestFetchAppVersion = tagName.substr(1);
					config.lastUpdateCheck = now;
				}
			}
			catch (const std::exception& e)
			{
				std::cout << "Failed to fetch latest update: " << e.what() << std::endl;
			}
		}

		auto& instance = Application::getInstance();
		auto currentVersion = IO::split(instance.getAppVersion(), ".");
		auto latestVersion = IO::split(config.latestFetchAppVersion, ".");

		if (currentVersion.size() != latestVersion.size())
		{
			std::cout << "Assertion failed: number of version part don't match" << std::endl;
			return;
		}

		std::cout << "Current version: " << instance.getAppVersion() << std::endl;
		std::cout << "Latest version: " << config.latestFetchAppVersion << std::endl;

		for (int i = 0; i < currentVersion.size(); i++)
		{
			auto currentVersionPart = std::stoi(currentVersion[i]);
			auto latestVersionPart = std::stoi(latestVersion[i]);

			if (latestVersionPart > currentVersionPart)
			{
				std::cout << "Update available" << std::endl;
				std::string title = UI::modalTitle(Text::updateAvailable);
				std::string content = IO::formatString(localize(Text::updateAvailableDescription),
				                                       config.latestFetchAppVersion);
				DialogContent::Action yes = { localize(Text::yes).string,
					                          &ScoreEditor::openReleasePage };
				DialogContent::Action no = { localize(Text::no).string, nullptr };
				dialog.open(title, { content }, { yes, no });
				return;
			}
		}

		std::cout << "No update" << std::endl;
	}

	void ScoreEditor::loadPresets(const FilePath& path) { presetManager.loadPresets(path); }
	void ScoreEditor::savePresets(const FilePath& path) { presetManager.savePresets(path); }

	void ScoreEditor::writeSettings()
	{
		auto& config = getConfig();
		config.masterVolume = audio.getMasterVolume();
		config.bgmVolume = audio.getMusicVolume();
		config.seVolume = audio.getSoundEffectsVolume();
		//config.division = timeline.getDivision();
		//config.zoom = timeline.getZoom();
	}

	void ScoreEditor::uninitialize()
	{
		audio.uninitializeAudioEngine();
		//timeline.background.dispose();
	}

	void ScoreEditor::update()
	{
		drawMenubar();
		auto& config = getConfig();
		auto timelineIt = timelines.find(currTimelineId);
		currTimeline = timelineIt != timelines.end() ? &timelineIt->second : nullptr;
		currContext = currTimeline ? &currTimeline->context : nullptr;
		if (currTimeline)
		{
			config.division = currTimeline->getQuarterDivision();
			config.zoom = currTimeline->getZoom().y;
		}
		toolbar.update(state, currContext, edit, pasteData);

		handleEvents();

		//if (config.backgroundBrightness != timeline.background.getBrightness())
		//	timeline.background.setBrightness(config.backgroundBrightness);

		//if (settingsWindow.isBackgroundChangePending)
		//{
		//	static const std::string defaultBackgroundPath =
		//	    Application::getAppDir() + "res\\textures\\default.png";
		//	timeline.background.load(config.backgroundImage.empty() ? defaultBackgroundPath
		//	                                                        : config.backgroundImage);
		//	settingsWindow.isBackgroundChangePending = false;
		// }

		if (config.seProfileIndex != audio.getSoundEffectsProfileIndex())
		{
			// audio.stopSoundEffects(false);
			audio.setSoundEffectsProfileIndex(config.seProfileIndex);
		}

		// if (config.autoSaveEnabled && autoSaveTimer.elapsedMinutes() >= config.autoSaveInterval)
		//{
		//	autoSave();
		//	autoSaveTimer.reset();
		//}

		settingsWindow.update();
		dialog.update();
		//serializeWindow.update(*this, context, timeline);
		if (getConfig().debugEnabled)
			debugWindow.update(audio);

		constexpr auto propWindowFlags =
		    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;
		auto updateWindowNoContext = []()
		{ ImGui::TextDisabled(localize(Text::timelineNoContextSelected)); };
		if (ImGui::Begin(ScorePropertiesWindow::getWindowName(), NULL, propWindowFlags))
		{
			if (currContext)
				propertiesWindow.update(*currTimeline, *currContext, audio);
			else
				updateWindowNoContext();
		}
		ImGui::End();

		if (ImGui::Begin(ScoreNotePropertiesWindow::getWindowName(), NULL, propWindowFlags))
		{
			if (currContext)
				notePropertiesWindow.update(*currContext);
			else
				updateWindowNoContext();
		}
		ImGui::End();

		if (ImGui::Begin(ScoreOptionsWindow::getWindowName(), NULL, propWindowFlags))
		{
			if (currContext)
				optionsWindow.update(*currContext, edit);
			else
				updateWindowNoContext();
		}
		ImGui::End();

		if (ImGui::Begin(PresetsWindow::getWindowName(), NULL, propWindowFlags))
		{
			if (currContext)
				presetsWindow.update(presetManager, *currContext, pasteData);
			else
				updateWindowNoContext();
		}
		ImGui::End();

		if (ImGui::Begin(LayersWindow::getWindowName(), NULL, propWindowFlags))
		{
			if (currContext)
				layersWindow.update(*currContext, dialog);
			else
				updateWindowNoContext();
		}
		ImGui::End();

		if (ImGui::Begin(WaypointsWindow::getWindowName(), NULL, propWindowFlags))
		{
			if (currTimeline)
				waypointsWindow.update(*currTimeline);
			else
				updateWindowNoContext();
		}
		ImGui::End();

#ifdef DEBUG
		if (showImGuiDemoWindow)
			ImGui::ShowDemoWindow(&showImGuiDemoWindow);
#endif
	}

	void ScoreEditor::handleEvents()
	{
		auto& input = getConfig().input;
		ImGuiInputFlags defaultFlags = ImGuiInputFlags_RouteGlobal;
		if (!ImGui::GetIO().WantCaptureKeyboard)
		{
			if (ImGui::AnyShortcut(input.create, defaultFlags))
				state.wantCreateScore = true;
			if (ImGui::AnyShortcut(input.open, defaultFlags))
				state.wantOpenScore = true;
			if (ImGui::AnyShortcut(input.save, defaultFlags))
				state.wantSaveScore = true;
			if (ImGui::AnyShortcut(input.saveAs, defaultFlags))
				state.wantSaveAsScore = true;
			if (ImGui::AnyShortcut(input.exportScore, defaultFlags))
				state.wantExportScore = true;
			if (ImGui::AnyShortcut(input.openSettings, defaultFlags))
				settingsWindow.open();
			if (ImGui::AnyShortcut(input.openHelp, defaultFlags))
				openHelp();
			for (auto&& mode : EnumRange(InsertMode::InsertModeMax))
				if (ImGui::AnyShortcut(input.*insertModeBindings[int(mode)], defaultFlags))
					edit.changeInsertMode(mode);

			if (currTimeline && currContext)
			{
				float minNoteWidth = currContext->minNoteWidth(),
				      maxNoteWidth = currContext->maxNoteWidth();
				if (ImGui::AnyShortcut(input.togglePlayback, defaultFlags))
					currTimeline->setPlaying(!currTimeline->isPlaying());
				if (ImGui::AnyShortcut(input.stop, defaultFlags))
					currTimeline->stop();
				if (ImGui::AnyShortcut(input.previousTick, defaultFlags | ImGuiInputFlags_Repeat))
					currTimeline->jumpToPrevDivision();
				if (ImGui::AnyShortcut(input.nextTick, defaultFlags | ImGuiInputFlags_Repeat))
					currTimeline->jumpToNextDivision();
				if (ImGui::AnyShortcut(input.zoomOut, defaultFlags | ImGuiInputFlags_Repeat))
					currTimeline->setZoomY(currTimeline->getZoom().y /
					                       ScoreEditorTimeline::ZOOM_Y_FACTOR);
				if (ImGui::AnyShortcut(input.zoomIn, defaultFlags | ImGuiInputFlags_Repeat))
					currTimeline->setZoomY(currTimeline->getZoom().y *
					                       ScoreEditorTimeline::ZOOM_Y_FACTOR);
				if (ImGui::AnyShortcut(input.decreaseNoteSize,
				                       defaultFlags | ImGuiInputFlags_Repeat))
					edit.noteWidth = std::clamp(edit.noteWidth - 1, minNoteWidth, maxNoteWidth);
				if (ImGui::AnyShortcut(input.increaseNoteSize,
				                       defaultFlags | ImGuiInputFlags_Repeat))
					edit.noteWidth = std::clamp(edit.noteWidth + 1, minNoteWidth, maxNoteWidth);
				if (ImGui::AnyShortcut(input.undo, defaultFlags) && currContext->history.hasUndo())
					currContext->undo();
				if (ImGui::AnyShortcut(input.redo, defaultFlags) && currContext->history.hasRedo())
					currContext->redo();
				if (ImGui::AnyShortcut(input.selectAll, defaultFlags))
					currContext->selectAll();
				if (ImGui::AnyShortcut(input.deleteSelection, defaultFlags))
					currContext->deleteSelection();
				if (ImGui::AnyShortcut(input.cutSelection, defaultFlags))
					currContext->cutSelection();
				if (ImGui::AnyShortcut(input.copySelection, defaultFlags))
					currContext->copySelection();
				if (ImGui::AnyShortcut(input.paste, defaultFlags))
					pasteData.startPaste();
				if (ImGui::AnyShortcut(input.flipPaste, defaultFlags))
				{
					pasteData.startPaste();
					pasteData.flip();
				}
				if (ImGui::AnyShortcut(input.cancelPaste, defaultFlags))
					pasteData.cancelPaste();
				if (currContext->hasAnySelected() &&
				    ImGui::AnyShortcut(input.duplicate, defaultFlags))
				{
					currContext->copySelection();
					pasteData.startPaste();
				}
				if (currContext->hasAnySelected() &&
				    ImGui::AnyShortcut(input.flipDuplicate, defaultFlags))
				{
					currContext->copySelection();
					pasteData.startPaste();
					pasteData.flip();
				}
				if (ImGui::AnyShortcut(input.flip))
					currContext->flipSelection();
				if (ImGui::IsAnyPressed(input.shrinkDown))
					currContext->shrinkSelection(1);
				if (ImGui::IsAnyPressed(input.shrinkUp))
					currContext->shrinkSelection(-1);
				if (ImGui::IsAnyPressed(input.compressSelection))
					currContext->compressSelection();
				if (ImGui::IsAnyPressed(input.connectHolds))
					currContext->connectHoldsInSelection();
				if (ImGui::IsAnyPressed(input.splitHold))
					currContext->splitHoldInSelection();
				if (ImGui::IsAnyPressed(input.lerpHiSpeeds))
					currContext->lerpHiSpeeds(currTimeline->getQuarterDivision(), EaseType::Linear);
			}
		}

		if (state.wantCreateScore)
		{
			create();
			state.wantCreateScore = false;
		}
		if (state.wantSaveScore)
		{
			if (currContext)
				save(*currContext);
			state.wantSaveScore = false;
		}
		if (state.wantSaveAsScore)
		{
			if (currContext)
				saveAs(*currContext);
			state.wantSaveAsScore = false;
		}
	}

	size_t ScoreEditor::updateRecentFilesList(const std::string& entry)
	{
		auto& files = getConfig().recentFiles;
		while (files.size() >= maxRecentFilesEntries)
			files.pop_back();

		// Remove the entry (if found) to the beginning of the vector
		auto it = std::find(files.begin(), files.end(), entry);
		if (it != files.end())
			files.erase(it);

		files.insert(files.begin(), entry);
		return files.size();
	}

	void ScoreEditor::create()
	{
		//timeline.setPlaying(context, false);

		//context.score = {};
		//context.selectedLayer = 0;
		//context.workingData = {};
		//context.history.clear();
		//context.scoreStats.reset();
		//context.audio.disposeMusic();
		//context.waveformL.clear();
		//context.waveformR.clear();
		//context.clearSelection();

		//// New score; nothing to save
		//context.upToDate = true;

		//UI::setWindowTitle(windowUntitled);
	}

	void ScoreEditor::loadScore(std::string filename)
	{
		//if (!IO::File::exists(filename))
		//	return;

		//timeline.setPlaying(context, false);
		//serializeWindow.deserialize(filename);
	}

	void ScoreEditor::loadMusic(std::string filename)
	{
		//Result result = context.audio.loadMusic(filename);
		//if (result.isOk() || filename.empty())
		//{
		//	context.workingData.musicFilename = filename;
		//}
		//else
		//{
		//	std::string errorMessage = IO::formatString(
		//	    "%s\n%s: %s\n%s: %s", getString("error_load_music_file"), getString("music_file"),
		//	    filename.c_str(), getString("error"), result.getMessage().c_str());

		//	IO::messageBox(APP_NAME, errorMessage, IO::MessageBoxButtons::Ok,
		//	               IO::MessageBoxIcon::Error);
		//}

		//context.waveformL.generateMipChainsFromSampleBuffer(context.audio.musicBuffer, 0);
		//context.waveformR.generateMipChainsFromSampleBuffer(context.audio.musicBuffer, 1);
		//timeline.setPlaying(context, false);
	}

	void ScoreEditor::open()
	{
		IO::FileDialog fileDialog{};
		fileDialog.parentWindowHandle = Application::getAppWindowHandle();
		fileDialog.title = "Open Score File";
		fileDialog.filters = { IO::combineFilters("All Supported Files",
			                                      { IO::mmwsFilter, IO::susFilter, IO::uscFilter,
			                                        IO::lvlDatFilter }),
			                   IO::mmwsFilter,
			                   IO::susFilter,
			                   IO::uscFilter,
			                   IO::lvlDatFilter,
			                   IO::allFilter };

		if (fileDialog.openFile() == IO::FileDialogResult::OK)
			loadScore(fileDialog.outputFilename);
	}

	bool ScoreEditor::trySave(std::string filename)
	{
		if (filename.empty())
			return saveAs();
		else
			return save(filename);

		return false;
	}

	bool ScoreEditor::save(std::string filename)
	{
		//try
		//{
		//	context.score.metadata = context.workingData.toScoreMetadata();
		//	NativeScoreSerializer().serialize(context.score, filename);

		//	UI::setWindowTitle(IO::File::getFilename(filename));
		//	context.upToDate = true;
		//}
		//catch (const std::exception& err)
		//{
		//	IO::messageBox(APP_NAME,
		//	               IO::formatString("%s\n%s: %s", getString("error_save_score_file"),
		//	                                getString("error"), err.what()),
		//	               IO::MessageBoxButtons::Ok, IO::MessageBoxIcon::Error);
		//	return false;
		//}

		return true;
	}

	bool ScoreEditor::saveAs()
	{
		//IO::FileDialog fileDialog{};
		//fileDialog.title = "Save Chart";
		//fileDialog.filters = { IO::mmwsFilter };
		//fileDialog.defaultExtension = "ucmmws";
		//fileDialog.parentWindowHandle = Application::getAppWindowHandle();
		//fileDialog.inputFilename =
		//    IO::File::getFilenameWithoutExtension(context.workingData.filename);

		//if (fileDialog.saveFile() == IO::FileDialogResult::OK)
		//{
		//	context.workingData.filename = fileDialog.outputFilename;
		//	bool saved = save(context.workingData.filename);
		//	if (saved)
		//		updateRecentFilesList(fileDialog.outputFilename);

		//	return saved;
		//}

		return false;
	}

	void ScoreEditor::exportScore()
	{
		//SerializeFormat format = static_cast<SerializeFormat>(config.defaultExportFormat);
		//if (ScoreSerializeController::isValidFormat(format))
		//{
		//	IO::FileDialog fileDialog{};
		//	fileDialog.title = "Export Chart";
		//	fileDialog.filters = { ScoreSerializeController::getFormatFilter(format) };
		//	fileDialog.defaultExtension =
		//	    ScoreSerializeController::getFormatDefaultExtension(format);
		//	fileDialog.parentWindowHandle = Application::getAppWindowHandle();

		//	if (fileDialog.saveFile() == IO::FileDialogResult::OK)
		//		serializeWindow.serialize(context, fileDialog.outputFilename);
		//}
		//else
		//{
		//	serializeWindow.serialize(context);
		//}
	}

	void ScoreEditor::drawMenubar()
	{
		ImGui::BeginMainMenuBar();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UI::scale({ 10, 2 }));

		auto& config = getConfig();
		auto& input = config.input;
		if (ImGui::BeginMenu(localize(Text::file)))
		{
			if (ImGui::MenuItem(localize(Text::newFile), ToShortcutString(input.create)))
				state.wantCreateScore = true;

			if (ImGui::MenuItem(localize(Text::openFile), ToShortcutString(input.open)))
				state.wantOpenScore = true;

			if (ImGui::BeginMenu(localize(Text::openRecent)))
			{
				for (size_t index = 0; index < config.recentFiles.size(); index++)
				{
					const std::string& entry = config.recentFiles[index];
					// avoid ID collision
					std::string label = IO::formatString("%s##recent_items_%zd", entry, index);
					if (ImGui::MenuItem(label.c_str()))
					{
						FilePath path = IO::stringToPath(entry);
						if (IO::File::exists(path))
						{
							appendOpenFile(path);
						}
						else
						{
							std::string title = UI::modalTitle(Text::fileNotFound);
							std::string content = IO::formatString(
							    "%s \"%s\" %s.\n%s", localize(Text::fileNotFoundMsg1), entry,
							    localize(Text::fileNotFoundMsg2),
							    localize(Text::removeRecentFileNotFound));
							auto onYes = [index = index]()
							{
								auto& config = getConfig();
								if (!isArrayIndexInBounds(index, config.recentFiles))
									return;
								config.recentFiles.erase(config.recentFiles.begin() + index);
							};
							DialogContent::Action yes = { localize(Text::yes).string, onYes };
							DialogContent::Action no = { localize(Text::no).string, nullptr };
							dialog.open(title, { content }, { yes, no });
						}
					}
				}

				ImGui::Separator();
				bool hasRecentFiles = config.recentFiles.size();
				if (ImGui::MenuItem(localize(Text::clear), nullptr, false, hasRecentFiles))
					config.recentFiles.clear();

				ImGui::EndMenu();
			}

			ImGui::Separator();
			if (ImGui::MenuItem(localize(Text::save), ToShortcutString(input.save), false,
			                    currContext))
				save(*currContext);

			if (ImGui::MenuItem(localize(Text::saveAs), ToShortcutString(input.saveAs), false,
			                    currContext))
				saveAs(*currContext);

			if (ImGui::MenuItem(localize(Text::exportScore), ToShortcutString(input.exportScore),
			                    false, currContext))
				exportScore(*currContext);

			ImGui::Separator();
			if (ImGui::MenuItem(localize(Text::exit), ToShortcutString(ImGuiKey_F4, ImGuiMod_Alt)))
				Application::getInstance().getWindowState().closing = true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(localize(Text::edit)))
		{
			if (ImGui::MenuItem(localize(Text::undo), ToShortcutString(input.undo), false,
			                    currContext && currContext->history.hasUndo()))
				currContext->undo();

			if (ImGui::MenuItem(localize(Text::redo), ToShortcutString(input.redo), false,
			                    currContext && currContext->history.hasRedo()))
				currContext->redo();

			ImGui::Separator();
			if (ImGui::MenuItem(localize(Text::del), ToShortcutString(input.deleteSelection), false,
			                    currContext && currContext->hasAnySelected()))
				currContext->deleteSelection();

			if (ImGui::MenuItem(localize(Text::cut), ToShortcutString(input.cutSelection), false,
			                    currContext && currContext->hasAnySelected()))
				currContext->cutSelection();

			if (ImGui::MenuItem(localize(Text::copy), ToShortcutString(input.copySelection), false,
			                    currContext && currContext->hasAnySelected()))
				currContext->copySelection();

			if (ImGui::MenuItem(localize(Text::paste), ToShortcutString(input.paste)))
				pasteData.startPaste();

			if (ImGui::MenuItem(localize(Text::duplicate), ToShortcutString(input.duplicate), false,
			                    currContext && currContext->hasAnySelected()))
			{
				currContext->copySelection();
				pasteData.startPaste();
			}

			if (ImGui::MenuItem(localize(Text::flipDuplicate),
			                    ToShortcutString(input.flipDuplicate), false,
			                    currContext && currContext->hasAnySelected()))
			{
				currContext->copySelection();
				pasteData.startPaste();
				pasteData.flip();
			}

			if (ImGui::MenuItem(localize(Text::flip), ToShortcutString(input.flip), false,
			                    currContext && currContext->hasAnySelected()))
				currContext->flipSelection();

			ImGui::Separator();
			if (ImGui::MenuItem(localize(Text::selectAll), ToShortcutString(input.selectAll)))
				currContext->selectAll();

			ImGui::Separator();
			if (ImGui::MenuItem(localize(Text::settings), ToShortcutString(input.openSettings)))
				settingsWindow.open();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(localize(Text::view)))
		{
			ImGui::MenuItem(localize(Text::showStepOutlines), NULL, &edit.drawHoldStepOutlines);
			ImGui::MenuItem(localize(Text::cursorAutoScroll), NULL, &config.followCursorInPlayback);
			ImGui::MenuItem(localize(Text::returnToLastTick), NULL,
			                &config.returnToLastSelectedTickOnPause);
			ImGui::MenuItem(localize(Text::drawWaveform), NULL, &config.drawWaveform);

			ImGui::EndMenu();
		}

		if (getConfig().debugEnabled)
		{
			if (ImGui::BeginMenu(localize(Text::debug)))
			{
#ifdef DEBUG
				ImGui::MenuItem("ImGui Demo Window", NULL, &showImGuiDemoWindow);
#endif

				// if (ImGui::MenuItem("Auto Save"))
				//	autoSave();

				// if (ImGui::MenuItem("Delete Old Auto Save (1)"))
				//	deleteOldAutoSave(1);

				// if (ImGui::MenuItem("Delete Old Auto Save (Max)"))
				//	deleteOldAutoSave(config.autoSaveMaxCount);

				bool audioRunning = audio.isEngineStarted();
				if (ImGui::MenuItem(audioRunning ? "Stop Audio" : "Start Audio",
				                    audioRunning ? ICON_FA_VOLUME_UP : ICON_FA_VOLUME_MUTE))
				{
					if (audioRunning)
						audio.stopEngine();
					else
						audio.startEngine();
				}

				ImGui::EndMenu();
			}
		}

		if (ImGui::BeginMenu(localize(Text::window)))
		{
			if (ImGui::MenuItem(localize(Text::vsync), NULL, &getConfig().vsync))
				glfwSwapInterval(getConfig().vsync);

			ImGui::MenuItem(localize(Text::fpsShow), NULL, &getConfig().showFPS);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(localize(Text::help)))
		{
			if (ImGui::MenuItem(localize(Text::help), ToShortcutString(input.openHelp)))
				openHelp();

			if (ImGui::MenuItem(localize(Text::about)))
			{
				std::string title = UI::modalTitle(Text::about);
				const char* desc =
				    APP_NAME "\nThis application is based on MikuMikuWorld for Chart Cyanvas\n"
				             "Copyright (c) 2023 Nanashi. (@sevenc-nanashi)\n\n"
				             "Which was based on MikuMikuWorld.\n"
				             "Copyright (C) 2022 Crash5b\n\n";
				std::string appVersion =
				    IO::formatString("Version %s", Application::getInstance().getAppVersion());
				std::string translator =
				    IO::formatString(localize(Text::translatedBy), localize(Text::translator));
				dialog.open(title, { desc, "", appVersion, translator }, { { "OK", nullptr } });
			}
			ImGui::EndMenu();
		}

		if (getConfig().showFPS)
		{
			std::string fps = IO::formatString("%.3fms (%.1fFPS)", ImGui::GetIO().DeltaTime * 1000,
			                                   ImGui::GetIO().Framerate);
			ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(fps.c_str()).x -
			                     ImGui::GetStyle().WindowPadding.x);
			ImGui::Text(fps.c_str());
		}

		ImGui::PopStyleVar();
		ImGui::EndMainMenuBar();
	}

	void ScoreEditor::autoSave()
	{
		// create auto save directory if none exists
		if (!std::filesystem::exists(autoSavePath))
			std::filesystem::create_directory(autoSavePath);

		//context.score.metadata = context.workingData.toScoreMetadata();
		//NativeScoreSerializer().serialize(context.score, autoSavePath + "\\mmw_auto_save_" +
		//                                                     Utilities::getCurrentDateTime() +
		//                                                     UC_MMWS_EXTENSION);

		//// get mmws files
		//int mmwsCount = 0;
		//for (const auto& file : std::filesystem::directory_iterator(wAutoSaveDir))
		//{
		//	std::string extension = file.path().extension().string();
		//	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		//	mmwsCount += extension == UC_MMWS_EXTENSION;
		//}

		//// delete older files
		//if (mmwsCount > config.autoSaveMaxCount)
		//	deleteOldAutoSave(mmwsCount - config.autoSaveMaxCount);
	}

	int ScoreEditor::deleteOldAutoSave(int count)
	{
		if (!std::filesystem::exists(autoSavePath))
			return 0;

		// get mmws files
		using entry = std::filesystem::directory_entry;
		std::vector<entry> deleteFiles;
		for (const auto& file : std::filesystem::directory_iterator(autoSavePath))
		{
			std::string extension = IO::toString(file.path().extension());
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
			if (extension == MMWS_EXTENSION)
				deleteFiles.push_back(file);
		}

		// sort files by modification date
		std::sort(deleteFiles.begin(), deleteFiles.end(), [](const entry& f1, const entry& f2)
		          { return f1.last_write_time() < f2.last_write_time(); });

		int deleteCount = 0;
		int remainingCount = count;
		while (remainingCount && deleteFiles.size())
		{
			std::filesystem::remove(deleteFiles.begin()->path());
			deleteFiles.erase(deleteFiles.begin());

			--remainingCount;
			++deleteCount;
		}

		return deleteCount;
	}

	void ScoreEditor::appendOpenFile(const FilePath& filepath)
	{
		state.pendingOpenFiles.push_back(filepath);
	}

	void ScoreEditor::openHelp()
	{
		ImGui::GetPlatformIO().Platform_OpenInShellFn(
		    GImGui, "https://github.com/crash5band/MikuMikuWorld/wiki");
	}

	void ScoreEditor::openReleasePage()
	{
		ImGui::GetPlatformIO().Platform_OpenInShellFn(
		    GImGui, "https://github.com/UntitledCharts/MikuMikuWorld4UC/releases");
	}
}
