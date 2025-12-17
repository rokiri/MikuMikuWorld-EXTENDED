// Put httplib first otherwise the compiler will throw an error
#include <corecrt_math.h>
#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include <cpp-httplib/httplib.h>

#include "Application.h"
#include "ApplicationConfiguration.h"
#include "Constants.h"
#include "File.h"
#include "SUS.h"
#include "NativeScoreSerializer.h"
#include "UI.h"
#include "Utilities.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>

using nlohmann::json;

namespace MikuMikuWorld
{
	static MultiInputBinding* timelineModeBindings[] = {
		&config.input.timelineSelect,   &config.input.timelineTap,
		&config.input.timelineHold,     &config.input.timelineHoldMid,
		&config.input.timelineFlick,    &config.input.timelineCritical,
		&config.input.timelineFriction, &config.input.timelineGuide,
		&config.input.timelineDamage,   &config.input.timelineDummy,
		&config.input.timelineBpm,      &config.input.timelineTimeSignature,
		&config.input.timelineHiSpeed,
	};

	constexpr const char* toolbarFlickNames[] = { "none", "default",   "left",      "right",
		                                          "down", "down_left", "down_right" };

	constexpr const char* toolbarStepNames[] = { "normal", "hidden", "skip" };

	ScoreEditor::ScoreEditor()
	{
		renderer = std::make_unique<Renderer>();

		context.audio.initializeAudioEngine();
		context.audio.setMasterVolume(config.masterVolume);
		context.audio.setMusicVolume(config.bgmVolume);
		context.audio.setSoundEffectsVolume(config.seVolume);
		context.audio.loadSoundEffects();
		context.audio.setSoundEffectsProfileIndex(config.seProfileIndex);

		timeline.setDivision(config.division);
		timeline.setZoom(config.zoom);

		autoSavePath = Application::getAppDir() + "auto_save";
		autoSaveTimer.reset();

		std::thread fetchUpdateThread(
		    [this]
		    {
			    try
			    {
				    ScoreEditor::fetchUpdate();
			    }
			    catch (const std::exception& e)
			    {
				    std::cout << "Failed to fetch latest update: " << e.what() << std::endl;
			    }
		    });

		fetchUpdateThread.detach();
	}

	void ScoreEditor::fetchUpdate()
	{
		std::wstring updateFlagPath =
		    IO::mbToWideStr(Application::getAppDir() + "latest_version.txt");
		bool shouldFetchUpdate = true;
		std::string latestVersionString;
		if (IO::File::exists(updateFlagPath))
		{
			auto file = IO::File(updateFlagPath, IO::FileMode::Read);
			using fs_time_t = std::filesystem::file_time_type;
			auto lastWriteTime = std::filesystem::last_write_time(updateFlagPath);
			auto now = fs_time_t::clock::now();
			auto diff =
			    std::chrono::duration_cast<std::chrono::minutes>(now - lastWriteTime).count();
			std::cout << "Last update check: " << diff << " minutes ago" << std::endl;
			if (diff < 60)
			{
				std::ifstream file(updateFlagPath);
				std::getline(file, latestVersionString);
				file.close();
				std::cout << "Loading cached latest version" << std::endl;
				shouldFetchUpdate = false;
			}
		}
		if (shouldFetchUpdate)
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
				latestVersionString = tagName.substr(1);
			}

