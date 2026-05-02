#include "Framebuffer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

namespace MikuMikuWorld
{
	Framebuffer::Framebuffer(unsigned int w, unsigned int h)
	    : fbo(), rbo(), buffer(), width{ w }, height{ h }
	{
		setup();
	}

	Framebuffer::~Framebuffer() { dispose(); }

	Framebuffer::Framebuffer(Framebuffer&& other) noexcept
	    : fbo(other.fbo), rbo(other.rbo), buffer(other.buffer), width(other.width),
	      height(other.height)
	{
		other.fbo = 0;
		other.rbo = 0;
		other.buffer = 0;
		other.width = 0;
		other.height = 0;
	}

	Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
	{
		if (this != &other)
		{
			dispose();

			fbo = other.fbo;
			rbo = other.rbo;
			buffer = other.buffer;
			width = other.width;
			height = other.height;

			other.fbo = 0;
			other.rbo = 0;
			other.buffer = 0;
			other.width = 0;
			other.height = 0;
		}
		return *this;
	}

	unsigned int Framebuffer::getWidth() const { return width; }

	unsigned int Framebuffer::getHeight() const { return height; }

	unsigned int Framebuffer::getTexture() const { return buffer; }

	void Framebuffer::clear(float r, float g, float b, float a)
	{
		GLbitfield clearBits = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		glClearColor(r, g, b, a);
		glClear(clearBits);
	}

	void Framebuffer::bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, width, height);
	}

	void Framebuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

	void Framebuffer::dispose()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		if (fbo)
			glDeleteFramebuffers(1, &fbo);
		fbo = 0;

		if (buffer)
			glDeleteTextures(1, &buffer);
		buffer = 0;

		if (rbo)
			glDeleteRenderbuffers(1, &rbo);
		rbo = 0;

		width = 0;
		height = 0;
	}

	void Framebuffer::resize(unsigned int w, unsigned int h)
	{
		if (width == w && height == h)
			return;

		width = w;
		height = h;

		createTexture(buffer);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	}

	void Framebuffer::createTexture(unsigned int tex)
	{
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}

	void Framebuffer::setup()
	{
		glGenFramebuffers(1, &fbo);
		glGenTextures(1, &buffer);
		createTexture(buffer);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
		                          rbo);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			fprintf(stderr, "Framebuffer::setup() ERROR: Incomplete framebuffer");

		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}