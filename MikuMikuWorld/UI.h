#pragma once
#include "IO.h"
#include "IconsFontAwesome5.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_stdlib.h"
#include "Localization.h"
#include "InputBinding.h"
#include "NoteTypes.h"
#include <vector>

#define APP_NAME "MikuMikuWorld EXTENDED"

namespace MikuMikuWorld
{
	constexpr float primaryLineThickness = 0.250f;
	constexpr float secondaryLineThickness = 0.150f;
	constexpr float tertiaryLineThickness = 0.100f;
	constexpr float toolTipDelay = 0.5f;

	constexpr ImGuiChildFlags ImGuiChildFlags_PaddedBorder =
	    ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
	constexpr ImGuiTableFlags ImGuiTableFlags_PropertyTable =
	    ImGuiTableFlags_Resizable | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame |
	    ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoClip | ImGuiTableFlags_PreciseWidths;

	enum class ColorDisplay : uint8_t
	{
		RGB,
		HSV,
		HEX
	};

	enum class BaseTheme : uint8_t
	{
		DARK,
		LIGHT,
		BASE_THEME_MAX
	};

	constexpr const char* colorDisplayStr[]{ "RGB", "HSV", "Hex" };

	class UI
	{
	  private:
		static const ImVec2 _btnNormal;
		static const ImVec2 _btnSmall;
		static const ImVec2 _toolbarBtnSize;
		static const ImVec2 _toolbarBtnImgSize;

	  public:
		static ImVec2 btnNormal;
		static ImVec2 btnSmall;
		static ImVec2 toolbarBtnSize;
		static ImVec2 toolbarBtnImgSize;
		static std::array<ImVec4, 9> accentColors;

		static float scale(float value);
		static ImVec2 scale(const ImVec2& size);
		inline static std::string itemLabel(std::string_view label)
		{
			std::string labelStr{ label };
			return IO::formatString("%s###%s", localize(labelStr), labelStr);
		}
		inline static std::string modalTitle(std::string_view title)
		{
			std::string titleStr{ title };
			return IO::formatString("%s - %s###%s", APP_NAME, localize(titleStr), titleStr);
		}
		inline static std::string iconTitle(std::string_view icon, std::string_view title)
		{
			std::string titleStr{ title };
			return IO::formatString("%s %s###%s", icon.data(), localize(titleStr), titleStr);
		}
		static bool transparentButton(const char* label, ImVec2 size = btnNormal,
		                              bool repeat = false, bool enabled = true);
		static bool transparentButton2(const char* label, ImVec2 pos, ImVec2 size);
		static bool togglableButton(const char* label, ImVec2 size, bool& selected);
		static bool coloredButton(const char* label, ImVec2 pos, ImVec2 size, ImU32 col,
		                          bool enabled = true);
		static bool isAnyPopupOpen();

