#pragma once

#include "Core/Object.h"
#include "Render/OpenGL/ES/Platform.h"
#include "Render/OpenGL/ES/TypesOpenGLES.h"

namespace traktor
{
	namespace render
	{

class StateCache : public Object
{
	T_RTTI_CLASS;

public:
	StateCache();

	void reset();

	void setRenderState(const RenderStateOpenGL& renderState, bool invertCull);

	void setColorMask(uint32_t colorMask);

	void setDepthMask(GLboolean depthMask);

	void setArrayBuffer(GLint arrayBuffer);

	void setElementArrayBuffer(GLint elemArrayBuffer);

	void setVertexArrayObject(GLint vertexArrayObject);

	void setProgram(GLuint program);

private:
	RenderStateOpenGL m_renderState;
	GLint m_arrayBuffer;
	GLint m_elemArrayBuffer;
	GLint m_vertexArrayObject;
	GLuint m_program;
};

	}
}
