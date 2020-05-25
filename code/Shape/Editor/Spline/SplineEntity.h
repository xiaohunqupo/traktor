#pragma once

#include "Core/Ref.h"
#include "Core/Math/TransformPath.h"
#include "Render/Types.h"
#include "Resource/Proxy.h"
#include "World/Entity/GroupEntity.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_SHAPE_EDITOR_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor
{
	namespace db
	{
	
class Database;

	}

	namespace render
	{

class IndexBuffer;
class IRenderSystem;
class Shader;
class VertexBuffer;

	}

	namespace world
	{

class Entity;
class IWorldRenderPass;
class WorldBuildContext;
class WorldRenderView;

	}

	namespace shape
	{

class SplineEntityData;

/*! Spline entity.
 * \ingroup Shape
 */
class T_DLLCLASS SplineEntity : public world::GroupEntity
{
	T_RTTI_CLASS;

public:
	SplineEntity(
		const SplineEntityData* data,
		db::Database* database,
		render::IRenderSystem* renderSystem,
		const std::wstring& assetPath,
		const resource::Proxy< render::Shader >& shader
	);

	virtual void update(const world::UpdateParams& update) override final;

	void build(
		const world::WorldBuildContext& context,
		const world::WorldRenderView& worldRenderView,
		const world::IWorldRenderPass& worldRenderPass
	);

	const TransformPath& getPath() const { return m_path; }

private:
	struct MaterialBatch
	{
		render::Primitives primitives;
	};

	Ref< const SplineEntityData > m_data;
	Ref< db::Database > m_database;
	Ref< render::IRenderSystem > m_renderSystem;
	std::wstring m_assetPath;
	resource::Proxy< render::Shader > m_shader;

	TransformPath m_path;
	bool m_dirty;

	Ref< render::VertexBuffer > m_vertexBuffer;
	Ref< render::IndexBuffer > m_indexBuffer;
	AlignedVector< MaterialBatch > m_batches;
};

	}
}
