#include "Application.h"
#include "ApplicationResource.h"

namespace MikuMikuWorld
{
	ApplicationResource& MikuMikuWorld::getResources() { return Application::instance->resource; }

	Shader* getShader(const std::string& name)
	{
		auto& shaders = Application::instance->resource.shaders;
		auto it = shaders.find(name);
		return it != shaders.end() ? &it->second : nullptr;
	}

	void ApplicationResource::loadShader(const std::string& filename)
	{
		std::string filepath = (Application::getFullPath("res", "shaders") / filename).u8string();
		shaders.emplace(filename, Shader(filename, filepath));
	}
}