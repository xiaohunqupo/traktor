#include <cstring>
#include "Core/Log/Log.h"
#include "Core/Math/Log2.h"
#include "Core/Misc/SafeDestroy.h"
#include "Render/OpenGL/ES/Platform.h"
#include "Render/OpenGL/ES/ExtensionsGLES.h"
#include "Render/OpenGL/ES/RenderTargetDepthOpenGLES.h"
#include "Render/OpenGL/ES/RenderTargetOpenGLES.h"
#include "Render/OpenGL/ES/RenderTargetSetOpenGLES.h"
#if defined(__ANDROID__)
#	include "Render/OpenGL/ES/Android/ContextOpenGLES.h"
#elif defined(__IOS__)
#	include "Render/OpenGL/ES/iOS/ContextOpenGLES.h"
#elif defined(__EMSCRIPTEN__)
#	include "Render/OpenGL/ES/Emscripten/ContextOpenGLES.h"
#elif defined(__PNACL__)
#	include "Render/OpenGL/ES/PNaCl/ContextOpenGLES.h"
#elif defined(_WIN32)
#	include "Render/OpenGL/ES/Win32/ContextOpenGLES.h"
#elif defined(__LINUX__) || defined(__RPI__)
#	include "Render/OpenGL/ES/Linux/ContextOpenGLES.h"
#endif

#if !defined(GL_RED_EXT)
#	define GL_RED_EXT 0x1903
#endif

namespace traktor
{
	namespace render
	{
		namespace
		{

struct DeleteFramebufferCallback : public ContextOpenGLES::IDeleteCallback
{
	GLuint m_framebufferName;

	DeleteFramebufferCallback(GLuint framebufferName)
	:	m_framebufferName(framebufferName)
	{
	}

	virtual ~DeleteFramebufferCallback()
	{
	}

