#include "../File.h"
#include "../IO.h"
#include "../PlatformIO.h"
#include "../JsonIO.h"
#include "Texture.h"
#include <glad/glad.h>
#include "GLFW/glfw3.h"
#include "stb_image.h"
#include <filesystem>
#include <fstream>

using namespace IO;
using namespace jsonIO;
using json = nlohmann::json;

namespace MikuMikuWorld
{
	Sprite::Sprite(float _x, float _y, float _w, float _h, std::string name)
	    : x{ _x }, y{ _y }, width{ _w }, height{ _h }, name{ name }
	{
	}

	float Sprite::getX() const { return x; }

	float Sprite::getY() const { return y; }

	float Sprite::getWidth() const { return width; }

	float Sprite::getHeight() const { return height; }

	const std::string& Sprite::getName() const { return name; };

	Texture::Texture(const std::string& filename)
	    : Texture(filename, TextureFilterMode::Linear, TextureFilterMode::Linear)
	{
	}

	Texture::Texture(const std::string& filename, TextureFilterMode filter)
	    : Texture(filename, filter, filter)
	{
	}

	Texture::Texture(const std::string& filename, TextureFilterMode min, TextureFilterMode mag)
	{
		glID = 0;
		this->filename = filename;
		auto filepath = IO::stringToPath(filename);
		name = IO::toString(File::getFilenameWithoutExtension(filepath));
		createTexture(filename, min, mag);

		// Default sprite
		sprites.emplace(name, Sprite(0, 0, width, height, name));

		filepath.replace_extension(".json");
		if (File::exists(filepath))
		{
			File sprFile(filepath, FileMode::Read);
			json sprJson = json::parse(sprFile.readAllText());
			sprFile.close();
			if (jsonIO::arrayHasData(sprJson, "sprites"))
			{
				for (auto&& jspr : sprJson["sprites"])
				{
					if (!keyExists(jspr, "name", json::value_t::string) ||
					    !keyExists(jspr, "x", json::value_t::number_unsigned) ||
					    !keyExists(jspr, "y", json::value_t::number_unsigned) ||
					    !keyExists(jspr, "w", json::value_t::number_unsigned) ||
					    !keyExists(jspr, "h", json::value_t::number_unsigned))
						continue;
					sprites.emplace(
					    jspr["name"].get<std::string>(),
					    Sprite(jspr["x"], jspr["y"], jspr["w"], jspr["h"],
					                                     jspr["name"]));
				}
			}
		}
	}

	Texture::~Texture() { dispose(); }

	std::pair<Vector2, Vector2> Texture::getCoords(const Sprite& sprite) const
	{
		assert(sprites.find(sprite.getName()) != sprites.end() && "Sprite is not of this texture!");
		float x0 = sprite.getX() / width, y0 = sprite.getY() / height;
		float x1 = (sprite.getX() + sprite.getWidth()) / width,
		      y1 = (sprite.getY() + sprite.getHeight()) / height;
		return { { x0, y0 }, { x1, y1 } };
	}

	void Texture::bind() const { glBindTexture(GL_TEXTURE_2D, glID); }

	void Texture::dispose() const
	{
		if (glID == 0)
			return;
		glDeleteTextures(1, &glID);
	}

	const Sprite* Texture::getDefaultSprite() const { return getSprite(name); }

	const Sprite* MikuMikuWorld::Texture::getSprite(const std::string& spriteName) const
	{
		auto it = sprites.find(spriteName);
		return it == sprites.end() ? nullptr : &it->second;
	}

	void Texture::createTexture(const std::string& filename, TextureFilterMode minFilter,
	                            TextureFilterMode magFilter)
	{
		glGenTextures(1, &glID);
		glBindTexture(GL_TEXTURE_2D, glID);

		int nrChannels;
		stbi_set_flip_vertically_on_load(0);
		auto data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 4);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)magFilter);

		glBindTexture(GL_TEXTURE_2D, 0);
		free(data);
	}
}
