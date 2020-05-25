#pragma once

#include "World/IEntityComponent.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_TERRAIN_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor
{
	namespace world
	{

class IWorldRenderPass;
class WorldBuildContext;
class WorldRenderView;

	}

	namespace terrain
	{

class TerrainComponent;

class T_DLLCLASS TerrainLayerComponent : public world::IEntityComponent
{
	T_RTTI_CLASS;

public:
	TerrainLayerComponent();

	virtual void setOwner(world::Entity* owner) override;

	virtual void update(const world::UpdateParams& update) override;

	virtual void build(
		const world::WorldBuildContext& context,
		const world::WorldRenderView& worldRenderView,
		const world::IWorldRenderPass& worldRenderPass
	) = 0;

	virtual void updatePatches() = 0;

private:
	bool m_dirty;
};

	}
}