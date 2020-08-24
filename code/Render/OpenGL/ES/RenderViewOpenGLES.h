#pragma once

#include <list>
#include <stack>
#include "Render/IRenderView.h"
#include "Render/OpenGL/ES/Platform.h"

#if defined(_WIN32)
#	include "Render/OpenGL/ES/Win32/Window.h"
#elif defined(__LINUX__)
#	include "Render/OpenGL/ES/Linux/Window.h"
#endif

namespace traktor
{
	namespace render
	{

class ContextOpenGLES;
class VertexBufferOpenGLES;
class IndexBufferOpenGLES;
class ProgramOpenGLES;
class RenderTargetSetOpenGLES;
class RenderTargetOpenGLES;
class StateCache;

/*!
 * \ingroup OGL
 */
class RenderViewOpenGLES
:	public IRenderView
#if defined(_WIN32)
,	public IWindowListener
#endif
{
	T_RTTI_CLASS;

public:
	RenderViewOpenGLES(
		ContextOpenGLES* context
	);

	virtual ~RenderViewOpenGLES();

	virtual bool nextEvent(RenderEvent& outEvent) override final;

	virtual void close() override final;

	virtual bool reset(const RenderViewDefaultDesc& desc) override final;

	virtual bool reset(int32_t width, int32_t height) override final;

	virtual int getWidth() const override final;

	virtual int getHeight() const override final;

	virtual bool isActive() const override final;

	virtual bool isMinimized() const override final;

	virtual bool isFullScreen() const override final;

	virtual void showCursor() override final;

	virtual void hideCursor() override final;

	virtual bool isCursorVisible() const override final;

	virtual bool setGamma(float gamma) override final;

	virtual void setViewport(const Viewport& viewport) override final;

	virtual SystemWindow getSystemWindow() override final;

	virtual bool beginFrame() override final;

	virtual void endFrame() override final;

	virtual void present() override final;

	virtual bool beginPass(const Clear* clear) override final;

	virtual bool beginPass(IRenderTargetSet* renderTargetSet, const Clear* clear, uint32_t load, uint32_t store) override final;

	virtual bool beginPass(IRenderTargetSet* renderTargetSet, int32_t renderTarget, const Clear* clear, uint32_t load, uint32_t store) override final;

	virtual void endPass() override final;

	virtual void draw(VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer, IProgram* program, const Primitives& primitives) override final;

	virtual void draw(VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer, IProgram* program, const Primitives& primitives, uint32_t instanceCount) override final;

	virtual void compute(IProgram* program, const int32_t* workSize) override final;

	virtual bool copy(ITexture* destinationTexture, const Region& destinationRegion, ITexture* sourceTexture, const Region& sourceRegion) override final;

	virtual int32_t beginTimeQuery() override final;

	virtual void endTimeQuery(int32_t query) override final;

	virtual bool getTimeQuery(int32_t query, bool wait, double& outDuration) const override final;

	virtual void pushMarker(const char* const marker) override final;

	virtual void popMarker() override final;

	virtual void getStatistics(RenderViewStatistics& outStatistics) const override final;

private:
	struct RenderTargetStack
	{
		RenderTargetSetOpenGLES* renderTargetSet;
		int32_t renderTarget;
		Viewport viewport;
	};

	Ref< ContextOpenGLES > m_context;
	Ref< StateCache > m_stateCache;
	std::stack< RenderTargetStack > m_renderTargetStack;
	Viewport m_viewport;
	int32_t m_width;
	int32_t m_height;
	bool m_cursorVisible;
	std::list< RenderEvent > m_eventQueue;
	uint32_t m_drawCalls;
	uint32_t m_primitiveCount;

#if defined(_WIN32)
	bool windowListenerEvent(Window* window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& outResult);
#endif
};

	}
}