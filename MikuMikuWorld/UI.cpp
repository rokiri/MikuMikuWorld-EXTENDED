#include "UI.h"
#include "Utilities.h"
#include "Colors.h"
#include "Text.h"
#include "Tempo.h"
#include "ApplicationResource.h"
#include <string>
#include <algorithm>

namespace MikuMikuWorld
{
	const ImVec2 UI::_btnNormal{ 24, 24 };
	const ImVec2 UI::_btnSmall{ 19, 19 };
	const ImVec2 UI::_toolbarBtnSize{ 22, 22 };
	const ImVec2 UI::_toolbarBtnImgSize{ 20, 20 };

	ImVec2 UI::btnNormal{ UI::_btnNormal };
	ImVec2 UI::btnSmall{ UI::_btnSmall };
	ImVec2 UI::toolbarBtnSize{ UI::_toolbarBtnSize };
	ImVec2 UI::toolbarBtnImgSize{ UI::_toolbarBtnImgSize };
	float uiScale = 1.f;

	std::array<ImVec4, 9> UI::accentColors{
		ImVec4{ 0.10f, 0.10f, 0.10f, 1.00f }, // User
		ImVec4{ 0.45f, 0.29f, 0.75f, 1.00f }, // Default : Untilted Charts
		ImVec4{ 0.51f, 0.80f, 0.82f, 1.00f }, // Default : Chart Cyanvas
		ImVec4{ 0.16f, 0.44f, 0.75f, 1.00f }, // Default : Original MMW
		ImVec4{ 0.30f, 0.31f, 0.86f, 1.00f }, // Light Music
		ImVec4{ 0.40f, 0.69f, 0.15f, 1.00f }, // Idol
		ImVec4{ 0.76f, 0.05f, 0.32f, 1.00f }, // Street
		ImVec4{ 0.81f, 0.45f, 0.06f, 1.00f }, // Theme Park
		ImVec4{ 0.50f, 0.25f, 0.55f, 1.00f }, // School Refusal
	};

	float UI::scale(float value) { return std::floor(value * uiScale); }

	ImVec2 UI::scale(const ImVec2& size) { return floor(size * uiScale); }

