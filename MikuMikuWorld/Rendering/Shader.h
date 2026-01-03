#pragma once
#include <glad/glad.h>
#include "DirectXMath.h"
#include <string>
#include <filesystem>
#include <unordered_map>

namespace MikuMikuWorld
{
	class Shader
	{
	  private:
		unsigned int ID;
		unsigned int uloc;
		std::string name;
		std::unordered_map<std::string, GLint> locMap;

		void compile(const std::filesystem::path& source);
		GLint getUniformLoc(const std::string& name);

	  public:
		Shader(const std::string& name, const std::filesystem::path& source);
		~Shader();

		std::string getName() const;
		void use();

		void setBool(const std::string& name, bool value);
		void setInt(const std::string& name, int value);
		void setFloat(const std::string& name, float value);
		void setVec2(const std::string& name, DirectX::XMVECTOR v);
		void setVec3(const std::string& name, DirectX::XMVECTOR v);
		void setVec4(const std::string& name, DirectX::XMVECTOR v);
		void setMatrix4(const std::string& name, DirectX::XMMATRIX m);
	};
}
