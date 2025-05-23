/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Terrain/TerrainComponent.h"

#include "Core/Math/Float.h"
#include "Core/Math/MathUtils.h"
#include "Core/Misc/SafeDestroy.h"
#include "Heightfield/Heightfield.h"
#include "Render/Buffer.h"
#include "Render/Context/RenderContext.h"
#include "Render/IRenderSystem.h"
#include "Render/VertexElement.h"
#include "Resource/IResourceManager.h"
#include "Terrain/Terrain.h"
#include "Terrain/TerrainSurfaceCache.h"
#include "World/IWorldRenderPass.h"
#include "World/World.h"
#include "World/WorldBuildContext.h"
#include "World/WorldHandles.h"
#include "World/WorldRenderView.h"
#include "World/WorldSetupContext.h"

#include <algorithm>
#include <limits>

namespace traktor::terrain
{
namespace
{

const resource::Id< render::Shader > c_shaderCull(L"{8BA73DD8-0FD9-4C15-A772-EACC14014AEC}");

const render::Handle c_handleTerrain_VisualizeLods(L"Terrain_VisualizeLods");
const render::Handle c_handleTerrain_VisualizeMap(L"Terrain_VisualizeMap");
const render::Handle c_handleTerrain_SurfaceAlbedo(L"Terrain_SurfaceAlbedo");
const render::Handle c_handleTerrain_SurfaceOffset(L"Terrain_SurfaceOffset");
const render::Handle c_handleTerrain_Heightfield(L"Terrain_Heightfield");
const render::Handle c_handleTerrain_ColorMap(L"Terrain_ColorMap");
const render::Handle c_handleTerrain_SplatMap(L"Terrain_SplatMap");
const render::Handle c_handleTerrain_CutMap(L"Terrain_CutMap");
const render::Handle c_handleTerrain_Normals(L"Terrain_Normals");
const render::Handle c_handleTerrain_ViewProjection(L"Terrain_ViewProjection");
const render::Handle c_handleTerrain_Eye(L"Terrain_Eye");
const render::Handle c_handleTerrain_WorldOrigin(L"Terrain_WorldOrigin");
const render::Handle c_handleTerrain_WorldExtent(L"Terrain_WorldExtent");
const render::Handle c_handleTerrain_PatchExtent(L"Terrain_PatchExtent");
const render::Handle c_handleTerrain_DebugPatchIndex(L"Terrain_DebugPatchIndex");
const render::Handle c_handleTerrain_DebugMap(L"Terrain_DebugMap");
const render::Handle c_handleTerrain_CutEnable(L"Terrain_CutEnable");
const render::Handle c_handleTerrain_PatchData(L"Terrain_PatchData");
const render::Handle c_handleTerrain_TargetSize(L"Terrain_TargetSize");
const render::Handle c_handleTerrain_DrawBuffer(L"Terrain_DrawBuffer");
const render::Handle c_handleTerrain_CulledDrawBuffer(L"Terrain_CulledDrawBuffer");

const int32_t c_patchLodSteps = 3;
const int32_t c_surfaceLodSteps = 3;

struct CullPatch
{
	float error[4];
	float distance;
	float area;
	uint32_t patchId;
	Vector4 patchOrigin;
};

typedef std::pair< float, const TerrainComponent::Patch* > cull_patch_t;

struct DrawData
{
	float patchOrigin[4];
	float surfaceOffset[4];
	float patchBoundingBoxMn[4];
	float patchBoundingBoxMx[4];
};

Ref< render::ITexture > create1x1Texture(render::IRenderSystem* renderSystem, const Color4ub& value)
{
	render::SimpleTextureCreateDesc stcd = {};
	stcd.width = 1;
	stcd.height = 1;
	stcd.mipCount = 1;
	stcd.format = render::TfR8G8B8A8;
	stcd.sRGB = false;
	stcd.immutable = true;
	stcd.initialData[0].data = &value;
	stcd.initialData[0].pitch = 4;
	return renderSystem->createSimpleTexture(stcd, T_FILE_LINE_W);
}

}

T_IMPLEMENT_RTTI_CLASS(L"traktor.terrain.TerrainComponent", TerrainComponent, world::IEntityComponent)

TerrainComponent::TerrainComponent(resource::IResourceManager* resourceManager, render::IRenderSystem* renderSystem)
	: m_resourceManager(resourceManager)
	, m_renderSystem(renderSystem)
{
}

bool TerrainComponent::create(const TerrainComponentData& data)
{
	if (!m_resourceManager->bind(data.getTerrain(), m_terrain))
		return false;

	if (!m_resourceManager->bind(c_shaderCull, m_shaderCull))
		return false;

	m_heightfield = m_terrain->getHeightfield();

	m_patchLodDistance = data.getPatchLodDistance();
	m_patchLodBias = data.getPatchLodBias();
	m_patchLodExponent = data.getPatchLodExponent();
	m_surfaceLodDistance = data.getSurfaceLodDistance();
	m_surfaceLodBias = data.getSurfaceLodBias();
	m_surfaceLodExponent = data.getSurfaceLodExponent();

	if (!createPatches())
		return false;
	if (!createRayTracingPatches())
		return false;

	m_defaultColorMap = create1x1Texture(m_renderSystem, Color4ub(128, 128, 128, 128));
	m_defaultCutMap = create1x1Texture(m_renderSystem, Color4ub(0, 0, 0, 0));

	return true;
}

void TerrainComponent::setup(
	const world::WorldSetupContext& context,
	const world::WorldRenderView& worldRenderView,
	float detailDistance,
	uint32_t cacheSize)
{
	const int32_t viewIndex = worldRenderView.getIndex();
	const bool snapshot = worldRenderView.getSnapshot();

	if (!validate(viewIndex, cacheSize))
		return;

	const Vector4& worldExtent = m_heightfield->getWorldExtent();

	const Matrix44 viewInv = worldRenderView.getView().inverse();
	const Matrix44 viewProj = worldRenderView.getProjection() * worldRenderView.getView();
	const Vector4 eyePosition = worldRenderView.getEyePosition();
	const Vector4 eyeDirection = worldRenderView.getEyeDirection();

	const Vector4 patchExtent(worldExtent.x() / float(m_patchCount), worldExtent.y(), worldExtent.z() / float(m_patchCount), 0.0f);
	const Vector4 patchDeltaHalf = patchExtent * Vector4(0.5f, 0.5f, 0.5f, 0.0f);
	const Vector4 patchDeltaX = patchExtent * Vector4(1.0f, 0.0f, 0.0f, 0.0f);
	const Vector4 patchDeltaZ = patchExtent * Vector4(0.0f, 0.0f, 1.0f, 0.0f);
	Vector4 patchTopLeft = (-worldExtent * Scalar(0.5f)).xyz1();

	// Calculate world frustum.
	const Frustum viewCullFrustum = worldRenderView.getCullFrustum();
	Frustum worldCullFrustum = viewCullFrustum;
	for (uint32_t i = 0; i < worldCullFrustum.planes.size(); ++i)
		worldCullFrustum.planes[i] = viewInv * worldCullFrustum.planes[i];

	// Cull patches to world frustum.
	AlignedVector< CullPatch >& visiblePatches = m_view[viewIndex].visiblePatches;
	visiblePatches.resize(0);
	for (uint32_t pz = 0; pz < m_patchCount; ++pz)
	{
		Vector4 patchOrigin = patchTopLeft;
		for (uint32_t px = 0; px < m_patchCount; ++px)
		{
			const uint32_t patchId = px + pz * m_patchCount;

			const Patch& patch = m_patches[patchId];
			const Vector4 patchCenterWorld = (patchOrigin + patchDeltaHalf) * Vector4(1.0f, 0.0f, 1.0f, 0.0f) + Vector4(0.0f, (patch.minHeight + patch.maxHeight) * 0.5f, 0.0f, 1.0f);

			const Aabb3 patchAabb(
				patchCenterWorld * Vector4(1.0f, 0.0f, 1.0f, 1.0f) + Vector4(-patchDeltaHalf.x(), patch.minHeight, -patchDeltaHalf.z(), 0.0f),
				patchCenterWorld * Vector4(1.0f, 0.0f, 1.0f, 1.0f) + Vector4(patchDeltaHalf.x(), patch.maxHeight, patchDeltaHalf.z(), 0.0f));

			if (worldCullFrustum.inside(patchAabb) != Frustum::Result::Outside)
			{
				const Scalar lodDistance = (patchCenterWorld - eyePosition).xyz0().length();
				const Vector4 patchCenterWorld_x0zw = patchCenterWorld * Vector4(1.0f, 0.0f, 1.0f, 1.0f);
				const Vector4 eyePosition_0y00 = Vector4(0.0f, eyePosition.y(), 0.0f, 0.0f);

				CullPatch cp;

				// Calculate screen error for each lod.
				for (int i = 0; i < LodCount; ++i)
				{
					const Vector4 Pworld[2] = {
						patchCenterWorld_x0zw + eyePosition_0y00,
						patchCenterWorld_x0zw + eyePosition_0y00 + Vector4(0.0f, patch.error[i], 0.0f, 0.0f)
					};

					Vector4 Pview[2] = {
						worldRenderView.getView() * Pworld[0],
						worldRenderView.getView() * Pworld[1]
					};

					if (Pview[0].z() < viewCullFrustum.getNearZ())
						Pview[0].set(2, viewCullFrustum.getNearZ());
					if (Pview[1].z() < viewCullFrustum.getNearZ())
						Pview[1].set(2, viewCullFrustum.getNearZ());

					Vector4 Pclip[] = {
						worldRenderView.getProjection() * Pview[0].xyz1(),
						worldRenderView.getProjection() * Pview[1].xyz1()
					};

					T_ASSERT(Pclip[0].w() > 0.0f);
					T_ASSERT(Pclip[1].w() > 0.0f);

					Pclip[0] /= Pclip[0].w();
					Pclip[1] /= Pclip[1].w();

					const Vector4 d = Pclip[1] - Pclip[0];

					const float dx = d.x();
					const float dy = d.y();

					cp.error[i] = std::sqrt(dx * dx + dy * dy) * 100.0f;
				}

				// Project patch bounding box extents onto view plane and calculate screen area.
				Vector4 extents[8];
				patchAabb.getExtents(extents);

				Vector4 mn(
					std::numeric_limits< float >::max(),
					std::numeric_limits< float >::max(),
					std::numeric_limits< float >::max(),
					std::numeric_limits< float >::max());
				Vector4 mx(
					-std::numeric_limits< float >::max(),
					-std::numeric_limits< float >::max(),
					-std::numeric_limits< float >::max(),
					-std::numeric_limits< float >::max());

				bool clipped = false;
				for (int32_t i = 0; i < sizeof_array(extents); ++i)
				{
					Vector4 p = viewProj * extents[i];
					if (p.w() <= 0.0f)
					{
						clipped = true;
						break;
					}

					// Homogeneous divide.
					p /= p.w();

					// Track screen space extents.
					mn = min(mn, p);
					mx = max(mx, p);
				}

				const Vector4 e = mx - mn;

				cp.distance = lodDistance;
				cp.area = !clipped ? e.x() * e.y() : 1000.0_simd;
				cp.patchId = patchId;
				cp.patchOrigin = patchOrigin;
				cp.patchAabb = patchAabb;

				visiblePatches.push_back(cp);
			}
			else
			{
				ViewPatch& viewPatch = m_view[viewIndex].viewPatches[patchId];
				viewPatch.lastPatchLod = c_patchLodSteps;
				viewPatch.lastSurfaceLod = c_surfaceLodSteps;
			}

			patchOrigin += patchDeltaX;
		}
		patchTopLeft += patchDeltaZ;
	}

	// Sort patches front to back to maximize best use of surface cache and rendering.
	std::sort(visiblePatches.begin(), visiblePatches.end(), [](const CullPatch& lh, const CullPatch& rh) {
		return lh.distance < rh.distance;
	});

	for (uint32_t i = 0; i < LodCount; ++i)
		m_view[viewIndex].patchLodInstances[i].resize(0);

	// Update all patch surfaces.
	for (const auto& visiblePatch : visiblePatches)
	{
		const Patch& patch = m_patches[visiblePatch.patchId];
		const Vector4& patchOrigin = visiblePatch.patchOrigin;

		ViewPatch& viewPatch = m_view[viewIndex].viewPatches[visiblePatch.patchId];

		// Calculate which surface lod to use based one distance to patch center.
		const float distance = max(visiblePatch.distance - detailDistance, 0.0f);
		const float surfaceLodDistance = std::pow(clamp(distance / m_surfaceLodDistance + m_surfaceLodBias, 0.0f, 1.0f), m_surfaceLodExponent);
		const float surfaceLodF = surfaceLodDistance * c_surfaceLodSteps;
		int32_t surfaceLod = int32_t(surfaceLodF + 0.5f);

		const float c_lodHysteresisThreshold = 0.5f;
		if (surfaceLod != viewPatch.lastSurfaceLod)
		{
			if (std::abs(surfaceLodF - viewPatch.lastSurfaceLod) < c_lodHysteresisThreshold)
				surfaceLod = viewPatch.lastSurfaceLod;
		}

		// Find patch lod based on screen space error.
		int32_t patchLod = 0;
		for (int32_t j = 3; j > 0; --j)
		{
			if (visiblePatch.error[j] <= 1.0f)
			{
				patchLod = j;
				break;
			}
		}

		viewPatch.lastPatchLod = patchLod;
		viewPatch.lastSurfaceLod = surfaceLod;
		viewPatch.surfaceOffset = Vector4::zero();

		// Queue patch instance.
		m_view[viewIndex].patchLodInstances[patchLod].push_back(&visiblePatch);
	}

	// Update base color texture.
	if (!snapshot)
		m_surfaceCache->setupBaseColor(
			context.getRenderGraph(),
			m_terrain,
			-worldExtent * 0.5_simd,
			worldExtent);
}

void TerrainComponent::build(
	const world::WorldBuildContext& context,
	const world::WorldRenderView& worldRenderView,
	const world::IWorldRenderPass& worldRenderPass,
	float detailDistance,
	uint32_t cacheSize)
{
	const int32_t viewIndex = worldRenderView.getIndex();
	const bool snapshot = worldRenderView.getSnapshot();

	if (!validate(viewIndex, cacheSize))
		return;

	render::Shader* shader = m_terrain->getTerrainShader();

	auto perm = worldRenderPass.getPermutation(shader);

	shader->setCombination(c_handleTerrain_CutEnable, m_terrain->getCutMap(), perm);

	if (m_visualizeMode >= VmSurfaceLod && m_visualizeMode <= VmPatchLod)
		shader->setCombination(c_handleTerrain_VisualizeLods, true, perm);
	else if (m_visualizeMode >= VmColorMap && m_visualizeMode <= VmCutMap)
		shader->setCombination(c_handleTerrain_VisualizeMap, true, perm);

	render::IProgram* program = shader->getProgram(perm).program;
	if (!program)
		return;

	const Vector4& worldExtent = m_heightfield->getWorldExtent();
	const Vector4 eyePosition = worldRenderView.getEyePosition();
	const Vector4 patchExtent(worldExtent.x() / float(m_patchCount), worldExtent.y(), worldExtent.z() / float(m_patchCount), 0.0f);

	render::RenderContext* renderContext = context.getRenderContext();

	// Update indirect draw buffers; do this only once per frame
	// since all draws are the same for other passes.
	if ((worldRenderPass.getPassFlags() & world::IWorldRenderPass::First) != 0)
	{
		auto draw = (render::IndexedIndirectDraw*)m_drawBuffer->lock();
		auto data = (DrawData*)m_dataBuffer->lock();

		for (const auto& visiblePatch : m_view[viewIndex].visiblePatches)
		{
			const Patch& patch = m_patches[visiblePatch.patchId];
			const Vector4& patchOrigin = visiblePatch.patchOrigin;

			ViewPatch& viewPatch = m_view[viewIndex].viewPatches[visiblePatch.patchId];

			const auto& p = m_primitives[viewPatch.lastPatchLod];

			draw->indexCount = p.getVertexCount();
			draw->instanceCount = 1;
			draw->firstIndex = p.offset;
			draw->vertexOffset = 0;
			draw->firstInstance = 0;
			draw++;

			patchOrigin.storeUnaligned(data->patchOrigin);
			if (!snapshot)
				viewPatch.surfaceOffset.storeUnaligned(data->surfaceOffset);
			else
			{
				const Vector4 snapshotOffset(
					patchOrigin.x() / worldExtent.x() + 0.5f,
					patchOrigin.z() / worldExtent.z() + 0.5f,
					patchExtent.x() / worldExtent.x(),
					patchExtent.z() / worldExtent.z());
				snapshotOffset.storeUnaligned(data->surfaceOffset);
			}

			visiblePatch.patchAabb.mn.storeAligned(data->patchBoundingBoxMn);
			visiblePatch.patchAabb.mx.storeAligned(data->patchBoundingBoxMx);

			data++;
		}

		m_drawBuffer->unlock();
		m_dataBuffer->unlock();
	}

	// Cull draw buffer to HiZ target; only deferred renderer using HiZ culling.
	if (!snapshot && worldRenderPass.getTechnique() == world::ShaderTechnique::DeferredGBufferWrite)
	{
		const Vector2 viewSize = worldRenderView.getViewSize();

		auto renderBlock = renderContext->allocNamed< render::ComputeRenderBlock >(L"Terrain cull");

		renderBlock->program = m_shaderCull->getProgram().program;

		renderBlock->programParams = renderContext->alloc< render::ProgramParameters >();
		renderBlock->programParams->beginParameters(renderContext);

		worldRenderPass.setProgramParameters(renderBlock->programParams);

		renderBlock->programParams->setVectorParameter(c_handleTerrain_TargetSize, Vector4(viewSize.x, viewSize.y, 0.0f, 0.0f));
		renderBlock->programParams->setMatrixParameter(c_handleTerrain_ViewProjection, worldRenderView.getProjection() * worldRenderView.getView());
		renderBlock->programParams->setBufferViewParameter(c_handleTerrain_DrawBuffer, m_drawBuffer->getBufferView());
		renderBlock->programParams->setBufferViewParameter(c_handleTerrain_CulledDrawBuffer, m_culledDrawBuffer->getBufferView());
		renderBlock->programParams->setBufferViewParameter(c_handleTerrain_PatchData, m_dataBuffer->getBufferView());
		renderBlock->programParams->endParameters(renderContext);

		renderBlock->workSize[0] = (int32_t)m_view[viewIndex].visiblePatches.size();

		renderContext->compute(renderBlock);
		renderContext->compute< render::BarrierRenderBlock >(render::Stage::Compute, render::Stage::Indirect, nullptr, 0);
	}

	// Render all patches using indirect draw.
	{
		auto rb = renderContext->allocNamed< render::IndirectRenderBlock >(L"Terrain patches");
		rb->distance = 10000.0f;
		rb->program = program;
		rb->programParams = renderContext->alloc< render::ProgramParameters >();
		rb->indexBuffer = m_indexBuffer->getBufferView();
		rb->indexType = render::IndexType::UInt32;
		rb->vertexBuffer = m_vertexBuffer->getBufferView();
		rb->vertexLayout = m_vertexLayout;
		rb->primitive = render::PrimitiveType::Triangles;

		if (worldRenderPass.getTechnique() == world::ShaderTechnique::DeferredGBufferWrite)
			rb->drawBuffer = m_culledDrawBuffer->getBufferView();
		else
			rb->drawBuffer = m_drawBuffer->getBufferView();

		rb->drawCount = (uint32_t)m_view[viewIndex].visiblePatches.size();

		rb->programParams->beginParameters(renderContext);

		rb->programParams->setTextureParameter(c_handleTerrain_Heightfield, m_terrain->getHeightMap());
		rb->programParams->setTextureParameter(c_handleTerrain_SurfaceAlbedo, m_surfaceCache->getBaseTexture());
		rb->programParams->setTextureParameter(c_handleTerrain_ColorMap, m_terrain->getColorMap() ? m_terrain->getColorMap().getResource() : m_defaultColorMap.ptr());
		rb->programParams->setTextureParameter(c_handleTerrain_Normals, m_terrain->getNormalMap());
		rb->programParams->setTextureParameter(c_handleTerrain_SplatMap, m_terrain->getSplatMap());
		rb->programParams->setTextureParameter(c_handleTerrain_CutMap, m_terrain->getCutMap() ? m_terrain->getCutMap().getResource() : m_defaultCutMap.ptr());
		rb->programParams->setVectorParameter(c_handleTerrain_Eye, eyePosition);
		rb->programParams->setVectorParameter(c_handleTerrain_WorldOrigin, -worldExtent * Scalar(0.5f));
		rb->programParams->setVectorParameter(c_handleTerrain_WorldExtent, worldExtent);
		rb->programParams->setVectorParameter(c_handleTerrain_PatchExtent, patchExtent);

		if (m_visualizeMode == VmColorMap)
			rb->programParams->setTextureParameter(c_handleTerrain_DebugMap, m_terrain->getColorMap());
		else if (m_visualizeMode == VmNormalMap)
			rb->programParams->setTextureParameter(c_handleTerrain_DebugMap, m_terrain->getNormalMap());
		else if (m_visualizeMode == VmHeightMap)
			rb->programParams->setTextureParameter(c_handleTerrain_DebugMap, m_terrain->getHeightMap());
		else if (m_visualizeMode == VmSplatMap)
			rb->programParams->setTextureParameter(c_handleTerrain_DebugMap, m_terrain->getSplatMap());
		else if (m_visualizeMode == VmCutMap)
			rb->programParams->setTextureParameter(c_handleTerrain_DebugMap, m_terrain->getCutMap());

		worldRenderPass.setProgramParameters(rb->programParams);

		rb->programParams->setBufferViewParameter(c_handleTerrain_PatchData, m_dataBuffer->getBufferView());
		rb->programParams->endParameters(renderContext);

		renderContext->draw(render::RenderPriority::Opaque, rb);
	}
}

void TerrainComponent::setVisualizeMode(VisualizeMode visualizeMode)
{
	m_visualizeMode = visualizeMode;
}

void TerrainComponent::destroy()
{
	safeDestroy(m_indexBuffer);
	safeDestroy(m_vertexBuffer);
	safeDestroy(m_drawBuffer);
	safeDestroy(m_dataBuffer);

	// for (auto vb : m_rtVertexBuffers)
	//	vb->destroy();
	// for (auto va : m_rtPerVertexAttributes)
	//	va->destroy();
	// for (auto as : m_rtAS)
	//	as->destroy();

	// m_rtVertexBuffers.clear();
	// m_rtPerVertexAttributes.clear();
	// m_rtAS.clear();

	safeDestroy(m_rtwInstance);
}

void TerrainComponent::setOwner(world::Entity* owner)
{
	m_owner = owner;
}

void TerrainComponent::setWorld(world::World* world)
{
	// Remove from last world.
	safeDestroy(m_rtwInstance);

	// Add to new world.
	if (world != nullptr)
	{
		T_FATAL_ASSERT(m_rtwInstance == nullptr);
		world::RTWorldComponent* rtw = world->getComponent< world::RTWorldComponent >();
		if (rtw != nullptr)
			m_rtwInstance = rtw->createInstance(m_rtParts);
	}

	m_world = world;
}

void TerrainComponent::setTransform(const Transform& transform)
{
}

Aabb3 TerrainComponent::getBoundingBox() const
{
	const Vector4& worldExtent = m_heightfield->getWorldExtent();
	return Aabb3(-worldExtent, worldExtent);
}

void TerrainComponent::update(const world::UpdateParams& update)
{
}

bool TerrainComponent::validate(int32_t viewIndex, uint32_t cacheSize)
{
	const bool cacheSizeChange = (m_surfaceCache == nullptr) || (cacheSize != m_cacheSize);

	if (cacheSizeChange)
	{
		safeDestroy(m_surfaceCache);
		m_surfaceCache = new TerrainSurfaceCache();
		if (!m_surfaceCache->create(m_resourceManager, m_renderSystem, cacheSize))
		{
			m_surfaceCache = nullptr;
			return false;
		}
		m_cacheSize = cacheSize;
	}

	if (
		m_terrain.changed() ||
		m_heightfield.changed() ||
		cacheSizeChange)
	{
		m_heightfield.consume();
		m_terrain.consume();

		if (!createPatches())
			return false;
		if (!createRayTracingPatches())
			return false;
	}

	return true;
}

void TerrainComponent::updatePatches(const uint32_t* region, bool updateErrors)
{
	const uint32_t patchDim = m_terrain->getPatchDim();
	const uint32_t heightfieldSize = m_heightfield->getSize();

	const uint32_t mnx = region ? max< uint32_t >(region[0], 0) : 0;
	const uint32_t mnz = region ? max< uint32_t >(region[1], 0) : 0;
	const uint32_t mxx = region ? min< uint32_t >(region[2] + 1, m_patchCount) : m_patchCount;
	const uint32_t mxz = region ? min< uint32_t >(region[3] + 1, m_patchCount) : m_patchCount;

	for (uint32_t pz = mnz; pz < mxz; ++pz)
	{
		for (uint32_t px = mnx; px < mxx; ++px)
		{
			const uint32_t patchId = px + pz * m_patchCount;

			if (updateErrors)
			{
				const Terrain::Patch& patchData = m_terrain->getPatches()[patchId];
				Patch& patch = m_patches[patchId];
				patch.minHeight = patchData.height[0];
				patch.maxHeight = patchData.height[1];
				patch.error[0] = 0.0f;
				patch.error[1] = patchData.error[0];
				patch.error[2] = patchData.error[1];
				patch.error[3] = patchData.error[2];
			}
		}
	}
}

void TerrainComponent::updateRayTracingPatches()
{
	if (m_renderSystem->supportRayTracing())
		createRayTracingPatches();
}

bool TerrainComponent::createPatches()
{
	m_patches.clear();
	m_patchCount = 0;

	safeDestroy(m_indexBuffer);
	safeDestroy(m_vertexBuffer);
	safeDestroy(m_drawBuffer);
	safeDestroy(m_culledDrawBuffer);
	safeDestroy(m_dataBuffer);

	const uint32_t heightfieldSize = m_heightfield->getSize();
	T_ASSERT(heightfieldSize > 0);

	const uint32_t patchDim = m_terrain->getPatchDim();
	const uint32_t detailSkip = m_terrain->getDetailSkip();

	const uint32_t patchVertexCount = patchDim * patchDim;
	m_patchCount = heightfieldSize / (patchDim * detailSkip);

	AlignedVector< render::VertexElement > vertexElements;
	vertexElements.push_back(render::VertexElement(render::DataUsage::Position, render::DtFloat2, 0));
	const uint32_t vertexSize = render::getVertexSize(vertexElements);

	m_vertexBuffer = m_renderSystem->createBuffer(
		render::BuVertex,
		patchVertexCount * vertexSize,
		false);
	if (!m_vertexBuffer)
		return false;

	float* vertex = static_cast< float* >(m_vertexBuffer->lock());
	T_ASSERT_M(vertex, L"Unable to lock vertex buffer");

	for (uint32_t z = 0; z < patchDim; ++z)
	{
		for (uint32_t x = 0; x < patchDim; ++x)
		{
			*vertex++ = float(x) / (patchDim - 1);
			*vertex++ = float(z) / (patchDim - 1);
		}
	}

	m_vertexBuffer->unlock();

	m_vertexLayout = m_renderSystem->createVertexLayout(vertexElements);

	m_patches.reserve(m_patchCount * m_patchCount);
	for (uint32_t pz = 0; pz < m_patchCount; ++pz)
	{
		for (uint32_t px = 0; px < m_patchCount; ++px)
		{
			m_patches.push_back(
				{ 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } });
			for (int32_t i = 0; i < sizeof_array(m_view); ++i)
				m_view[i].viewPatches.push_back(
					{ c_patchLodSteps, c_surfaceLodSteps, Vector4::zero() });
		}
	}

