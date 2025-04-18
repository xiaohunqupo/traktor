/*
 * TRAKTOR
 * Copyright (c) 2022-2025 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Object.h"
#include "Core/RefArray.h"
#include "Render/Image2/ImageGraphTypes.h"

#include <functional>
#include <string>

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RENDER_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::render
{

class ImageGraph;
class ImageGraphContext;
class ImagePassStep;
class ProgramParameters;
class RenderGraph;
class RenderPass;
class ScreenRenderer;

struct ImageGraphView;

/*! Image pass
 * \ingroup Render
 *
 * An image pass represent an entire render pass,
 * so an image pass may contain several steps which
 * are executed in sequence in a single render pass.
 */
class T_DLLCLASS ImagePass : public Object
{
	T_RTTI_CLASS;

public:
	RenderPass* addRenderGraphPasses(
		const ImageGraph* graph,
		const ImageGraphContext& context,
		const ImageGraphView& view,
		const targetSetVector_t& targetSetIds,
		const bufferVector_t& sbufferIds,
		const std::function< void(const RenderGraph& renderGraph, ProgramParameters*) >& parametersFn,
		ScreenRenderer* screenRenderer,
		RenderGraph& renderGraph) const;

private:
	friend class ImagePassData;

	std::wstring m_name;
	PassOutput m_output;
	Clear m_clear;
	RefArray< const ImagePassStep > m_steps;
};

}