		static bool beginPropertyTable(const char* id = "prop_table", int columns = 2,
		                               ImGuiTableFlags flags = ImGuiTableFlags_PropertyTable);
		static void endPropertyTable();
		static void labelPropertyColumn(const char* label);
		static void labelPropertyRow(const char* label, std::string_view value);
		static bool intPropertyRow(std::string_view text, int& val, const char* format = "%d",
		                           int lowerBound = 0, int higherBound = 0);
		static bool floatPropertyRow(std::string_view text, float& val, const char* format = "%.3f",
		                             float lowerBound = 0.0f, float higherBound = 0.0f);
		static bool doublePropertyRow(std::string_view text, double& val,
		                              const char* format = "%.6f", double lowerBound = 0.0,
		                              double higherBound = 0.0);
		static bool dragFloatPropertyRow(std::string_view text, float& val,
		                                 const char* format = "%.3f");
		static bool intSliderPropertyRow(std::string_view text, int& val, int min, int max,
		                                 const char* format = "%d");
		static bool floatSliderPropertyRow(std::string_view text, float& val, float min, float max,
		                                   const char* format = "%.3f");
		static void percentSliderPropertyRow(std::string_view text, float& val);
		static bool stringPropertyRow(std::string_view text, std::string& val);
		static bool fractionPropertyRow(std::string_view text, int& numerator, int& denominator);
		static bool checkboxPropertyRow(std::string_view text, bool& val);
		static int filePropertyRow(std::string_view text, std::string& val,
		                           const char* hint = "n/a");
		static void statPropertyColumn(std::string_view text, int mode, ImVec2 size);
		// = 0: no change
		// > 0: value set
		// < 0: value increment/decrement
		static int mixedIntPropertyRow(std::string_view text, bool mixed, int& val,
		                               const char* format = "%d", int lowerBound = 0,
		                               int higherBound = 0, int step = 1);
		static int mixedFloatPropertyRow(std::string_view text, bool mixed, float& val,
		                                 const char* format = "%.3f", float lowerBound = 0,
		                                 float higherBound = 0, float step = 1);
		static void separatorRow();
		static void tooltip(const char* label);
		static bool divisionSelect(const char* label, int& value, const int* items, size_t count);
		static bool zoomControl(const char* label, float& value, float min, float max, float width,
		                        float factor = 2);
		static bool timeSignatureSelect(int& numerator, int& denominator);
		static void centeredText(const std::string& str);
		static void centeredText(const char* str);
		void drawShadedText(ImDrawList* drawList, ImVec2 textPos, float fontSize, ImU32 fontColor,
		                    const char* text);
		static void updateUIScaling(float scale);