			auto file = IO::File(updateFlagPath, IO::FileMode::Write);
			file.write(latestVersionString);
			file.flush();
			file.close();
		}

		auto currentVersion = Utilities::splitString(Application::getAppVersion(), '.');
		auto latestVersion = Utilities::splitString(latestVersionString, '.');

		if (currentVersion.size() != latestVersion.size())
		{
			std::cout << "Assertion failed: number of version part don't match" << std::endl;
		}

		std::cout << "Current version: " << Application::getAppVersion() << std::endl;
		std::cout << "Latest version: " << latestVersionString << std::endl;

		for (int i = 0; i < currentVersion.size(); i++)
		{
			auto currentVersionPart = std::stoi(currentVersion[i]);
			auto latestVersionPart = std::stoi(latestVersion[i]);

			if (latestVersionPart > currentVersionPart)
			{
				std::cout << "Update available" << std::endl;
				updateAvailableDialog.latestVersion = latestVersionString;
				updateAvailableDialog.open = true;
				return;
			}
		}

		std::cout << "No update" << std::endl;
	}

	void ScoreEditor::writeSettings()
	{
		config.masterVolume = context.audio.getMasterVolume();
		config.bgmVolume = context.audio.getMusicVolume();
		config.seVolume = context.audio.getSoundEffectsVolume();

		config.division = timeline.getDivision();
		config.zoom = timeline.getZoom();
	}

	void ScoreEditor::uninitialize()
	{
		context.audio.uninitializeAudioEngine();
		timeline.background.dispose();
	}

	void ScoreEditor::update()
	{
		drawMenubar();
		drawToolbar();

		if (!ImGui::GetIO().WantCaptureKeyboard)
		{
			if (ImGui::IsAnyPressed(config.input.create))
				Application::windowState.resetting = true;
			if (ImGui::IsAnyPressed(config.input.open))
			{
				Application::windowState.resetting = true;
				Application::windowState.shouldPickScore = true;
			}

			if (ImGui::IsAnyPressed(config.input.openSettings))
				settingsWindow.open = true;
			if (ImGui::IsAnyPressed(config.input.openHelp))
				help();
			if (ImGui::IsAnyPressed(config.input.save))
				trySave(context.workingData.filename);
			if (ImGui::IsAnyPressed(config.input.saveAs))
				saveAs();
			if (ImGui::IsAnyPressed(config.input.exportScore))
				exportScore();
			if (ImGui::IsAnyPressed(config.input.togglePlayback))
				timeline.setPlaying(context, !timeline.isPlaying());
			if (ImGui::IsAnyPressed(config.input.stop))
				timeline.stop(context);
			if (ImGui::IsAnyPressed(config.input.previousTick, true))
				timeline.previousTick(context);
			if (ImGui::IsAnyPressed(config.input.nextTick, true))
				timeline.nextTick(context);
			if (ImGui::IsAnyPressed(config.input.selectAll))
				context.selectAll();
			if (ImGui::IsAnyPressed(config.input.deleteSelection))
				context.deleteSelection();
			if (ImGui::IsAnyPressed(config.input.cutSelection))
				context.cutSelection();
			if (ImGui::IsAnyPressed(config.input.copySelection))
				context.copySelection();
			if (ImGui::IsAnyPressed(config.input.paste))
				context.paste(false);
			if (ImGui::IsAnyPressed(config.input.flipPaste))
				context.paste(true);
			if (ImGui::IsAnyPressed(config.input.cancelPaste))
				context.cancelPaste();
			if (ImGui::IsAnyPressed(config.input.duplicate))
				context.duplicateSelection(false);
			if (ImGui::IsAnyPressed(config.input.flipDuplicate))
				context.duplicateSelection(true);
			if (ImGui::IsAnyPressed(config.input.flip))
				context.flipSelection();
			if (ImGui::IsAnyPressed(config.input.undo))
				context.undo();
			if (ImGui::IsAnyPressed(config.input.redo))
				context.redo();
			if (ImGui::IsAnyPressed(config.input.zoomOut, true))
				timeline.setZoom(timeline.getZoom() - 0.25f);
			if (ImGui::IsAnyPressed(config.input.zoomIn, true))
				timeline.setZoom(timeline.getZoom() + 0.25f);
			if (ImGui::IsAnyPressed(config.input.decreaseNoteSize, true))
				edit.noteWidth = std::clamp(edit.noteWidth - 1, MIN_NOTE_WIDTH, MAX_NOTE_WIDTH);
			if (ImGui::IsAnyPressed(config.input.increaseNoteSize, true))
				edit.noteWidth = std::clamp(edit.noteWidth + 1, MIN_NOTE_WIDTH, MAX_NOTE_WIDTH);
			if (ImGui::IsAnyPressed(config.input.shrinkDown))
				context.shrinkSelection(Direction::Down);
			if (ImGui::IsAnyPressed(config.input.shrinkUp))
				context.shrinkSelection(Direction::Up);
			if (ImGui::IsAnyPressed(config.input.compressSelection))
				context.compressSelection();
			if (ImGui::IsAnyPressed(config.input.connectHolds))
				context.connectHoldsInSelection();
			if (ImGui::IsAnyPressed(config.input.splitHold))
				context.splitHoldInSelection();
			if (ImGui::IsAnyPressed(config.input.lerpHiSpeeds))
				context.lerpHiSpeeds(timeline.getDivision(), EaseType::Linear);

			for (int i = 0; i < (int)TimelineMode::TimelineModeMax; ++i)
				if (ImGui::IsAnyPressed(*timelineModeBindings[i]))
					timeline.changeMode((TimelineMode)i, edit);
		}

		timeline.laneWidth = config.timelineWidth;
		timeline.notesHeight =
		    config.matchNotesSizeToTimeline ? config.timelineWidth : config.notesHeight;

		if (config.backgroundBrightness != timeline.background.getBrightness())
			timeline.background.setBrightness(config.backgroundBrightness);

		if (settingsWindow.isBackgroundChangePending)
		{
			static const std::string defaultBackgroundPath =
			    Application::getAppDir() + "res\\textures\\default.png";
			timeline.background.load(config.backgroundImage.empty() ? defaultBackgroundPath
			                                                        : config.backgroundImage);
			settingsWindow.isBackgroundChangePending = false;
		}

		if (config.seProfileIndex != context.audio.getSoundEffectsProfileIndex())
		{
			context.audio.stopSoundEffects(false);
			context.audio.setSoundEffectsProfileIndex(config.seProfileIndex);
		}

		if (propertiesWindow.isPendingLoadMusic)
		{
			loadMusic(propertiesWindow.pendingLoadMusicFilename);
			propertiesWindow.pendingLoadMusicFilename.clear();
			propertiesWindow.isPendingLoadMusic = false;
		}

		if (config.autoSaveEnabled && autoSaveTimer.elapsedMinutes() >= config.autoSaveInterval)
		{
			autoSave();
			autoSaveTimer.reset();
		}

		if (recentFileNotFoundDialog.update() == DialogResult::Yes)
		{
			if (isArrayIndexInBounds(recentFileNotFoundDialog.removeIndex, config.recentFiles))
				config.recentFiles.erase(config.recentFiles.begin() +
				                         recentFileNotFoundDialog.removeIndex);
		}

		settingsWindow.update();
		aboutDialog.update();
		updateAvailableDialog.update();
		serializeWindow.update(*this, context, timeline);

		ImGui::Begin(IMGUI_TITLE(ICON_FA_MUSIC, "notes_timeline"), NULL,
		             ImGuiWindowFlags_Static | ImGuiWindowFlags_NoScrollbar |
		                 ImGuiWindowFlags_NoScrollWithMouse);
		timeline.update(context, edit, renderer.get());
		ImGui::End();

		if (config.debugEnabled)
		{
			debugWindow.update(context, timeline);
		}

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_ALIGN_LEFT, "chart_properties"), NULL,
		                 ImGuiWindowFlags_Static))
		{
			propertiesWindow.update(context);
		}
		ImGui::End();

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_WRENCH, "note_properties"), NULL,
		                 ImGuiWindowFlags_Static))
		{
			notePropertiesWindow.update(context);
		}
		ImGui::End();

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_WRENCH, "options"), NULL, ImGuiWindowFlags_Static))
		{
			optionsWindow.update(context, edit, timeline.getMode());
		}
		ImGui::End();

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_DRAFTING_COMPASS, "presets"), NULL,
		                 ImGuiWindowFlags_Static))
		{
			presetsWindow.update(context, presetManager);
		}
		ImGui::End();

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_LAYER_GROUP, "layers"), NULL, ImGuiWindowFlags_Static))
		{
			layersWindow.update(context);
		}
		ImGui::End();

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_LOCATION_ARROW, "waypoints"), NULL,
		                 ImGuiWindowFlags_Static))
		{
			waypointsWindow.update(context);
		}
		ImGui::End();

