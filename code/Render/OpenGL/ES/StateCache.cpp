#include "Render/OpenGL/ES/ExtensionsGLES.h"
#include "Render/OpenGL/ES/StateCache.h"

namespace traktor
{
	namespace render
	{

T_IMPLEMENT_RTTI_CLASS(L"traktor.render.StateCache", StateCache, Object)

StateCache::StateCache()
:	m_arrayBuffer(0)
,	m_elemArrayBuffer(0)
,	m_vertexArrayObject(0)
,	m_program(0)
{
}

void StateCache::reset()
{
	m_arrayBuffer = 0;
	m_elemArrayBuffer = 0;
	m_vertexArrayObject = 0;
	m_program = 0;
}

void StateCache::setRenderState(const RenderStateOpenGL& renderState, bool invertCull)
{
	if (renderState.cullFaceEnable)
	{
		if (!m_renderState.cullFaceEnable)
		{
			T_OGL_SAFE(glEnable(GL_CULL_FACE));
			m_renderState.cullFaceEnable = true;
		}
		GLuint cullFace = renderState.cullFace;
		if (invertCull)
		{
			if (cullFace == GL_FRONT)
				cullFace = GL_BACK;
			else
				cullFace = GL_FRONT;
		}
		if (cullFace != m_renderState.cullFace)
		{
			T_OGL_SAFE(glCullFace(cullFace));
			m_renderState.cullFace = cullFace;
		}
	}
	else
	{
		if (m_renderState.cullFaceEnable)
		{
			T_OGL_SAFE(glDisable(GL_CULL_FACE));
			m_renderState.cullFaceEnable = false;
		}
	}

	if (renderState.blendEnable)
	{
		if (!m_renderState.blendEnable)
		{
			T_OGL_SAFE(glEnable(GL_BLEND));
			m_renderState.blendEnable = true;
		}
		if (
			renderState.blendFuncColorSrc != m_renderState.blendFuncColorSrc ||
			renderState.blendFuncColorDest != m_renderState.blendFuncColorDest ||
			renderState.blendFuncAlphaSrc != m_renderState.blendFuncAlphaSrc ||
			renderState.blendFuncAlphaDest != m_renderState.blendFuncAlphaDest
		)
		{
			T_OGL_SAFE(glBlendFuncSeparate(
				renderState.blendFuncColorSrc, renderState.blendFuncColorDest,
				renderState.blendFuncAlphaSrc, renderState.blendFuncAlphaDest
			));
			m_renderState.blendFuncColorSrc = renderState.blendFuncColorSrc;
			m_renderState.blendFuncColorDest = renderState.blendFuncColorDest;
			m_renderState.blendFuncAlphaSrc = renderState.blendFuncAlphaSrc;
			m_renderState.blendFuncAlphaDest = renderState.blendFuncAlphaDest;
		}
		if (
			renderState.blendColorEquation != m_renderState.blendColorEquation ||
			renderState.blendAlphaEquation != m_renderState.blendAlphaEquation
		)
		{
			T_OGL_SAFE(glBlendEquationSeparate(renderState.blendColorEquation, renderState.blendAlphaEquation));
			m_renderState.blendColorEquation = renderState.blendColorEquation;
		}
	}
	else
	{
		if (m_renderState.blendEnable)
		{
			T_OGL_SAFE(glDisable(GL_BLEND));
			m_renderState.blendEnable = false;
		}
	}

	if (renderState.depthTestEnable)
	{
		if (!m_renderState.depthTestEnable || renderState.depthFunc != m_renderState.depthFunc)
		{
			T_OGL_SAFE(glDepthFunc(renderState.depthFunc));
			m_renderState.depthFunc = renderState.depthFunc;
		}
		if (!m_renderState.depthTestEnable)
		{
			T_OGL_SAFE(glEnable(GL_DEPTH_TEST));
			m_renderState.depthTestEnable = true;
		}
	}
	else
	{
		if (m_renderState.depthTestEnable)
		{
			T_OGL_SAFE(glDisable(GL_DEPTH_TEST));
			m_renderState.depthTestEnable = false;
		}
	}

	if (renderState.stencilTestEnable)
	{
		if (!m_renderState.stencilTestEnable)
		{
			T_OGL_SAFE(glEnable(GL_STENCIL_TEST));
			T_OGL_SAFE(glStencilMask(~0UL));
			m_renderState.stencilTestEnable = true;
		}
		if (renderState.stencilOpFail != m_renderState.stencilOpFail || renderState.stencilOpZFail != m_renderState.stencilOpZFail || renderState.stencilOpZPass != m_renderState.stencilOpZPass)
		{
			T_OGL_SAFE(glStencilOp(renderState.stencilOpFail, renderState.stencilOpZFail, renderState.stencilOpZPass));
			m_renderState.stencilOpFail = renderState.stencilOpFail;
			m_renderState.stencilOpZFail = renderState.stencilOpZFail;
			m_renderState.stencilOpZPass = renderState.stencilOpZPass;
		}
		if (renderState.stencilFunc != m_renderState.stencilFunc || renderState.stencilRef != m_renderState.stencilRef)
		{
			T_OGL_SAFE(glStencilFunc(renderState.stencilFunc, renderState.stencilRef, ~0UL));
			m_renderState.stencilFunc = renderState.stencilFunc;
			m_renderState.stencilRef = renderState.stencilRef;
		}
	}
	else
	{
		if (m_renderState.stencilTestEnable)
		{
			T_OGL_SAFE(glDisable(GL_STENCIL_TEST));
			m_renderState.stencilTestEnable = false;
		}
	}

	if (renderState.colorMask != m_renderState.colorMask)
	{
		T_OGL_SAFE(glColorMask(
			(renderState.colorMask & RenderStateOpenGL::CmRed) ? GL_TRUE : GL_FALSE,
			(renderState.colorMask & RenderStateOpenGL::CmGreen) ? GL_TRUE : GL_FALSE,
			(renderState.colorMask & RenderStateOpenGL::CmBlue) ? GL_TRUE : GL_FALSE,
			(renderState.colorMask & RenderStateOpenGL::CmAlpha) ? GL_TRUE : GL_FALSE
		));
		m_renderState.colorMask = renderState.colorMask;
	}

	if (renderState.depthMask != m_renderState.depthMask)
	{
		T_OGL_SAFE(glDepthMask(renderState.depthMask));
		m_renderState.depthMask = renderState.depthMask;
	}
}

void StateCache::setColorMask(uint32_t colorMask)
{
	if (colorMask != m_renderState.colorMask)
	{
		T_OGL_SAFE(glColorMask(
			(colorMask & RenderStateOpenGL::CmRed) ? GL_TRUE : GL_FALSE,
			(colorMask & RenderStateOpenGL::CmGreen) ? GL_TRUE : GL_FALSE,
			(colorMask & RenderStateOpenGL::CmBlue) ? GL_TRUE : GL_FALSE,
			(colorMask & RenderStateOpenGL::CmAlpha) ? GL_TRUE : GL_FALSE
		));
		m_renderState.colorMask = colorMask;
	}
}

void StateCache::setDepthMask(GLboolean depthMask)
{
	if (depthMask != m_renderState.depthMask)
	{
		T_OGL_SAFE(glDepthMask(depthMask));
		m_renderState.depthMask = depthMask;
	}
}

void StateCache::setArrayBuffer(GLint arrayBuffer)
{
	if (m_arrayBuffer != arrayBuffer)
	{
		T_OGL_SAFE(glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer));
		m_arrayBuffer = arrayBuffer;
	}
}

void StateCache::setElementArrayBuffer(GLint elemArrayBuffer)
{
	if (m_elemArrayBuffer != elemArrayBuffer)
	{
		T_OGL_SAFE(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elemArrayBuffer));
		m_elemArrayBuffer = elemArrayBuffer;
	}
}

void StateCache::setVertexArrayObject(GLint vertexArrayObject)
{
#if defined(GL_OES_vertex_array_object) && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
	if (m_vertexArrayObject != vertexArrayObject && g_glBindVertexArrayOES != 0)
	{
		T_OGL_SAFE(g_glBindVertexArrayOES(vertexArrayObject));
		m_vertexArrayObject = vertexArrayObject;
	}
#endif
}

void StateCache::setProgram(GLuint program)
{
	if (m_program != program)
	{
		T_OGL_SAFE(glUseProgram(program));
		m_program = program;
	}
}

	}
}