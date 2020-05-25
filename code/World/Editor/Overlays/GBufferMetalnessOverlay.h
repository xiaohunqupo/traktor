#pragma once

#include "Resource/Proxy.h"
#include "World/Editor/IDebugOverlay.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_WORLD_EDITOR_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor
{
    namespace render
    {

class Shader;

    }

    namespace world
    {

class T_DLLCLASS GBufferMetalnessOverlay : public IDebugOverlay
{
    T_RTTI_CLASS;

public:
    virtual bool create(resource::IResourceManager* resourceManager) override final;

    virtual void setup(render::RenderGraph& renderGraph, render::ScreenRenderer* screenRenderer, IWorldRenderer* worldRenderer, const WorldRenderView& worldRenderView, float alpha) const override final;

private:
    resource::Proxy< render::Shader > m_shader;
};

    }
}