		/// <summary>
		/// For use with sequential enums only
		/// </summary>
		template <typename StrType, size_t N, typename TEnum,
		          typename TUnder = std::underlying_type_t<TEnum>>
		static bool selectPropertyRow(std::string_view text, TEnum& value,
		                              const StrType (&texts)[N], TEnum max = TEnum(N),
		                              TEnum start = TEnum(0))
		{
			labelPropertyColumn(localize(text));

			bool edited = false;
			const TUnder val = static_cast<TUnder>(value);
			const TUnder begVal = static_cast<TUnder>(start);
			const TUnder maxVal = static_cast<TUnder>(max);

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::PushID(text.data(), text.data() + text.size());
			if (ImGui::BeginCombo("", localize(texts[val])))
			{
				for (TUnder i = begVal; i < maxVal; ++i)
				{
					const bool selected = (val == i);

					if (ImGui::Selectable(localize(texts[i]), selected))
					{
						value = static_cast<TEnum>(i);
						edited = true;
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopID();

			return edited;
		}

		template <typename T, typename ValueIt, typename InputIt, typename FValue, typename F>
		static auto selectPropertyRow(std::string_view text, T& value, ValueIt valueBegin,
		                              ValueIt valueEnd, FValue valueCast, InputIt first, F cstrCast)
		    -> typename std::enable_if_t<
		        std::is_same_v<decltype(valueCast(*valueBegin) == value), bool>, bool>
		{
			labelPropertyColumn(localize(text));

			bool edited = false;

			const char* preview = "";
			InputIt selectedIt = std::next(first, std::distance(valueBegin, valueEnd));
			InputIt keyIt = first;
			for (auto valIt = valueBegin; valIt != valueEnd; ++valIt, ++keyIt)
			{
				if (valueCast(*valIt) == value)
				{
					preview = cstrCast(*keyIt);
					selectedIt = keyIt;
					break;
				}
			}

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::PushID(text.data(), text.data() + text.size());
			if (ImGui::BeginCombo("", preview))
			{
				keyIt = first;
				for (auto valIt = valueBegin; valIt != valueEnd; ++valIt, ++keyIt)
				{
					const bool selected = (keyIt == selectedIt);

					if (ImGui::Selectable(cstrCast(*keyIt), selected))
					{
						value = valueCast(*valIt);
						edited = true;
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopID();

			return edited;
		}

		template <typename T, typename TList, typename InputIt, typename F>
		static auto selectPropertyRow(std::string_view text, T& value, TList values, size_t size,
		                              InputIt first, F cstrCast) ->
		    typename std::enable_if_t<std::is_same_v<decltype(values[0] == value), bool>, bool>
		{
			return selectPropertyRow(
			    text, value, std::begin(values), std::begin(values) + size,
			    [](auto&& v) -> const T& { return v; }, first, cstrCast);
		}

		template <typename T, typename TList, typename InputIt>
		static auto selectPropertyRow(std::string_view text, T& value, TList values, size_t size,
		                              InputIt first) ->
		    typename std::enable_if_t<std::is_same_v<decltype(values[0] == value), bool>, bool>
		{
			return selectPropertyRow(text, value, values, size, first,
			                         [](const char* s) { return s; });
		}

		template <typename StrType, size_t N, typename TEnum,
		          typename TUnder = std::underlying_type_t<TEnum>>
		static bool selectMenuItems(std::string_view text, bool enabled, const StrType (&texts)[N],
		                            TEnum& selected, TEnum max = TEnum(N), TEnum start = TEnum(0))
		{
			bool activated = false;
			const TUnder begVal = static_cast<TUnder>(start);
			const TUnder maxVal = static_cast<TUnder>(max);

			if (ImGui::BeginMenu(localize(text), enabled))
			{
				for (TUnder i = begVal; i < maxVal; ++i)
					if (ImGui::MenuItem(localize(texts[i])))
					{
						selected = static_cast<TEnum>(i);
						activated = true;
					}
				ImGui::EndMenu();
			}
			return activated;
		}

		template <typename StrType, size_t N, typename TEnum,
		          typename TUnder = std::underlying_type_t<TEnum>>
		static bool selectMixedPropertyRow(std::string_view text, bool mixed, TEnum& value,
		                                   StrType mixedText, const StrType (&texts)[N],
		                                   TEnum max = TEnum(N), TEnum start = TEnum(0))
		{
			labelPropertyColumn(localize(text));

			bool edited = false;
			const TUnder val = mixed ? static_cast<TUnder>(-1) : static_cast<TUnder>(value);
			const TUnder begVal = static_cast<TUnder>(start);
			const TUnder maxVal = std::min<TUnder>(static_cast<TUnder>(max), N);

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::PushID(text.data(), text.data() + text.size());
			if (ImGui::BeginCombo("", localize(mixed ? mixedText : texts[val])))
			{
				for (TUnder i = begVal; i < maxVal; ++i)
				{
					const bool selected = (val == i);

					if (ImGui::Selectable(localize(texts[i]), selected))
					{
						value = static_cast<TEnum>(i);
						edited = true;
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::PopID();

			return edited;
		}

		template <typename TEnum, typename TUnder = std::underlying_type_t<TEnum>>
		static bool checkboxFlagPropertyRow(std::string_view text, TEnum& value, TEnum flag,
		                                    bool mixed = false)
		{
			labelPropertyColumn(localize(text));

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::PushID(text.data(), text.data() + text.size());
			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
			bool val = hasFlag(value, flag);
			bool edited = ImGui::Checkbox("", &val);
			ImGui::PopItemFlag();
			ImGui::PopID();
			if (edited)
				value = setFlag(value, flag, val);

			return edited;
		}
	};
}

namespace ImGui
{
	void AddQuadMultiColor(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2,
	                       const ImVec2& p3, const ImVec2& p4, ImU32 col1, ImU32 col2, ImU32 col3,
	                       ImU32 col4);

	void AddImageMultiColor(ImDrawList* drawList, ImTextureRef texture_ref, const ImVec2& p_min,
	                        const ImVec2& p_max, const ImVec2& uv_min = ImVec2(0, 0),
	                        const ImVec2& uv_max = ImVec2(1, 1),
	                        ImU32 col_upr_left = IM_COL32_WHITE,
	                        ImU32 col_upr_right = IM_COL32_WHITE,
	                        ImU32 col_bot_right = IM_COL32_WHITE,
	                        ImU32 col_bot_left = IM_COL32_WHITE);
	void AddImageQuadMultiColor(ImDrawList* drawList, ImTextureRef tex_ref, const ImVec2& p1,
	                            const ImVec2& p2, const ImVec2& p3, const ImVec2& p4,
	                            const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
	                            const ImVec2& uv4, ImU32 col1, ImU32 col2, ImU32 col3, ImU32 col4);
}