	bool UI::transparentButton(const char* txt, ImVec2 size, bool repeat, bool enabled)
	{
		ImGui::BeginDisabled(!enabled);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scale({ 2.5f, 2 }));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, repeat);

		bool pressed = ImGui::Button(txt, size);

		ImGui::PopItemFlag();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::EndDisabled();
		return pressed;
	}

	bool UI::transparentButton2(const char* txt, ImVec2 pos, ImVec2 size)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.4f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, scale(3.0f));

		ImGui::SetCursorScreenPos(pos);
		ImGui::PushID(txt);
		bool pressed = ImGui::Button("", size);
		ImGui::PopID();

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		return pressed;
	}

	bool UI::togglableButton(const char* txt, ImVec2 size, bool& selected)
	{
		const auto& colors = ImGui::GetStyle().Colors;
		if (selected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, colors[ImGuiCol_ButtonActive]);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors[ImGuiCol_ButtonActive]);
		}
		bool pressed = ImGui::Button(txt, size);
		if (selected)
			ImGui::PopStyleColor(2);
		if (pressed)
			selected = !selected;
		return pressed;
	}

	bool UI::coloredButton(const char* txt, ImVec2 pos, ImVec2 size, ImU32 col, bool enabled)
	{
		ImVec4 colb = ImGui::ColorConvertU32ToFloat4(col);
		ImVec4 colh = generateHighlightColor(colb);
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !enabled);
		ImGui::PushStyleColor(ImGuiCol_Button, colb);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colh);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, colh);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.15f, 0.15f, 1.0));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, scale(1.5f));

		ImGui::SetCursorScreenPos(pos);
		ImVec2 sz = size;
		if (sz.x <= 0)
			sz.x = std::max(ImGui::CalcTextSize(txt).x + 5.0f, 30.0f);

		if (sz.y <= 0)
			sz.y = ImGui::GetFrameHeightWithSpacing();

		sz += ImGui::GetStyle().FramePadding;

		bool pressed = ImGui::Button(txt, sz);

		ImGui::PopStyleVar(1);
		ImGui::PopStyleColor(4);
		ImGui::PopItemFlag();
		return pressed;
	}

	bool UI::isAnyPopupOpen()
	{
		return ImGui::IsPopupOpen(ImGuiID(0),
		                          ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	}

	bool UI::beginPropertyTable(const char* id, int columns, ImGuiTableFlags flags)
	{
		return ImGui::BeginTable(id, columns, flags);
	}

	void UI::endPropertyTable() { ImGui::EndTable(); }

	void UI::labelPropertyColumn(const char* label)
	{
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(label);
	}

	void UI::labelPropertyRow(const char* label, std::string_view value)
	{
		labelPropertyColumn(label);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(value.data(), value.data() + value.size());
	}

	bool UI::intPropertyRow(std::string_view text, int& val, const char* format, int lowerBound,
	                        int higherBound)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		int step = 1;
		ImGui::PushID(text.data(), text.data() + text.size());
		bool edited = ImGui::InputScalar("", ImGuiDataType_S32, &val, &step, &step, format);
		ImGui::PopID();
		if (edited && lowerBound != higherBound)
			val = std::clamp(val, lowerBound, higherBound);

		return edited;
	}

	bool UI::floatPropertyRow(std::string_view text, float& val, const char* format,
	                          float lowerBound, float higherBound)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool edited = ImGui::InputFloat("", &val, 1.0f, 10.f, format);
		ImGui::PopID();
		if (edited && lowerBound != higherBound)
			val = std::clamp(val, lowerBound, higherBound);

		return edited;
	}

	bool UI::doublePropertyRow(std::string_view text, double& val, const char* format,
	                           double lowerBound, double higherBound)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool edited = ImGui::InputDouble("", &val, 1.0, 10.0, format);
		ImGui::PopID();
		if (edited && lowerBound != higherBound)
			val = std::clamp(val, lowerBound, higherBound);

		return edited;
	}

	bool UI::dragFloatPropertyRow(std::string_view text, float& val, const char* format)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool active =
		    ImGui::DragFloat("", &val, 1.0f, 0.0f, 0.0f, format, ImGuiSliderFlags_NoRoundToFormat);
		ImGui::PopID();

		return active;
	}

	bool UI::intSliderPropertyRow(std::string_view text, int& val, int min, int max,
	                              const char* format)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool active = ImGui::SliderInt("", &val, min, max, format, ImGuiSliderFlags_AlwaysClamp);
		ImGui::PopID();

		return active;
	}

	bool UI::floatSliderPropertyRow(std::string_view text, float& val, float min, float max,
	                                const char* format)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool active = ImGui::SliderFloat("", &val, min, max, format, ImGuiSliderFlags_AlwaysClamp);
		ImGui::PopID();

		return active;
	}

	void UI::percentSliderPropertyRow(std::string_view text, float& val)
	{
		float scaled = val * 100;
		if (floatSliderPropertyRow(text, scaled, 0, 100, "%.0f%%"))
			val = scaled / 100.0f;
	}

	bool UI::stringPropertyRow(std::string_view text, std::string& val)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool active = ImGui::InputText("", &val);
		ImGui::PopID();

		return active;
	}

	int UI::filePropertyRow(std::string_view text, std::string& val, const char* hint)
	{
		labelPropertyColumn(localize(text));

		int result = 0;
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		ImGui::SetNextItemWidth(-UI::btnSmall.x - ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::InputTextWithHint("", hint, &val, ImGuiInputTextFlags_EnterReturnsTrue))
			result = 1;
		ImGui::SameLine();

		if (ImGui::Button("...", UI::btnSmall))
			result = -1;

		ImGui::PopID();

		return result;
	}

	void UI::statPropertyColumn(std::string_view text, int mode, ImVec2 size)
	{
		auto& resource = getResources().timelineTexture;
		const Texture* texture = resource.getToolbarTexture();
		const Sprite* sprite = resource.getScoreStatsSprite(InsertMode(mode));
		if (texture && sprite)
		{
			ImGui::TableNextColumn();
			auto&& [uv0, uv1] = texture->getCoords(*sprite);
			ImGui::Image(texture->getID(), size, uv0, uv1);
		}
		else
		{
			labelPropertyColumn(localize(text));
		}
	}

	template <typename T>
	static int mixedScalarInput(bool mixed, T& value, const char* format, T step)
	{
		int value_changed = 0;
		const ImGuiStyle& style = ImGui::GetStyle();
		const float button_size = ImGui::GetFrameHeight();
		char buf[64];
		ImGuiDataType dataType;
		if constexpr (std::is_same_v<T, int>)
			dataType = ImGuiDataType_S32;
		else if constexpr (std::is_same_v<T, float>)
			dataType = ImGuiDataType_Float;
		else
			static_assert(false && "DataType not supported!");
		if (!mixed)
			ImGui::DataTypeFormatString(buf, IM_ARRAYSIZE(buf), dataType, &value, format);
		else
		{
			buf[0] = '\0';
			std::strcpy(buf, "—");
		}
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(
		    std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
		if (ImGui::InputText("", buf, IM_ARRAYSIZE(buf),
		                     ImGuiInputTextFlags_AutoSelectAll |
		                         ImGuiInputTextFlags_EnterReturnsTrue))
			value_changed =
			    mixed || ImGui::DataTypeApplyFromText(buf, dataType, &value, format, NULL);

		// Step buttons
		ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, style.FramePadding.y);
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		if (ImGui::ButtonEx("-", { button_size, button_size }))
		{
			value -= step;
			value_changed = -1;
		}
		if (ImGui::IsItemDeactivated())
			value_changed = -1;
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		if (ImGui::ButtonEx("+", { button_size, button_size }))
		{
			value += step;
			value_changed = -1;
		}
		if (ImGui::IsItemDeactivated())
			value_changed = -1;
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
		ImGui::EndGroup();
		return value_changed;
	}

	int UI::mixedIntPropertyRow(std::string_view text, bool mixed, int& val, const char* format,
	                            int lowerBound, int higherBound, int step)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		int edited = mixedScalarInput(mixed, val, format, step);
		ImGui::PopID();

		if (edited && lowerBound != higherBound)
			val = std::clamp(val, lowerBound, higherBound);

		return edited;
	}

	int UI::mixedFloatPropertyRow(std::string_view text, bool mixed, float& val, const char* format,
	                              float lowerBound, float higherBound, float step)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		int edited = mixedScalarInput(mixed, val, format, step);
		ImGui::PopID();
		if (edited && lowerBound != higherBound)
			val = std::clamp(val, lowerBound, higherBound);

		return edited;
	}

	bool UI::fractionPropertyRow(std::string_view text, int& numerator, int& denominator)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());

		float controlWidth = ImGui::GetContentRegionAvail().x / 2.0f - ImGui::CalcTextSize("/").x -
		                     ImGui::GetStyle().ItemSpacing.x * 2;
		bool edit = false;

		ImGui::PushID("numerator");
		ImGui::SetNextItemWidth(controlWidth);
		ImGui::InputInt("", &numerator, 0, 0);
		edit |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::PopID();

		ImGui::SameLine();
		ImGui::Text("/");
		ImGui::SameLine();

		ImGui::PushID("denominator");
		ImGui::SetNextItemWidth(controlWidth);
		ImGui::InputInt("", &denominator, 0, 0);
		edit |= ImGui::IsItemDeactivatedAfterEdit();

		ImGui::PopID();

		return edit;
	}

	bool UI::checkboxPropertyRow(std::string_view text, bool& val)
	{
		labelPropertyColumn(localize(text));

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PushID(text.data(), text.data() + text.size());
		bool edited = ImGui::Checkbox("", &val);
		ImGui::PopID();

		return edited;
	}

	void UI::tooltip(const char* label)
	{
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
		{
			float txtWidth = ImGui::CalcTextSize(label).x + (ImGui::GetStyle().WindowPadding.x * 2);
			ImGui::SetNextWindowSize(ImVec2(std::min(txtWidth, 250.0f), -1));
			if (ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious,
			                          ImGuiWindowFlags_NoResize))
			{
				ImGui::TextWrapped("%s", label);
				ImGui::EndTooltip();
			}
		}
	}

	bool UI::divisionSelect(const char* label, int& value, const int* items, size_t count)
	{
		ImGui::PushID(label);

		const char* popupWindow = "##division_combo";
		ImVec2 popupPos = ImGui::GetCursorScreenPos();
		bool activate = false;
		if (ImGui::InputScalar("##division_input", ImGuiDataType_S32, &value, NULL, NULL,
		                       localize(Text::divisionAffix),
		                       ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank))
			value = std::clamp(value, items[0], items[count - 1]);
		ImVec2 popupSize = ImGui::GetItemRectSize();
		activate = ImGui::IsItemDeactivatedAfterEdit();

		// enable right-click
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		    ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
			ImGui::OpenPopup(popupWindow);

		ImGui::SameLine(0, 0);
		if (ImGui::ArrowButton("##open_combo", ImGuiDir_Down))
			ImGui::OpenPopup(popupWindow);
		popupSize.x += ImGui::GetItemRectSize().x;

		ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always, { 0, 1 });
		ImGui::SetNextWindowSizeConstraints(popupSize, { FLT_MAX, FLT_MAX });
		if (ImGui::BeginPopupContextItem(popupWindow))
		{
			for (size_t i = 0; i < count; ++i)
			{
				std::string divStr = IO::formatString(localize(Text::division), items[i]);
				if (ImGui::Selectable(divStr.c_str(), value == items[i]))
				{
					value = items[i];
					activate = true;
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
		return activate;
	}

	bool UI::zoomControl(const char* label, float& value, float min, float max, float width,
	                     float factor)
	{
		ImGui::PushID(label);
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

		bool act = false;
		if (UI::transparentButton(ICON_FA_SEARCH_MINUS, UI::btnSmall, true, value > min))
		{
			value = std::max(min, value / factor);
			act = true;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(width);

		act |= ImGui::SliderFloat("", &value, min, max, "%.2fx", ImGuiSliderFlags_Logarithmic);
		tooltip(localize(Text::zoomTooltip));
		ImGui::SameLine();

		if (UI::transparentButton(ICON_FA_SEARCH_PLUS, UI::btnSmall, true, value < max))
		{
			value = std::min(max, value * factor);
			act = true;
		}
		ImGui::PopItemFlag();
		ImGui::PopID();

		return act;
	}

	bool UI::timeSignatureSelect(int& numerator, int& denominator)
	{
		labelPropertyColumn(localize(Text::timeSignature));

		ImGui::TableNextColumn();
		float controlWidth = (ImGui::GetContentRegionAvail().x / 2.0f) -
		                     ImGui::CalcTextSize("/").x - ImGui::GetStyle().ItemSpacing.x * 2;
		bool edit = false;

		ImGui::SetNextItemWidth(controlWidth);
		if (ImGui::BeginCombo("##ts_num", std::to_string(numerator).c_str()))
		{
			for (int i = 1; i <= 32; ++i)
			{
				if (ImGui::Selectable(std::to_string(i).c_str()))
				{
					edit = numerator != i;
					numerator = i;
				}
				if (numerator == i)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::Text("/");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(controlWidth);

		if (ImGui::BeginCombo("##ts_denom", std::to_string(denominator).c_str()))
		{
			for (int i = 1; i <= 6; ++i)
			{
				int denom = std::pow(2, i);
				if (ImGui::Selectable(std::to_string(denom).c_str()))
				{
					edit = denominator != denom;
					denominator = denom;
				}
				if (denominator == denom)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return edit;
	}

	void UI::updateUIScaling(float scale)
	{
		uiScale = scale;
		btnNormal = floor(_btnNormal * scale);
		btnSmall = floor(_btnSmall * scale);
		toolbarBtnSize = floor(_toolbarBtnSize * scale);
		toolbarBtnImgSize = floor(_toolbarBtnImgSize * scale);
	}

	void UI::centeredText(const std::string& str) { UI::centeredText(str.c_str()); }

	void UI::centeredText(const char* str)
	{
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(str).x) / 2);
		ImGui::TextUnformatted(str);
	}

	void UI::drawShadedText(ImDrawList* drawList, ImVec2 textPos, float fontSize, ImU32 fontColor,
	                        const char* text)
	{
		if (!drawList)
			return;

		drawList->AddText(ImGui::GetFont(), fontSize, textPos + ImVec2{ 0.75f, 1.0f }, 0xff111111,
		                  text);
		drawList->AddText(ImGui::GetFont(), fontSize, textPos, fontColor, text);
	}
}

void ImGui::AddQuadMultiColor(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2,
                              const ImVec2& p3, const ImVec2& p4, ImU32 col1, ImU32 col2,
                              ImU32 col3, ImU32 col4)
{
	if (!drawList)
		return;
	if (((col1 | col2 | col3 | col4) & IM_COL32_A_MASK) == 0)
		return;

	const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
	drawList->PrimReserve(6, 4);
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 1));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 2));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 2));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 3));
	drawList->PrimWriteVtx(p1, uv, col1);
	drawList->PrimWriteVtx(p2, uv, col2);
	drawList->PrimWriteVtx(p3, uv, col3);
	drawList->PrimWriteVtx(p4, uv, col4);
}

