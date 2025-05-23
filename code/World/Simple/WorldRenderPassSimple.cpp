/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Render/Shader.h"
#include "Render/Context/ProgramParameters.h"
#include "World/WorldHandles.h"
#include "World/Simple/WorldRenderPassSimple.h"

namespace traktor::world
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.world.WorldRenderPassSimple", WorldRenderPassSimple, IWorldRenderPass)

WorldRenderPassSimple::WorldRenderPassSimple(
	render::handle_t technique,
	render::ProgramParameters* globalProgramParams,
	const Matrix44& view
)
:	m_technique(technique)
,	m_globalProgramParams(globalProgramParams)
,	m_view(view)
,	m_viewInverse(view.inverse())
{
}

render::handle_t WorldRenderPassSimple::getTechnique() const
{
	return m_technique;
}

uint32_t WorldRenderPassSimple::getPassFlags() const
{
	return IWorldRenderPass::First;
}

render::Shader::Permutation WorldRenderPassSimple::getPermutation(const render::Shader* shader) const
{
	return render::Shader::Permutation(m_technique);
}

void WorldRenderPassSimple::setProgramParameters(render::ProgramParameters* programParams) const
{
	const Matrix44 w = Matrix44::identity();
	programParams->attachParameters(m_globalProgramParams);
	programParams->setMatrixParameter(ShaderParameter::View, m_view);
	programParams->setMatrixParameter(ShaderParameter::ViewInverse, m_viewInverse);
	programParams->setMatrixParameter(ShaderParameter::World, w);
	programParams->setMatrixParameter(ShaderParameter::WorldView, m_view * w);

}

void WorldRenderPassSimple::setProgramParameters(render::ProgramParameters* programParams, const Transform& lastWorld, const Transform& world) const
{
	const Matrix44 w = world.toMatrix44();
	programParams->attachParameters(m_globalProgramParams);
	programParams->setMatrixParameter(ShaderParameter::View, m_view);
	programParams->setMatrixParameter(ShaderParameter::ViewInverse, m_viewInverse);
	programParams->setMatrixParameter(ShaderParameter::World, w);
	programParams->setMatrixParameter(ShaderParameter::WorldView, m_view * w);
}

}
