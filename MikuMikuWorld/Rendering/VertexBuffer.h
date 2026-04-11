#pragma once
#include "Quad.h"

namespace MikuMikuWorld
{
	class VertexBuffer
	{
	  private:
		Vertex* buffer;
		int* indices;
		int indexCapacity;
		int vertexCapacity;
		int bufferPos;

		unsigned int vao;
		unsigned int vbo;
		unsigned int ebo;

	  public:
		VertexBuffer(int _capacity);
		~VertexBuffer();

		VertexBuffer(const VertexBuffer&) = delete;
		VertexBuffer& operator=(const VertexBuffer&) = delete;

		VertexBuffer(VertexBuffer&& other) noexcept;
		VertexBuffer& operator=(VertexBuffer&& other) noexcept;

		void setup();
		void dispose();
		void bind() const;
		size_t pushBuffer(const Vertex* vertices, size_t count);
		void resetBufferPos();
		void uploadBuffer();
		void flushBuffer();
		int getCapacity() const;
		int getSize() const;
	};
}