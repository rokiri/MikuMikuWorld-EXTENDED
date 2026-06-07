#include "VertexBuffer.h"
#include "glad/glad.h"
#include <cstddef>

namespace MikuMikuWorld
{
	VertexBuffer::VertexBuffer(int _capacity)
	    : vertexCapacity{ _capacity }, bufferPos{ 0 }, vao{ 0 }, vbo{ 0 }, ebo{ 0 }
	{
		buffer = nullptr;
		indices = nullptr;
		indexCapacity = (vertexCapacity * 6) / 4;
	}

	VertexBuffer::~VertexBuffer() { dispose(); }

	VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept
	    : buffer(other.buffer), indices(other.indices), indexCapacity(other.indexCapacity),
	      vertexCapacity(other.vertexCapacity), bufferPos(other.bufferPos), vao(other.vao),
	      vbo(other.vbo), ebo(other.ebo)
	{
		other.buffer = nullptr;
		other.indices = nullptr;
		other.vao = 0;
		other.vbo = 0;
		other.ebo = 0;
		other.indexCapacity = 0;
		other.vertexCapacity = 0;
		other.bufferPos = 0;
	}

	VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept
	{
		if (this != &other)
		{
			dispose();

			buffer = other.buffer;
			indices = other.indices;
			indexCapacity = other.indexCapacity;
			vertexCapacity = other.vertexCapacity;
			bufferPos = other.bufferPos;
			vao = other.vao;
			vbo = other.vbo;
			ebo = other.ebo;

			other.buffer = nullptr;
			other.indices = nullptr;
			other.vao = 0;
			other.vbo = 0;
			other.ebo = 0;
			other.indexCapacity = 0;
			other.vertexCapacity = 0;
			other.bufferPos = 0;
		}
		return *this;
	}

	void VertexBuffer::setup()
	{
		buffer = new Vertex[vertexCapacity];
		indices = new int[indexCapacity];

		size_t offset = 0;
		for (size_t index = 0; index < indexCapacity; index += 6)
		{
			indices[index + 0] = offset + 0;
			indices[index + 1] = offset + 1;
			indices[index + 2] = offset + 2;

			indices[index + 3] = offset + 0;
			indices[index + 4] = offset + 2;
			indices[index + 5] = offset + 3;

			offset += 4;
		}

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertexCapacity * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCapacity * sizeof(unsigned int), indices,
		             GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		                      (void*)offsetof(Vertex, position));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		                      (void*)offsetof(Vertex, color));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		                      (void*)offsetof(Vertex, uv));

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		                      (void*)(offsetof(Vertex, uv) + offsetof(DirectX::XMFLOAT4, z)));
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(DirectX::XMVECTOR));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void VertexBuffer::dispose()
	{
		delete[] buffer;
		delete[] indices;
		buffer = nullptr;
		indices = nullptr;

		if (vao)
			glDeleteVertexArrays(1, &vao);
		vao = 0;
		if (vbo)
			glDeleteBuffers(1, &vbo);
		vbo = 0;
		if (ebo)
			glDeleteBuffers(1, &ebo);
		ebo = 0;
	}

	void VertexBuffer::bind() const
	{
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	}

	int VertexBuffer::getSize() const { return bufferPos * sizeof(Vertex); }

	int VertexBuffer::getCapacity() const { return vertexCapacity; }

	size_t VertexBuffer::pushBuffer(const Vertex* vertices, size_t count)
	{
		assert(count % 4 == 0 && "Only quads are supported");
		size_t copyCount = size_t(vertexCapacity) - (bufferPos / 4);
		copyCount *= 4;
		copyCount = std::min(copyCount, count);
		std::copy_n(vertices, copyCount, buffer + bufferPos);
		bufferPos += copyCount;
		return copyCount;
	}

	void VertexBuffer::resetBufferPos() { bufferPos = 0; }

	void VertexBuffer::uploadBuffer()
	{
		size_t size = getSize();
		glBufferSubData(GL_ARRAY_BUFFER, 0, size, buffer);
	}

	void VertexBuffer::flushBuffer()
	{
		size_t numIndices = (bufferPos / 4) * 6;
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
	}
}
