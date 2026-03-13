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

	class Background
	{
	  private:
		std::string filename;
		std::unique_ptr<Texture> texture;
		//std::unique_ptr<Framebuffer> framebuffer;

		float brightness;

		float width;
		float height;

		//bool dirty;

	  public:
		Background();

		void load(const std::string& filename);
		//void resize(Vector2 target);
		//void process(Renderer* renderer);
		void draw(Renderer* renderer, float scrWidth, float scrHeight);
		void draw(ImDrawList* drawList, ImVec2 min, ImVec2 max) const;
		void dispose();

		std::string getFilename() const;

		int getWidth() const;
		int getHeight() const;
		//int getTextureID() const;

		float getBrightness() const;
		void setBrightness(float b);

		//bool isDirty() const;
	};
}
