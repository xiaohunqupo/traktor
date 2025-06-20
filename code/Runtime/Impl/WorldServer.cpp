/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Runtime/Impl/WorldServer.h"

#include "Core/Log/Log.h"
#include "Core/Misc/ObjectStore.h"
#include "Core/Settings/PropertyBoolean.h"
#include "Core/Settings/PropertyFloat.h"
#include "Core/Settings/PropertyGroup.h"
#include "Core/Settings/PropertyInteger.h"
#include "Core/Settings/PropertyString.h"
#include "Physics/PhysicsManager.h"
#include "Render/IRenderSystem.h"
#include "Render/IRenderView.h"
#include "Resource/IResourceManager.h"
#include "Runtime/IEnvironment.h"
#include "Runtime/Impl/AudioServer.h"
#include "Runtime/Impl/PhysicsServer.h"
#include "Runtime/Impl/RenderServer.h"
#include "Runtime/Impl/ScriptServer.h"
#include "Scene/SceneFactory.h"
#include "Sound/Player/SoundPlayer.h"
#include "Spray/EffectRenderer.h"
#include "Terrain/EntityRenderer.h"
#include "World/Entity/ProbeRenderer.h"
#include "World/EntityFactory.h"
#include "World/IWorldRenderer.h"
#include "World/WorldEntityRenderers.h"
#include "World/WorldResourceFactory.h"

