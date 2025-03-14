/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Misc/ObjectStore.h"
#include "Core/Io/FileSystem.h"
#include "Database/Database.h"
#include "Physics/PhysicsManager.h"
#include "Render/IRenderSystem.h"
#include "Render/Shader.h"
#include "Resource/IResourceManager.h"
#include "Shape/Editor/EntityFactory.h"
#include "Shape/Editor/Prefab/PrefabComponentData.h"
#include "Shape/Editor/Solid/PrimitiveComponent.h"
#include "Shape/Editor/Solid/PrimitiveComponentData.h"
#include "Shape/Editor/Solid/SolidComponent.h"
#include "Shape/Editor/Solid/SolidComponentData.h"
#include "Shape/Editor/Spline/ControlPointComponent.h"
#include "Shape/Editor/Spline/ControlPointComponentData.h"
#include "Shape/Editor/Spline/SplineComponent.h"
#include "Shape/Editor/Spline/SplineComponentData.h"
#include "Shape/Editor/Spline/SplineLayerComponent.h"
#include "Shape/Editor/Spline/SplineLayerComponentData.h"
#include "World/IEntityBuilder.h"

namespace traktor::shape
{
	namespace
	{
		
const resource::Id< render::Shader > c_defaultShader(Guid(L"{F01DE7F1-64CE-4613-9A17-899B44D5414E}"));

	}

T_IMPLEMENT_RTTI_CLASS(L"traktor.shape.EntityFactory", EntityFactory, world::AbstractEntityFactory)

EntityFactory::EntityFactory(
	const std::wstring& assetPath,
	const std::wstring& modelCachePath
)
:	m_assetPath(assetPath)
,	m_modelCachePath(modelCachePath)
{
}

bool EntityFactory::initialize(const ObjectStore& objectStore)
{
	m_database = objectStore.get< db::Database >();
	m_resourceManager = objectStore.get< resource::IResourceManager >();
	m_renderSystem = objectStore.get< render::IRenderSystem >();
	m_physicsManager = objectStore.get< physics::PhysicsManager >();
	return true;
}

const TypeInfoSet EntityFactory::getEntityTypes() const
{
	return TypeInfoSet();
}

const TypeInfoSet EntityFactory::getEntityComponentTypes() const
{
	return makeTypeInfoSet<
		ControlPointComponentData,
		PrefabComponentData,
		PrimitiveComponentData,
		SolidComponentData,
		SplineComponentData,
		SplineLayerComponentData
	>();
}

Ref< world::Entity > EntityFactory::createEntity(const world::IEntityBuilder* builder, const world::EntityData& entityData) const
{
	return nullptr;
}

Ref< world::IEntityComponent > EntityFactory::createEntityComponent(const world::IEntityBuilder* builder, const world::IEntityComponentData& entityComponentData) const
{
	if (auto controlPointData = dynamic_type_cast< const ControlPointComponentData* >(&entityComponentData))
		return controlPointData->createComponent();
	else if (auto primitiveComponentData = dynamic_type_cast< const PrimitiveComponentData* >(&entityComponentData))
		return primitiveComponentData->createComponent();
	else if (auto solidComponentData = dynamic_type_cast< const SolidComponentData* >(&entityComponentData))
		return solidComponentData->createComponent(builder, m_resourceManager, m_renderSystem);
	else if (auto splineLayerData = dynamic_type_cast< const SplineLayerComponentData* >(&entityComponentData))
	{
		return splineLayerData->createComponent(
			m_database,
			m_modelCachePath,
			m_assetPath
		);
	}
	else if (auto splineComponentData = dynamic_type_cast< const SplineComponentData* >(&entityComponentData))
	{
		resource::Proxy< render::Shader > shader;
		if (!m_resourceManager->bind(c_defaultShader, shader))
			return nullptr;

		return new SplineComponent(
			m_resourceManager,
			m_renderSystem,
			m_physicsManager,
			shader,
			splineComponentData
		);
	}
	else
	{
		// We're ignoring PrefabComponentData since it will not do anything in editor.
		return nullptr;
	}
}

}