#ifdef DEBUG
		if (showImGuiDemoWindow)
			ImGui::ShowDemoWindow(&showImGuiDemoWindow);
#endif
	}

	size_t ScoreEditor::updateRecentFilesList(const std::string& entry)
	{
		while (config.recentFiles.size() >= maxRecentFilesEntries)
			config.recentFiles.pop_back();

		// Remove the entry (if found) to the beginning of the vector
		auto it = std::find(config.recentFiles.begin(), config.recentFiles.end(), entry);
		if (it != config.recentFiles.end())
			config.recentFiles.erase(it);

		config.recentFiles.insert(config.recentFiles.begin(), entry);
		return config.recentFiles.size();
	}

	void ScoreEditor::create()
	{
		timeline.setPlaying(context, false);

		context.score = {};
		context.selectedLayer = 0;
		context.workingData = {};
		context.history.clear();
		context.scoreStats.reset();
		context.audio.disposeMusic();
		context.waveformL.clear();
		context.waveformR.clear();
		context.clearSelection();

		// New score; nothing to save
		context.upToDate = true;

		UI::setWindowTitle(windowUntitled);
	}

	void ScoreEditor::loadScore(std::string filename)
	{
		if (!IO::File::exists(filename))
			return;

		timeline.setPlaying(context, false);
		serializeWindow.deserialize(filename);
	}

	void ScoreEditor::loadMusic(std::string filename)
	{
		Result result = context.audio.loadMusic(filename);
		if (result.isOk() || filename.empty())
		{
			context.workingData.musicFilename = filename;
		}
		else
		{
			std::string errorMessage = IO::formatString(
			    "%s\n%s: %s\n%s: %s", getString("error_load_music_file"), getString("music_file"),
			    filename.c_str(), getString("error"), result.getMessage().c_str());

			IO::messageBox(APP_NAME, errorMessage, IO::MessageBoxButtons::Ok,
			               IO::MessageBoxIcon::Error);
		}

		context.waveformL.generateMipChainsFromSampleBuffer(context.audio.musicBuffer, 0);
		context.waveformR.generateMipChainsFromSampleBuffer(context.audio.musicBuffer, 1);
		timeline.setPlaying(context, false);
	}

	void ScoreEditor::open()
	{
		IO::FileDialog fileDialog{};
		fileDialog.parentWindowHandle = Application::windowState.windowHandle;
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
		try
		{
			context.score.metadata = context.workingData.toScoreMetadata();
			NativeScoreSerializer().serialize(context.score, filename);

			UI::setWindowTitle(IO::File::getFilename(filename));
			context.upToDate = true;
		}
		catch (const std::exception& err)
		{
			IO::messageBox(APP_NAME,
			               IO::formatString("%s\n%s: %s", getString("error_save_score_file"),
			                                getString("error"), err.what()),
			               IO::MessageBoxButtons::Ok, IO::MessageBoxIcon::Error);
			return false;
		}

		return true;
	}

	bool ScoreEditor::saveAs()
	{
		IO::FileDialog fileDialog{};
		fileDialog.title = "Save Chart";
		fileDialog.filters = { IO::mmwsFilter };
		fileDialog.defaultExtension = "ucmmws";
		fileDialog.parentWindowHandle = Application::windowState.windowHandle;
		fileDialog.inputFilename =
		    IO::File::getFilenameWithoutExtension(context.workingData.filename);

		if (fileDialog.saveFile() == IO::FileDialogResult::OK)
		{
			context.workingData.filename = fileDialog.outputFilename;
			bool saved = save(context.workingData.filename);
			if (saved)
				updateRecentFilesList(fileDialog.outputFilename);

			return saved;
		}

		return false;
	}

	void ScoreEditor::exportScore()
	{
		SerializeFormat format = static_cast<SerializeFormat>(config.defaultExportFormat);
		if (ScoreSerializeController::isValidFormat(format))
		{
			IO::FileDialog fileDialog{};
			fileDialog.title = "Export Chart";
			fileDialog.filters = { ScoreSerializeController::getFormatFilter(format) };
			fileDialog.defaultExtension =
			    ScoreSerializeController::getFormatDefaultExtension(format);
			fileDialog.parentWindowHandle = Application::windowState.windowHandle;

			if (fileDialog.saveFile() == IO::FileDialogResult::OK)
				serializeWindow.serialize(context, fileDialog.outputFilename);
		}
		else
		{
			serializeWindow.serialize(context);
		}
	}

	void ScoreEditor::drawMenubar()
	{
		ImGui::BeginMainMenuBar();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 2));

		if (ImGui::BeginMenu(getString("file")))
		{
			if (ImGui::MenuItem(getString("new"), ToShortcutString(config.input.create)))
				Application::windowState.resetting = true;

			if (ImGui::MenuItem(getString("open"), ToShortcutString(config.input.open)))
			{
				Application::windowState.resetting = true;
				Application::windowState.shouldPickScore = true;
			}

			if (ImGui::BeginMenu(getString("open_recent")))
			{
				for (size_t index = 0; index < config.recentFiles.size(); index++)
				{
					const std::string& entry = config.recentFiles[index];
					if (ImGui::MenuItem(entry.c_str()))
					{
						if (IO::File::exists(entry))
						{
							Application::windowState.resetting = true;
							Application::pendingLoadScoreFile = entry;
						}
						else
						{
							recentFileNotFoundDialog.removeFilename = entry;
							recentFileNotFoundDialog.removeIndex = index;
							recentFileNotFoundDialog.open = true;
						}
					}
				}

				ImGui::Separator();
				if (ImGui::MenuItem(getString("clear"), nullptr, false,
				                    !config.recentFiles.empty()))
					config.recentFiles.clear();

				ImGui::EndMenu();
			}

			ImGui::Separator();
			if (ImGui::MenuItem(getString("save"), ToShortcutString(config.input.save)))
				trySave(context.workingData.filename);

			if (ImGui::MenuItem(getString("save_as"), ToShortcutString(config.input.saveAs)))
				saveAs();

			if (ImGui::MenuItem(getString("export_score"),
			                    ToShortcutString(config.input.exportScore)))
				exportScore();

			ImGui::Separator();
			if (ImGui::MenuItem(getString("exit"),
			                    ToShortcutString(ImGuiKey_F4, ImGuiMod_Alt)))
				Application::windowState.closing = true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(getString("edit")))
		{
			if (ImGui::MenuItem(getString("undo"), ToShortcutString(config.input.undo), false,
			                    context.history.hasUndo()))
				context.undo();

			if (ImGui::MenuItem(getString("redo"), ToShortcutString(config.input.redo), false,
			                    context.history.hasRedo()))
				context.redo();

			ImGui::Separator();
			if (ImGui::MenuItem(getString("delete"), ToShortcutString(config.input.deleteSelection),
			                    false, context.hasSelection()))
				context.deleteSelection();

			if (ImGui::MenuItem(getString("cut"), ToShortcutString(config.input.cutSelection),
			                    false, context.hasSelection()))
				context.cutSelection();

			if (ImGui::MenuItem(getString("copy"), ToShortcutString(config.input.copySelection),
			                    false, context.hasSelection()))
				context.copySelection();

			if (ImGui::MenuItem(getString("paste"), ToShortcutString(config.input.paste)))
				context.paste(false);

			if (ImGui::MenuItem(getString("duplicate"), ToShortcutString(config.input.duplicate),
			                    false, context.hasSelection()))
				context.duplicateSelection(false);

			ImGui::Separator();
			if (ImGui::MenuItem(getString("select_all"), ToShortcutString(config.input.selectAll)))
				context.selectAll();

			ImGui::Separator();
			if (ImGui::MenuItem(getString("settings"), ToShortcutString(config.input.openSettings)))
				settingsWindow.open = true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(getString("view")))
		{
			ImGui::MenuItem(getString("show_step_outlines"), NULL, &timeline.drawHoldStepOutlines);
			ImGui::MenuItem(getString("cursor_auto_scroll"), NULL, &config.followCursorInPlayback);
			ImGui::MenuItem(getString("return_to_last_tick"), NULL,
			                &config.returnToLastSelectedTickOnPause);
			ImGui::MenuItem(getString("draw_waveform"), NULL, &config.drawWaveform);

			ImGui::EndMenu();
		}

		if (config.debugEnabled)
		{
			if (ImGui::BeginMenu(getString("debug")))
			{
#ifdef DEBUG
				ImGui::MenuItem("ImGui Demo Window", NULL, &showImGuiDemoWindow);
#endif

				if (ImGui::MenuItem("Auto Save"))
					autoSave();

				if (ImGui::MenuItem("Delete Old Auto Save (1)"))
					deleteOldAutoSave(1);

				if (ImGui::MenuItem("Delete Old Auto Save (Max)"))
					deleteOldAutoSave(config.autoSaveMaxCount);

				bool audioRunning = context.audio.isEngineStarted();
				if (ImGui::MenuItem(audioRunning ? "Stop Audio" : "Start Audio",
				                    audioRunning ? ICON_FA_VOLUME_UP : ICON_FA_VOLUME_MUTE))
				{
					if (audioRunning)
						context.audio.stopEngine();
					else
						context.audio.startEngine();
				}

				ImGui::EndMenu();
			}
		}

		if (ImGui::BeginMenu(getString("window")))
		{
			if (ImGui::MenuItem(getString("vsync"), NULL, &config.vsync))
				glfwSwapInterval(config.vsync);

			ImGui::MenuItem(getString("show_fps"), NULL, &config.showFPS);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(getString("help")))
		{
			if (ImGui::MenuItem(getString("help"), ToShortcutString(config.input.openHelp)))
				help();

			if (ImGui::MenuItem(getString("about")))
				aboutDialog.open = true;

			ImGui::EndMenu();
		}

		if (config.showFPS)
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

	void ScoreEditor::drawToolbar()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 toolbarSize{ viewport->WorkSize.x,
			                UI::toolbarBtnSize.y + ImGui::GetStyle().WindowPadding.y + 5 };

		// keep toolbar on top in main viewport
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(toolbarSize, ImGuiCond_Always);

		// toolbar style
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
		ImGui::PushStyleColor(ImGuiCol_Separator, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::Begin("##app_toolbar", NULL, ImGuiWindowFlags_Toolbar);

		if (UI::toolbarButton(ICON_FA_FILE, getString("new"),
		                      ToShortcutString(config.input.create)))
		{
			Application::windowState.resetting = true;
		}

		if (UI::toolbarButton(ICON_FA_FOLDER_OPEN, getString("open"),
		                      ToShortcutString(config.input.open)))
		{
			Application::windowState.resetting = true;
			Application::windowState.shouldPickScore = true;
		}

		if (UI::toolbarButton(ICON_FA_SAVE, getString("save"), ToShortcutString(config.input.save)))
			trySave(context.workingData.filename);

		if (UI::toolbarButton(ICON_FA_FILE_EXPORT, getString("export_score"),
		                      ToShortcutString(config.input.exportScore)))
			exportScore();

		UI::toolbarSeparator();

		if (UI::toolbarButton(ICON_FA_CUT, getString("cut"),
		                      ToShortcutString(config.input.cutSelection),
		                      context.selectedNotes.size() > 0))
			context.cutSelection();

		if (UI::toolbarButton(ICON_FA_COPY, getString("copy"),
		                      ToShortcutString(config.input.copySelection),
		                      context.selectedNotes.size() > 0))
			context.copySelection();

		if (UI::toolbarButton(ICON_FA_PASTE, getString("paste"),
		                      ToShortcutString(config.input.paste)))
			context.paste(false);

		if (UI::toolbarButton(ICON_FA_CLONE, getString("duplicate"),
		                      ToShortcutString(config.input.duplicate),
		                      context.selectedNotes.size() > 0))
			context.duplicateSelection(false);

		UI::toolbarSeparator();

		if (UI::toolbarButton(ICON_FA_UNDO, getString("undo"), ToShortcutString(config.input.undo),
		                      context.history.hasUndo()))
			context.undo();

		if (UI::toolbarButton(ICON_FA_REDO, getString("redo"), ToShortcutString(config.input.redo),
		                      context.history.hasRedo()))
			context.redo();

		UI::toolbarSeparator();

		for (int i = 0; i < arrayLength(timelineModes); ++i)
		{
			std::string img{ IO::concat("timeline", timelineModes[i], "_") };
			if (i == (int)TimelineMode::InsertFlick)
				img.append("_").append(toolbarFlickNames[(int)edit.flickType]);
			else if (i == (int)TimelineMode::InsertLongMid)
				img.append("_").append(toolbarStepNames[(int)edit.stepType]);
			else if (i == (int)TimelineMode::InsertGuide)
			{
				img.append("_").append(guideColors[(int)edit.colorType]);
				img.append("_").append(
				    std::string(fadeTypes[(int)edit.fadeType]).substr(5).c_str());
			}

			if (UI::toolbarImageButton(img.c_str(), getString(timelineModes[i]),
			                           ToShortcutString(*timelineModeBindings[i]), true,
			                           (int)timeline.getMode() == i))
				timeline.changeMode((TimelineMode)i, edit);
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::End();
	}

	void ScoreEditor::help()
	{
		ShellExecuteW(0, 0, L"https://github.com/crash5band/MikuMikuWorld/wiki", 0, 0, SW_SHOW);
	}

	void ScoreEditor::autoSave()
	{
		std::wstring wAutoSaveDir = IO::mbToWideStr(autoSavePath);

		// create auto save directory if none exists
		if (!std::filesystem::exists(wAutoSaveDir))
			std::filesystem::create_directory(wAutoSaveDir);

		context.score.metadata = context.workingData.toScoreMetadata();
		NativeScoreSerializer().serialize(context.score, autoSavePath + "\\mmw_auto_save_" +
		                                                     Utilities::getCurrentDateTime() +
		                                                     UC_MMWS_EXTENSION);

		// get mmws files
		int mmwsCount = 0;
		for (const auto& file : std::filesystem::directory_iterator(wAutoSaveDir))
		{
			std::string extension = file.path().extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
			mmwsCount += extension == UC_MMWS_EXTENSION;
		}

		// delete older files
		if (mmwsCount > config.autoSaveMaxCount)
			deleteOldAutoSave(mmwsCount - config.autoSaveMaxCount);
	}

	int ScoreEditor::deleteOldAutoSave(int count)
	{
		std::wstring wAutoSaveDir = IO::mbToWideStr(autoSavePath);
		if (!std::filesystem::exists(wAutoSaveDir))
			return 0;

		// get mmws files
		using entry = std::filesystem::directory_entry;
		std::vector<entry> deleteFiles;
		for (const auto& file : std::filesystem::directory_iterator(wAutoSaveDir))
		{
			std::string extension = file.path().extension().string();
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
}
