#pragma once
#include <map>
#include <string>
#include <vector>
#include <json.hpp>
#include "Particle.h"
#include "Rendering/Texture.h"
#include "Rendering/Shader.h"
#include "PreviewEngine.h"

namespace MikuMikuWorld
{
	typedef std::map<int, Effect::Particle> ParticleIdMap;

	class ResourceManager
	{
	public:
		static std::vector<Texture> textures;
		static std::vector<Shader*> shaders;
		static std::vector<SpriteTransform> spriteTransforms;

		static void loadTexture(const std::string& filename, TextureFilterMode minFilter = TextureFilterMode::Linear, TextureFilterMode magFilter = TextureFilterMode::Linear);
		static int getTexture(const std::string& name);
		static int getTextureByFilename(const std::string& filename);

		static void loadShader(const std::string& filename);
		static int getShader(const std::string& name);

		static void loadTransforms(const std::string& filename);
		static int loadParticleEffect(const std::string& filename);
		static Effect::Particle& getParticleEffect(int id);
		static int getRootParticleIdByName(const std::string& name);
		static void removeAllParticleEffects();
		static void disposeTexture(int texID);

	private:
		static int nextParticleId;
		static ParticleIdMap particleIdMap;
		static std::map<std::string, int> effectNameToRootIdMap;

		static int readParticle(const nlohmann::json& j);
	};
}
