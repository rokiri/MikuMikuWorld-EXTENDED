#include "Application.h"
#include "ApplicationResource.h"

namespace MikuMikuWorld
{
	ApplicationResource& getResources() { return Application::instance->resource; }

	Shader* getShader(const std::string& name)
	{
		auto& shaders = Application::instance->resource.shaders;
		auto it = shaders.find(name);
		return it != shaders.end() ? &it->second : nullptr;
	}

	void ApplicationResource::loadShader(const std::string& name)
	{
		auto path = Application::getInstance().getResourcePath("shaders", IO::stringToPath(name));
		shaders.emplace(name, Shader(name, path));
	}
}