	updatePatches(nullptr, true);

	m_indices.resize(0);
	for (uint32_t lod = 0; lod < LodCount; ++lod)
	{
		const size_t indexOffset = m_indices.size();
		const uint32_t lodSkip = 1 << lod;

		for (uint32_t y = 0; y < patchDim - 1; y += lodSkip)
		{
			const uint32_t offset = y * patchDim;
			for (uint32_t x = 0; x < patchDim - 1; x += lodSkip)
			{
				if (lod > 0 && (x == 0 || y == 0 || x == patchDim - 1 - lodSkip || y == patchDim - 1 - lodSkip))
				{
					const int mid = x + offset + (lodSkip >> 1) + (lodSkip >> 1) * patchDim;

					if (x == 0)
					{
						m_indices.push_back(mid);
						m_indices.push_back(lodSkip + offset);
						m_indices.push_back(lodSkip + offset + lodSkip * patchDim);

						for (uint32_t i = 0; i < lodSkip; ++i)
						{
							m_indices.push_back(mid);
							m_indices.push_back(offset + i * patchDim + patchDim);
							m_indices.push_back(offset + i * patchDim);
						}
					}
					else if (x == patchDim - 1 - lodSkip)
					{
						m_indices.push_back(mid);
						m_indices.push_back(x + offset + lodSkip * patchDim);
						m_indices.push_back(x + offset);

						for (uint32_t i = 0; i < lodSkip; ++i)
						{
							m_indices.push_back(mid);
							m_indices.push_back(x + offset + i * patchDim + lodSkip);
							m_indices.push_back(x + offset + i * patchDim + lodSkip + patchDim);
						}
					}
					else
					{
						m_indices.push_back(mid);
						m_indices.push_back(x + offset + lodSkip * patchDim);
						m_indices.push_back(x + offset);

						m_indices.push_back(mid);
						m_indices.push_back(x + offset + lodSkip);
						m_indices.push_back(x + offset + lodSkip + lodSkip * patchDim);
					}

					if (y == 0)
					{
						m_indices.push_back(mid);
						m_indices.push_back(x + lodSkip * patchDim + offset + lodSkip);
						m_indices.push_back(x + lodSkip * patchDim + offset);

						for (uint32_t i = 0; i < lodSkip; ++i)
						{
							m_indices.push_back(mid);
							m_indices.push_back(x + offset + i);
							m_indices.push_back(x + offset + i + 1);
						}
					}
					else if (y == patchDim - 1 - lodSkip)
					{
						m_indices.push_back(mid);
						m_indices.push_back(x + offset);
						m_indices.push_back(x + offset + lodSkip);

						for (uint32_t i = 0; i < lodSkip; ++i)
						{
							m_indices.push_back(mid);
							m_indices.push_back(x + offset + i + lodSkip * patchDim + 1);
							m_indices.push_back(x + offset + i + lodSkip * patchDim);
						}
					}
					else
					{
						m_indices.push_back(mid);
						m_indices.push_back(x + offset);
						m_indices.push_back(x + offset + lodSkip);

						m_indices.push_back(mid);
						m_indices.push_back(x + offset + lodSkip * patchDim + lodSkip);
						m_indices.push_back(x + offset + lodSkip * patchDim);
					}
				}
				else
				{
					m_indices.push_back(x + offset);
					m_indices.push_back(lodSkip + x + offset);
					m_indices.push_back(lodSkip * patchDim + x + offset);

					m_indices.push_back(lodSkip + x + offset);
					m_indices.push_back(lodSkip * patchDim + lodSkip + x + offset);
					m_indices.push_back(lodSkip * patchDim + x + offset);
				}
			}
		}

		const size_t indexEndOffset = m_indices.size();
		T_ASSERT((indexEndOffset - indexOffset) % 3 == 0);

		m_primitives[lod] = render::Primitives::setIndexed(
			render::PrimitiveType::Triangles,
			(uint32_t)indexOffset,
			(uint32_t)(indexEndOffset - indexOffset) / 3);
	}