namespace traktor::runtime
{
namespace
{

const float c_sprayLodDistances[][2] = {
	{ 0.0f, 0.0f },
	{ 10.0f, 20.0f },
	{ 40.0f, 90.0f },
	{ 60.0f, 140.0f },
	{ 100.0f, 200.0f }
};

const float c_terrainDetailDistances[] = {
	0.0f,
	10.0f,
	30.0f,
	100.0f,
	200.0f
};

const uint32_t c_terrainSurfaceCacheSizes[] = {
	0,
	2048,
	4096,
	8192,
	8192
};

}

T_IMPLEMENT_RTTI_CLASS(L"traktor.runtime.WorldServer", WorldServer, IWorldServer)

bool WorldServer::create(const PropertyGroup* defaultSettings, const PropertyGroup* settings, IRenderServer* renderServer, IResourceServer* resourceServer)
{
	const std::wstring worldType = defaultSettings->getProperty< std::wstring >(L"World.Type");

	m_worldType = TypeInfo::find(worldType.c_str());
	if (!m_worldType)
	{
		log::error << L"Unable to create world server; no such type \"" << worldType << L"\"" << Endl;
		return false;
	}

	m_motionBlurQuality = (world::Quality)settings->getProperty< int32_t >(L"World.MotionBlurQuality", (int32_t)world::Quality::Medium);
	m_shadowQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ShadowQuality", (int32_t)world::Quality::Medium);
	m_reflectionsQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ReflectionsQuality", (int32_t)world::Quality::Medium);
	m_ambientOcclusionQuality = (world::Quality)settings->getProperty< int32_t >(L"World.AmbientOcclusionQuality", (int32_t)world::Quality::Medium);
	m_antiAliasQuality = (world::Quality)settings->getProperty< int32_t >(L"World.AntiAliasQuality", (int32_t)world::Quality::Medium);
	m_imageProcessQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ImageProcessQuality", (int32_t)world::Quality::Medium);
	m_particleQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ParticleQuality", (int32_t)world::Quality::Medium);
	m_terrainQuality = (world::Quality)settings->getProperty< int32_t >(L"World.TerrainQuality", (int32_t)world::Quality::Medium);
	m_gamma = settings->getProperty< float >(L"World.Gamma", 2.2f);

	m_renderServer = renderServer;
	m_resourceServer = resourceServer;
	m_entityFactory = new world::EntityFactory();
	m_entityRenderers = new world::WorldEntityRenderers();

	return true;
}

void WorldServer::destroy()
{
	m_entityFactory = nullptr;
	m_entityRenderers = nullptr;
	m_renderServer = nullptr;
	m_resourceServer = nullptr;
	m_effectEntityRenderer = nullptr;
	m_terrainEntityRenderer = nullptr;
	m_worldType = nullptr;
}

void WorldServer::createResourceFactories(IEnvironment* environment)
{
	resource::IResourceManager* resourceManager = environment->getResource()->getResourceManager();
	render::IRenderSystem* renderSystem = environment->getRender()->getRenderSystem();

	resourceManager->addFactory(new scene::SceneFactory(renderSystem, m_entityFactory));
	resourceManager->addFactory(new world::WorldResourceFactory(renderSystem, m_entityFactory));
}

void WorldServer::createEntityFactories(IEnvironment* environment)
{
	// Setup object store with relevant systems.
	ObjectStore objectStore;

	if (environment->getPhysics())
		objectStore.set(environment->getPhysics()->getPhysicsManager());
	if (environment->getAudio())
		objectStore.set(environment->getAudio()->getSoundPlayer());

	objectStore.set(environment->getRender()->getRenderSystem());
	objectStore.set(environment->getResource()->getResourceManager());

	// Create instances of all entity factories.
	const TypeInfoSet entityFactoryTypes = type_of< world::IEntityFactory >().findAllOf(false);
	for (const auto& entityFactoryType : entityFactoryTypes)
	{
		if (!entityFactoryType->isInstantiable())
			continue;

		Ref< world::IEntityFactory > entityFactory = dynamic_type_cast< world::IEntityFactory* >(entityFactoryType->createInstance());
		if (!entityFactory)
			continue;

		if (!entityFactory->initialize(objectStore))
			continue;

		m_entityFactory->addFactory(entityFactory);
		T_DEBUG(L"Entity factory \"" << type_name(entityFactory) << L"\" initialized.");
	}
}

void WorldServer::createEntityRenderers(IEnvironment* environment)
{
	render::IRenderSystem* renderSystem = environment->getRender()->getRenderSystem();
	resource::IResourceManager* resourceManager = environment->getResource()->getResourceManager();

	const float sprayLod1Distance = c_sprayLodDistances[(int32_t)m_particleQuality][0];
	const float sprayLod2Distance = c_sprayLodDistances[(int32_t)m_particleQuality][1];
	m_effectEntityRenderer = new spray::EffectRenderer(renderSystem, sprayLod1Distance, sprayLod2Distance);

	m_terrainEntityRenderer = new terrain::EntityRenderer(
		c_terrainDetailDistances[(int32_t)m_terrainQuality],
		c_terrainSurfaceCacheSizes[(int32_t)m_terrainQuality]);

	m_entityRenderers->add(new world::ProbeRenderer(resourceManager, renderSystem, *m_worldType));
	m_entityRenderers->add(m_effectEntityRenderer);
	m_entityRenderers->add(m_terrainEntityRenderer);

	// Setup object store with relevant systems.
	ObjectStore objectStore;
	objectStore.set(environment->getRender()->getRenderSystem());
	objectStore.set(environment->getResource()->getResourceManager());

	// Create instances of all entity renderers.
	const TypeInfoSet entityRendererTypes = type_of< world::IEntityRenderer >().findAllOf(false);
	for (const auto& entityRendererType : entityRendererTypes)
	{
		if (!entityRendererType->isInstantiable())
			continue;

		Ref< world::IEntityRenderer > entityRenderer = dynamic_type_cast< world::IEntityRenderer* >(entityRendererType->createInstance());
		if (!entityRenderer)
			continue;

		if (!entityRenderer->initialize(objectStore))
			continue;

		m_entityRenderers->add(entityRenderer);
		T_DEBUG(L"Entity renderer \"" << type_name(entityRenderer) << L"\" initialized.");
	}
}

int32_t WorldServer::reconfigure(const PropertyGroup* settings)
{
	const world::Quality motionBlurQuality = (world::Quality)settings->getProperty< int32_t >(L"World.MotionBlurQuality", (int32_t)world::Quality::Medium);
	const world::Quality shadowQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ShadowQuality", (int32_t)world::Quality::Medium);
	const world::Quality reflectionsQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ReflectionsQuality", (int32_t)world::Quality::Medium);
	const world::Quality ambientOcclusionQuality = (world::Quality)settings->getProperty< int32_t >(L"World.AmbientOcclusionQuality", (int32_t)world::Quality::Medium);
	const world::Quality antiAliasQuality = (world::Quality)settings->getProperty< int32_t >(L"World.AntiAliasQuality", (int32_t)world::Quality::Medium);
	const world::Quality imageProcessQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ImageProcessQuality", (int32_t)world::Quality::Medium);
	const world::Quality particleQuality = (world::Quality)settings->getProperty< int32_t >(L"World.ParticleQuality", (int32_t)world::Quality::Medium);
	const world::Quality terrainQuality = (world::Quality)settings->getProperty< int32_t >(L"World.TerrainQuality", (int32_t)world::Quality::Medium);
	const float gamma = settings->getProperty< float >(L"World.Gamma", 2.2f);

	// Check if we need to be reconfigured.
	if (
		motionBlurQuality == m_motionBlurQuality &&
		shadowQuality == m_shadowQuality &&
		reflectionsQuality == m_reflectionsQuality &&
		ambientOcclusionQuality == m_ambientOcclusionQuality &&
		antiAliasQuality == m_antiAliasQuality &&
		imageProcessQuality == m_imageProcessQuality &&
		particleQuality == m_particleQuality &&
		terrainQuality == m_terrainQuality &&
		gamma == m_gamma)
		return CrUnaffected;

	// Adjust in-place systems.
	const float sprayLod1Distance = c_sprayLodDistances[(int32_t)m_particleQuality][0];
	const float sprayLod2Distance = c_sprayLodDistances[(int32_t)m_particleQuality][1];
	m_effectEntityRenderer->setLodDistances(sprayLod1Distance, sprayLod2Distance);

	m_terrainEntityRenderer->setTerrainDetailDistance(c_terrainDetailDistances[(int32_t)terrainQuality]);
	m_terrainEntityRenderer->setTerrainCacheSize(c_terrainSurfaceCacheSizes[(int32_t)terrainQuality]);

	// Save ghost configuration state.
	m_motionBlurQuality = motionBlurQuality;
	m_shadowQuality = shadowQuality;
	m_reflectionsQuality = reflectionsQuality;
	m_ambientOcclusionQuality = ambientOcclusionQuality;
	m_antiAliasQuality = antiAliasQuality;
	m_imageProcessQuality = imageProcessQuality;
	m_terrainQuality = terrainQuality;
	m_gamma = gamma;

	return CrAccepted;
}

void WorldServer::addEntityFactory(world::IEntityFactory* entityFactory)
{
	m_entityFactory->addFactory(entityFactory);
}

void WorldServer::removeEntityFactory(world::IEntityFactory* entityFactory)
{
	m_entityFactory->removeFactory(entityFactory);
}

void WorldServer::addEntityRenderer(world::IEntityRenderer* entityRenderer)
{
	m_entityRenderers->add(entityRenderer);
}

void WorldServer::removeEntityRenderer(world::IEntityRenderer* entityRenderer)
{
	m_entityRenderers->remove(entityRenderer);
}

world::IEntityFactory* WorldServer::getEntityFactory()
{
	return m_entityFactory;
}

world::WorldEntityRenderers* WorldServer::getEntityRenderers()
{
	return m_entityRenderers;
}

Ref< world::IWorldRenderer > WorldServer::createWorldRenderer(const world::WorldRenderSettings* worldRenderSettings)
{
	world::WorldCreateDesc wcd;
	wcd.worldRenderSettings = worldRenderSettings;
	wcd.entityRenderers = m_entityRenderers;
	wcd.quality.motionBlur = m_motionBlurQuality;
	wcd.quality.shadows = m_shadowQuality;
	wcd.quality.reflections = m_reflectionsQuality;
	wcd.quality.ambientOcclusion = m_ambientOcclusionQuality;
	wcd.quality.antiAlias = m_antiAliasQuality;
	wcd.quality.imageProcess = m_imageProcessQuality;
	wcd.multiSample = m_renderServer->getMultiSample();
	wcd.gamma = m_gamma;
	wcd.hdr = m_renderServer->getRenderView()->isHDR();

	Ref< world::IWorldRenderer > worldRenderer = dynamic_type_cast< world::IWorldRenderer* >(m_worldType->createInstance());
	if (!worldRenderer)
		return nullptr;

	if (!worldRenderer->create(
			m_resourceServer->getResourceManager(),
			m_renderServer->getRenderSystem(),
			wcd))
		return nullptr;

	return worldRenderer;
}

}
