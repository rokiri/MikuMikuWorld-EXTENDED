#include "Jacket.h"
#include "ImGuiManager.h"
#include "File.h"
#include "Math.h"
#include "PlatformIO.h"
#include "ApplicationResource.h"
#include <algorithm>

namespace MikuMikuWorld
{
	constexpr ImVec2 imageOffset{ 5, 5 };
	constexpr ImVec2 imageSize{ 250, 250 };
	const ImVec2 previewSize{ imageSize + (imageOffset * 2) };

	Jacket::Jacket() { clear(); }

	void Jacket::load(const std::string& filename)
	{
		this->filename = filename;
		if (texture)
		{
			texture->dispose();
			texture = nullptr;
		}

		if (filename.empty() || !IO::File::exists(IO::stringToPath(filename)))
			return;

		texture = std::make_unique<Texture>(filename);
	}

	void Jacket::clear()
	{
		if (texture)
		{
			texture->dispose();
			texture = nullptr;
		}

		filename = "";
	}

	void Jacket::draw() const
	{
		if (texture == nullptr)
			return;

		// animate opacity
		float displayTime = GImGui->HoveredIdTimer - ImGui::GetStyle().HoverDelayNormal;
		float ratio = std::clamp(displayTime / 0.25f, 0.0f, 1.0f);
		ImVec4 color{ 1.0f, 1.0f, 1.0f, ratio };

		ImGui::SetNextWindowSize(UI::scale(previewSize), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(color.w);

		ImGui::BeginTooltip();
		ImGui::ImageWithBg(texture->getID(), ImGui::GetContentRegionAvail(), { 0, 0 }, { 1, 1 },
		                   { 0, 0, 0, 0 }, color);
		ImGui::EndTooltip();
	}

	const std::string& Jacket::getFilename() const { return filename; }

	int Jacket::getTexID() const
	{
		if (texture)
			return texture->getID();

		auto tex = getResources().backgroundResources.getDefaultJacketTexture();
		if (tex)
			return tex->getID();

		return 0;
	}
}