	m_indexBuffer = m_renderSystem->createBuffer(
		render::BuIndex,
		(uint32_t)m_indices.size() * sizeof(uint32_t),
		false);
	if (!m_indexBuffer)
		return false;

	uint32_t* index = static_cast< uint32_t* >(m_indexBuffer->lock());
	T_ASSERT_M(index, L"Unable to lock index buffer");

	for (uint32_t i = 0; i < uint32_t(m_indices.size()); ++i)
		index[i] = m_indices[i];

	m_indexBuffer->unlock();

	const uint32_t alignedPatchCount = alignUp((uint32_t)m_patches.size(), 16);

	m_drawBuffer = m_renderSystem->createBuffer(
		render::BuStructured | render::BuIndirect,
		alignedPatchCount * sizeof(render::IndexedIndirectDraw),
		true);
	if (!m_drawBuffer)
		return false;

	m_culledDrawBuffer = m_renderSystem->createBuffer(
		render::BuStructured | render::BuIndirect,
		alignedPatchCount * sizeof(render::IndexedIndirectDraw),
		false);
	if (!m_culledDrawBuffer)
		return false;

	m_dataBuffer = m_renderSystem->createBuffer(
		render::BuStructured,
		alignedPatchCount * sizeof(DrawData),
		true);
	if (!m_dataBuffer)
		return false;