void ImGui::AddImageMultiColor(ImDrawList* drawList, ImTextureRef texture_ref, const ImVec2& p_min,
                               const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max,
                               ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right,
                               ImU32 col_bot_left)
{
	if (!drawList)
		return;
	if (((col_upr_left | col_upr_right | col_bot_right | col_bot_left) & IM_COL32_A_MASK) == 0)
		return;
	const bool push_texture_id = texture_ref != drawList->_CmdHeader.TexRef;
	if (push_texture_id)
		drawList->PushTexture(texture_ref);

	drawList->PrimReserve(6, 4);
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 1));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 2));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 2));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 3));
	drawList->PrimWriteVtx(p_min, uv_min, col_upr_left);
	drawList->PrimWriteVtx({ p_max.x, p_min.y }, { uv_max.x, uv_min.y }, col_upr_right);
	drawList->PrimWriteVtx(p_max, uv_max, col_bot_right);
	drawList->PrimWriteVtx({ p_min.x, p_max.y }, { uv_min.x, uv_max.y }, col_bot_left);

	if (push_texture_id)
		drawList->PopTexture();
}

void ImGui::AddImageQuadMultiColor(ImDrawList* drawList, ImTextureRef tex_ref, const ImVec2& p1,
                                   const ImVec2& p2, const ImVec2& p3, const ImVec2& p4,
                                   const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
                                   const ImVec2& uv4, ImU32 col1, ImU32 col2, ImU32 col3,
                                   ImU32 col4)
{
	if (!drawList)
		return;
	if (((col1 | col2 | col3 | col4) & IM_COL32_A_MASK) == 0)
		return;
	const bool push_texture_id = tex_ref != drawList->_CmdHeader.TexRef;
	if (push_texture_id)
		drawList->PushTexture(tex_ref);

	drawList->PrimReserve(6, 4);
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 1));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 2));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 2));
	drawList->PrimWriteIdx((ImDrawIdx)(drawList->_VtxCurrentIdx + 3));
	drawList->PrimWriteVtx(p1, uv1, col1);
	drawList->PrimWriteVtx(p2, uv2, col2);
	drawList->PrimWriteVtx(p3, uv3, col3);
	drawList->PrimWriteVtx(p4, uv4, col4);

	if (push_texture_id)
		drawList->PopTexture();
}
