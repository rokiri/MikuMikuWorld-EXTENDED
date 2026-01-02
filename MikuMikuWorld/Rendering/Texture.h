#pragma once
#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include "../File.h"
#include "../Math.h"

namespace MikuMikuWorld
{
	enum class WrapMode
	{
		Clamp,
		Repeat
	};

	enum class TextureFilterMode
	{
		Linear = GL_LINEAR,
		Nearest = GL_NEAREST,
		LinearMipMapLinear = GL_LINEAR_MIPMAP_LINEAR,
		LinearMipMapNearest = GL_LINEAR_MIPMAP_NEAREST,
		NearestMipMapLinear = GL_NEAREST_MIPMAP_LINEAR,
		NearestMipMapNearest = GL_NEAREST_MIPMAP_NEAREST
	};

	class Sprite
	{
	  private:
		float x, y, width, height;
		std::string name;

	  public:
		Sprite(float _x, float _y, float _w, float _h, std::string name);

		float getX() const;
		float getY() const;
		float getWidth() const;
		float getHeight() const;
		const std::string& getName() const;
	};

	class Texture
	{
	  private:
		std::string name;
		std::string filename;
		int width;
		int height;
		unsigned int glID;

		void createTexture(const std::string& filename,
		          TextureFilterMode minFilter = TextureFilterMode::Linear,
		          TextureFilterMode magFilter = TextureFilterMode::Linear);

	  public:
		std::unordered_map<std::string, Sprite> sprites;

		Texture(const std::string& filename, TextureFilterMode min, TextureFilterMode mag);
		Texture(const std::string& filename, TextureFilterMode filter);
		Texture(const std::string& filename);
		~Texture();

		inline int getWidth() const { return width; }
		inline int getHeight() const { return height; }
		std::pair<Vector2, Vector2> getCoords(const Sprite& sprite) const;
		inline unsigned int getID() const { return glID; }
		inline const std::string& getName() const { return name; }
		inline const std::string& getFilename() const { return filename; }

		const Sprite* getDefaultSprite() const;
		const Sprite* getSprite(const std::string& spriteName) const;

		void bind() const;
		void dispose() const;
	};
}