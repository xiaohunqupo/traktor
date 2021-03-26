#include "Runtime/IEnvironment.h"
#include "Runtime/Engine/ScreenLayer.h"
#include "Runtime/Engine/ScreenLayerData.h"
#include "Core/Serialization/ISerializer.h"
#include "Render/Shader.h"
#include "Resource/IResourceManager.h"
#include "Resource/Member.h"

namespace traktor
{
	namespace runtime
	{

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.runtime.ScreenLayerData", 0, ScreenLayerData, LayerData)

Ref< Layer > ScreenLayerData::createInstance(Stage* stage, IEnvironment* environment) const
{
	resource::IResourceManager* resourceManager = environment->getResource()->getResourceManager();
	resource::Proxy< render::Shader > shader;

	// Bind proxies to resource manager.
	if (!resourceManager->bind(m_shader, shader))
		return nullptr;

	// Create layer instance.
	return new ScreenLayer(
		stage,
		m_name,
		m_permitTransition,
		environment,
		shader
	);
}

void ScreenLayerData::serialize(ISerializer& s)
{
	LayerData::serialize(s);
	s >> resource::Member< render::Shader >(L"shader", m_shader);
}

	}
}