	virtual void deleteResource()
	{
		T_OGL_SAFE(glDeleteFramebuffers(1, &m_framebufferName));
		delete this;
	}
};

bool haveExtension(const char* extension)
{
	int32_t extensionLength = strlen(extension);
	const char* supported = (const char*)glGetString(GL_EXTENSIONS);
	while (supported && *supported)
	{
		const char* end = supported;
		while (*end && *end != ' ')
		{
			if ((++end - supported) >= 200)
				break;
		}

		int32_t length = end - supported;
		if (
			length == extensionLength &&
			strncmp(supported, extension, length) == 0
		)
			return true;

		supported = end;
		while (*supported == ' ')
		{
			if ((++supported - end) >= 10)
				break;
		}
	}
	return false;
}

		}

T_IMPLEMENT_RTTI_CLASS(L"traktor.render.RenderTargetSetOpenGLES", RenderTargetSetOpenGLES, IRenderTargetSet)

RenderTargetSetOpenGLES::RenderTargetSetOpenGLES(ContextOpenGLES* context)
:	m_context(context)
,	m_depthBufferOrTexture(0)
,	m_contentValid(false)
{
	m_targetFBO[0] = 0;
}

RenderTargetSetOpenGLES::~RenderTargetSetOpenGLES()
{
	destroy();
}

bool RenderTargetSetOpenGLES::create(const RenderTargetSetCreateDesc& desc, IRenderTargetSet* sharedDepthStencil)
{
	m_desc = desc;

	// Create depth/stencil buffer.
	if (desc.createDepthStencil)
	{
		if (!desc.usingDepthStencilAsTexture)
		{
			T_OGL_SAFE(glGenRenderbuffers(1, &m_depthBufferOrTexture));
			T_OGL_SAFE(glBindRenderbuffer(GL_RENDERBUFFER, m_depthBufferOrTexture));

			GLenum format = GL_DEPTH_COMPONENT16;
#if GL_OES_packed_depth_stencil
			if (!m_desc.ignoreStencil)
				format = GL_DEPTH24_STENCIL8_OES;
#endif

			T_OGL_SAFE(glRenderbufferStorage(
				GL_RENDERBUFFER,
				format,
				desc.width,
				desc.height
			));
		}
		else
		{
			T_OGL_SAFE(glActiveTexture(GL_TEXTURE0));

			T_OGL_SAFE(glGenTextures(1, &m_depthBufferOrTexture));
			T_OGL_SAFE(glBindTexture(GL_TEXTURE_2D, m_depthBufferOrTexture));

			T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

			GLenum format = GL_DEPTH_COMPONENT16;
#if GL_OES_packed_depth_stencil
			if (!m_desc.ignoreStencil)
				format = GL_DEPTH24_STENCIL8_OES;
#endif
			T_OGL_SAFE(glTexImage2D(
				GL_TEXTURE_2D,
				0,
				format,
				m_desc.width,
				m_desc.height,
				0,
				format,
				GL_FLOAT,
				NULL
			));
		}

		m_depthTarget = new RenderTargetDepthOpenGLES(
			m_context,
			m_depthBufferOrTexture,
			m_desc.width,
			m_desc.height
		);
	}
	else if (sharedDepthStencil)
	{
		m_depthTarget = dynamic_type_cast< RenderTargetDepthOpenGLES* >(sharedDepthStencil->getDepthTexture());
	}

	// Create color targets.
	T_ASSERT(desc.count < sizeof_array(m_targetTextures));
	T_OGL_SAFE(glGenTextures(desc.count, m_targetTextures));

	for (int32_t i = 0; i < desc.count; ++i)
	{
		GLenum internalFormat;
		GLint format;
		GLenum type;

		switch (desc.targets[i].format)
		{
		case TfR8:
#if defined(GL_RED_EXT)
			if (haveExtension("GL_EXT_texture_rg"))
			{
				internalFormat = GL_RED_EXT;
				format = GL_RED_EXT;
				type = GL_UNSIGNED_BYTE;
			}
			else
			{
				log::warning << L"Extension \"GL_EXT_texture_rg\" not supported; using different format which may cause performance issues (TfR8 requested)." << Endl;
				internalFormat = GL_RGBA;
				format = GL_RGBA;
				type = GL_UNSIGNED_BYTE;
			}
#else
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
#endif
			break;

		case TfR8G8B8A8:
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;

		case TfR5G6B5:
		case TfR5G5B5A1:
		case TfR4G4B4A4:
		case TfR10G10B10A2:
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;

		case TfR11G11B10F:
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;

		case TfR16G16B16A16F:
#if defined(GL_HALF_FLOAT_OES)
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_HALF_FLOAT_OES;
			break;
#endif

		case TfR32G32B32A32F:
			internalFormat = GL_RGBA;
			format = GL_RGBA;
			type = GL_FLOAT;
			break;

#if !defined(__RPI__) && !defined(__IOS__)
		case TfR16G16F:
#	if defined(GL_HALF_FLOAT_OES)
			internalFormat = GL_RG16_EXT;
			format = GL_RGBA;
			type = GL_HALF_FLOAT_OES;
			break;
#	endif

		case TfR32G32F:
			internalFormat = GL_RG16_EXT;
			format = GL_RGBA;
			type = GL_FLOAT;
			break;
#endif

		case TfR16F:
			if (haveExtension("GL_EXT_texture_rg"))
			{
				internalFormat = GL_RED_EXT;
				format = GL_RED_EXT;
			}
			else
			{
				log::warning << L"Extension \"GL_EXT_texture_rg\" not supported; using different format which may cause performance issues (TfR16F requested)." << Endl;
				internalFormat = GL_RGBA;
				format = GL_RGBA;
			}
			if (haveExtension("GL_OES_texture_half_float"))
				type = GL_HALF_FLOAT_OES;
			else
			{
				log::warning << L"Extension \"GL_OES_texture_half_float\" not supported; using different format (TfR32F) which may cause performance issues (TfR16F requested)." << Endl;
				type = GL_FLOAT;
			}
			break;

		case TfR32F:
			if (haveExtension("GL_EXT_texture_rg"))
			{
				internalFormat = GL_RED_EXT;
				format = GL_RED_EXT;
				type = GL_FLOAT;
			}
			else
			{
				log::warning << L"Extension \"GL_EXT_texture_rg\" not supported; using different format which may cause performance issues (TfR32F requested)." << Endl;
				internalFormat = GL_RGBA;
				format = GL_RGBA;
				type = GL_FLOAT;
			}
			break;

		default:
			log::error << L"Unable to create render target, unsupported format " << int32_t(desc.targets[i].format) << Endl;
			return false;
		}

		T_OGL_SAFE(glActiveTexture(GL_TEXTURE0));
		T_OGL_SAFE(glBindTexture(GL_TEXTURE_2D, m_targetTextures[i]));

		T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		T_OGL_SAFE(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

		T_OGL_SAFE(glTexImage2D(
			GL_TEXTURE_2D,
			0,
			internalFormat,
			m_desc.width,
			m_desc.height,
			0,
			format,
			type,
			NULL
		));

		T_OGL_SAFE(glBindTexture(GL_TEXTURE_2D, 0));

		m_renderTargets[i] = new RenderTargetOpenGLES(
			m_context,
			m_targetTextures[i],
			m_desc.width,
			m_desc.height
		);
	}

	return true;
}

void RenderTargetSetOpenGLES::destroy()
{
	safeDestroy(m_depthTarget);

	for (uint32_t i = 0; i < sizeof_array(m_renderTargets); ++i)
		safeDestroy(m_renderTargets[i]);

	for (int32_t i = 0; i < m_desc.count; ++i)
	{
		if (m_context && m_targetFBO[i])
			m_context->deleteResource(new DeleteFramebufferCallback(m_targetFBO[i]));
		m_targetFBO[i] = 0;
	}

	m_context = 0;
}

int32_t RenderTargetSetOpenGLES::getWidth() const
{
	return m_desc.width;
}

int32_t RenderTargetSetOpenGLES::getHeight() const
{
	return m_desc.height;
}

ISimpleTexture* RenderTargetSetOpenGLES::getColorTexture(int32_t index) const
{
	T_ASSERT(index >= 0);
	T_ASSERT(index < sizeof_array(m_renderTargets));
	return m_renderTargets[index];
}

ISimpleTexture* RenderTargetSetOpenGLES::getDepthTexture() const
{
	return m_depthTarget;
}

bool RenderTargetSetOpenGLES::isContentValid() const
{
	return m_contentValid;
}

bool RenderTargetSetOpenGLES::read(int32_t index, void* buffer) const
{
	return false;
}

bool RenderTargetSetOpenGLES::bind(GLuint primaryDepthBuffer, int32_t renderTarget)
{
	if (!createFramebuffer(primaryDepthBuffer))
		return false;

	T_OGL_SAFE(glBindFramebuffer(GL_FRAMEBUFFER, m_targetFBO[renderTarget]));

#if defined(_DEBUG)
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		log::error << L"Framebuffer incomplete!" << Endl;

		log::info << m_desc.width << L" * " << m_desc.height << Endl;
		log::info << L"createDepthStencil " << m_desc.createDepthStencil << Endl;
		log::info << L"usingPrimaryDepthStencil " << m_desc.usingPrimaryDepthStencil << Endl;

		for (int32_t i = 0; i < m_desc.count; ++i)
		{
			log::info << i << L". " << m_desc.targets[i].format << Endl;
		}

		return false;
	}
#endif

	// Clear target to prevent driver from loading from system memory.
	if (!isContentValid())
	{
		T_OGL_SAFE(glClear(GL_COLOR_BUFFER_BIT));
	}

	return true;
}

bool RenderTargetSetOpenGLES::createFramebuffer(GLuint primaryDepthBuffer)
{
	// Already created?
	if (m_targetFBO[0] != 0)
		return true;

	if (m_desc.count > 0)
	{
		T_OGL_SAFE(glGenFramebuffers(m_desc.count, m_targetFBO));
		for (int32_t i = 0; i < m_desc.count; ++i)
		{
			T_OGL_SAFE(glBindFramebuffer(GL_FRAMEBUFFER, m_targetFBO[i]));

			// Create depth/stencil buffer.
			if (m_desc.createDepthStencil && !m_desc.usingPrimaryDepthStencil)
			{
				if (!m_desc.usingDepthStencilAsTexture)
				{
					T_OGL_SAFE(glFramebufferRenderbuffer(
						GL_FRAMEBUFFER,
						GL_DEPTH_ATTACHMENT,
						GL_RENDERBUFFER,
						m_depthBufferOrTexture
					));

					if (!m_desc.ignoreStencil)
					{
						T_OGL_SAFE(glFramebufferRenderbuffer(
							GL_FRAMEBUFFER,
							GL_STENCIL_ATTACHMENT,
							GL_RENDERBUFFER,
							m_depthBufferOrTexture
						));
					}
				}
				else
				{
					T_OGL_SAFE(glFramebufferTexture2D(
						GL_FRAMEBUFFER,
						GL_DEPTH_ATTACHMENT,
						GL_TEXTURE_2D,
						m_depthBufferOrTexture,
						0
					));
				}
			}
			else if (m_desc.usingPrimaryDepthStencil)
			{
				T_OGL_SAFE(glFramebufferRenderbuffer(
					GL_FRAMEBUFFER,
					GL_DEPTH_ATTACHMENT,
					GL_RENDERBUFFER,
					primaryDepthBuffer
				));
				if (!m_desc.ignoreStencil)
				{
					T_OGL_SAFE(glFramebufferRenderbuffer(
						GL_FRAMEBUFFER,
						GL_STENCIL_ATTACHMENT,
						GL_RENDERBUFFER,
						primaryDepthBuffer
					));
				}
			}

			// Attach color target.
			T_OGL_SAFE(glFramebufferTexture2D(
				GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				m_targetTextures[i],
				0
			));
		}
	}
	else
	{
		// No color targets, just plain depth.
		T_OGL_SAFE(glGenFramebuffers(1, m_targetFBO));
		T_OGL_SAFE(glBindFramebuffer(GL_FRAMEBUFFER, m_targetFBO[0]));

		// Create depth/stencil buffer.
		if (m_desc.createDepthStencil && !m_desc.usingPrimaryDepthStencil)
		{
			if (!m_desc.usingDepthStencilAsTexture)
			{
				T_OGL_SAFE(glFramebufferRenderbuffer(
					GL_FRAMEBUFFER,
					GL_DEPTH_ATTACHMENT,
					GL_RENDERBUFFER,
					m_depthBufferOrTexture
				));

				if (!m_desc.ignoreStencil)
				{
					T_OGL_SAFE(glFramebufferRenderbuffer(
						GL_FRAMEBUFFER,
						GL_STENCIL_ATTACHMENT,
						GL_RENDERBUFFER,
						m_depthBufferOrTexture
					));
				}
			}
			else
			{
				T_OGL_SAFE(glFramebufferTexture2D(
					GL_FRAMEBUFFER,
					GL_DEPTH_ATTACHMENT,
					GL_TEXTURE_2D,
					m_depthBufferOrTexture,
					0
				));
			}
		}
		else if (m_desc.usingPrimaryDepthStencil)
		{
			T_OGL_SAFE(glFramebufferRenderbuffer(
				GL_FRAMEBUFFER,
				GL_DEPTH_ATTACHMENT,
				GL_RENDERBUFFER,
				primaryDepthBuffer
			));
			if (!m_desc.ignoreStencil)
			{
				T_OGL_SAFE(glFramebufferRenderbuffer(
					GL_FRAMEBUFFER,
					GL_STENCIL_ATTACHMENT,
					GL_RENDERBUFFER,
					primaryDepthBuffer
				));
			}
		}
	}

	return true;
}

	}
}