	return true;
}

bool TerrainComponent::createRayTracingPatches()
{
	if (!m_renderSystem->supportRayTracing())
		return true;

	const uint32_t heightfieldSize = m_heightfield->getSize();
	T_ASSERT(heightfieldSize > 0);

	const uint32_t patchDim = m_terrain->getPatchDim();
	const uint32_t patchVertexCount = patchDim * patchDim;

	AlignedVector< render::VertexElement > vertexElements;
	vertexElements.push_back(render::VertexElement(render::DataUsage::Position, render::DtFloat3, 0));
	const uint32_t vertexSize = render::getVertexSize(vertexElements);

	Ref< const render::IVertexLayout > vertexLayout = m_renderSystem->createVertexLayout(vertexElements);
	if (!vertexLayout)
		return false;

	const Vector4& worldExtent = m_heightfield->getWorldExtent();
	const Vector4 patchExtent(worldExtent.x() / float(m_patchCount), worldExtent.y(), worldExtent.z() / float(m_patchCount), 0.0f);
	const Vector4 patchDeltaHalf = patchExtent * Vector4(0.5f, 0.5f, 0.5f, 0.0f);
	const Vector4 patchDeltaX = patchExtent * Vector4(1.0f, 0.0f, 0.0f, 0.0f);
	const Vector4 patchDeltaZ = patchExtent * Vector4(0.0f, 0.0f, 1.0f, 0.0f);
	Vector4 patchTopLeft = (-worldExtent * 0.5_simd).xyz1();

	// for (auto& part : m_rtParts)
	//{
	//	safeDestroy(part.perVertexData);
	//	safeDestroy(part.blas);
	// }

	safeDestroy(m_rtwInstance);

	m_rtVertexBuffers.resize(m_patchCount * m_patchCount);
	m_rtParts.resize(m_patchCount * m_patchCount);

	for (uint32_t pz = 0; pz < m_patchCount; ++pz)
	{
		Vector4 patchOrigin = patchTopLeft;
		for (uint32_t px = 0; px < m_patchCount; ++px)
		{
			const int32_t patchId = px + pz * m_patchCount;
			const Vector4 patchCenterWorld = patchOrigin + patchDeltaHalf;

			const Aabb3 patchAabb(
				patchCenterWorld * Vector4(1.0f, 0.0f, 1.0f, 1.0f) + Vector4(-patchDeltaHalf.x(), 0.0f, -patchDeltaHalf.z(), 0.0f),
				patchCenterWorld * Vector4(1.0f, 0.0f, 1.0f, 1.0f) + Vector4(patchDeltaHalf.x(), 0.0f, patchDeltaHalf.z(), 0.0f));

			// Create vertex buffer.
			m_rtVertexBuffers[patchId] = m_renderSystem->createBuffer(
				render::BuVertex,
				patchVertexCount * vertexSize,
				false);
			if (!m_rtVertexBuffers[patchId])
				return false;

			// Shrink the RT terrain a bit to reduce self intersection due
			// to mismatch between GBuffer and RT geometry.
			const Scalar elevationOffset = -1.0_simd;

			float* vertex = static_cast< float* >(m_rtVertexBuffers[patchId]->lock());
			T_ASSERT_M(vertex, L"Unable to lock vertex buffer");
			for (uint32_t z = 0; z < patchDim; ++z)
			{
				for (uint32_t x = 0; x < patchDim; ++x)
				{
					const float fx = float(x) / (patchDim - 1);
					const float fz = float(z) / (patchDim - 1);

					const float worldX = lerp(patchAabb.mn.x(), patchAabb.mx.x(), fx);
					const float worldZ = lerp(patchAabb.mn.z(), patchAabb.mx.z(), fz);
					const float worldY = m_heightfield->getWorldHeight(worldX, worldZ);

					float gridX, gridZ;
					m_heightfield->worldToGrid(worldX, worldZ, gridX, gridZ);

					const Vector4 normal = m_heightfield->normalAt(gridX, gridZ);

					*vertex++ = worldX + normal.x() * elevationOffset;
					*vertex++ = worldY + normal.y() * elevationOffset;
					*vertex++ = worldZ + normal.z() * elevationOffset;
				}
			}
			m_rtVertexBuffers[patchId]->unlock();

			// Create "per-vertex" attribute buffer.
			const uint32_t vertexAttribCount = m_primitives[1].count * 3;

			Ref< render::Buffer > perVertexData = m_renderSystem->createBuffer(
				render::BuStructured,
				vertexAttribCount * sizeof(world::HWRT_Material),
				false);
			if (!perVertexData)
				return false;

			world::HWRT_Material* va = static_cast< world::HWRT_Material* >(perVertexData->lock());
			T_ASSERT_M(va, L"Unable to lock vertex attribute buffer");
			for (uint32_t i = 0; i < vertexAttribCount; ++i)
			{
				const uint32_t index = m_indices[m_primitives[1].offset + i];

				int32_t ix = index % patchDim;
				int32_t iz = index / patchDim;

				const float fx = float(ix) / (patchDim - 1);
				const float fz = float(iz) / (patchDim - 1);

				const float worldX = lerp(patchAabb.mn.x(), patchAabb.mx.x(), fx);
				const float worldZ = lerp(patchAabb.mn.z(), patchAabb.mx.z(), fz);

				float gridX, gridZ;
				m_heightfield->worldToGrid(worldX, worldZ, gridX, gridZ);

				const Vector4 normal = m_heightfield->normalAt(gridX, gridZ);

				Vector4(0.2f, 0.4f, 0.1f, 0.0f).storeUnaligned3(va->albedo);
				normal.storeUnaligned3(va->normal);

				va->emissive = 0.0f;

				va->texCoord[0] = gridX / m_heightfield->getSize();
				va->texCoord[1] = gridZ / m_heightfield->getSize();
				va->albedoMap = (m_surfaceCache != nullptr && m_surfaceCache->getBaseTexture() != nullptr) ? m_surfaceCache->getBaseTexture()->getBindlessIndex() : -1;

				va++;
			}
			perVertexData->unlock();

			m_rtParts[patchId].perVertexData = perVertexData;

			// Create bottom level acceleration structure.
			m_rtParts[patchId].blas = m_renderSystem->createAccelerationStructure(m_rtVertexBuffers[patchId], vertexLayout, m_indexBuffer, render::IndexType::UInt32, { m_primitives[1] }, false);
			if (!m_rtParts[patchId].blas)
				return false;

			patchOrigin += patchDeltaX;
		}

		patchTopLeft += patchDeltaZ;
	}

	// Reset world to create new RT instance.
	setWorld(m_world);

	return true;
}

}
