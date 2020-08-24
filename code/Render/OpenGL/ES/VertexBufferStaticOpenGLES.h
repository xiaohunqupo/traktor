#pragma once

#include "Core/Containers/AlignedVector.h"
#include "Core/Misc/AutoPtr.h"
#include "Render/OpenGL/ES/VertexBufferOpenGLES.h"

namespace traktor
{
	namespace render
	{

class ContextOpenGLES;
class StateCache;
class VertexElement;

/*!
 * \ingroup OGL
 */
class VertexBufferStaticOpenGLES : public VertexBufferOpenGLES
{
	T_RTTI_CLASS;

public:
	VertexBufferStaticOpenGLES(ContextOpenGLES* context, const AlignedVector< VertexElement >& vertexElements, uint32_t bufferSize);

	virtual ~VertexBufferStaticOpenGLES();

	virtual void destroy() override final;

	virtual void* lock() override final;

	virtual void* lock(uint32_t vertexOffset, uint32_t vertexCount) override final;

	virtual void unlock() override final;

	virtual void activate(StateCache* stateCache) override final;

private:
	struct AttributeDesc
	{
		GLint location;
		GLint size;
		GLenum type;
		GLboolean normalized;
		GLuint offset;
	};

	Ref< ContextOpenGLES > m_context;
	AlignedVector< AttributeDesc > m_attributes;
	GLuint m_arrayObject;
	GLuint m_bufferObject;
	GLuint m_vertexStride;
	AutoPtr< uint8_t, AllocFreeAlign > m_buffer;
	GLintptr m_lockOffset;
	GLsizeiptr m_lockSize;
	bool m_dirty;
};

	}
}
