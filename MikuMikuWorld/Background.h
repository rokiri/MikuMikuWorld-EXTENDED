#pragma once
#include "Rendering/Texture.h"
#include "Rendering/Framebuffer.h"
#include "ImGui/imgui.h"
#include <string>
#include <memory>

namespace MikuMikuWorld
{
	class Renderer;
	class Texture;
	struct Vector2;
	class Jacket;

	class Background
	{
	  private:
		std::unique_ptr<Framebuffer> framebuffer;

		float brightness;

		bool dirty, wantJacket;

	  public:
		Background();

		void process(Renderer* renderer, const Jacket& jacket);
		void draw(ImDrawList* drawList, ImVec2 min, ImVec2 max) const;
		void dispose();

		std::string getFilename() const;

		float getBrightness() const;
		void setBrightness(float b);

		bool isDirty() const;
		void setDirty();

		bool hasJacket() const;
	};
}
