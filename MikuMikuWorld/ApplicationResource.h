#pragma once
#include <unordered_map>
#include "NoteResource.h"
#include "Rendering/Shader.h"

namespace MikuMikuWorld
{
	struct ApplicationResource
	{
		std::unordered_map<std::string, Shader> shaders;
		TimelineTexture timelineTexture;
		NoteTexture noteTexture;

		void loadShader(const std::string& name);
	};

	Shader* getShader(const std::string& name);
	ApplicationResource& getResources();
}