/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Containers/AlignedVector.h"
#include "Core/Containers/StaticVector.h"
#include "Core/Object.h"
#include "Core/RefArray.h"
#include "Render/Frame/RenderGraphTypes.h"
#include "Render/Types.h"

#include <functional>

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RENDER_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::render
{

class RenderContext;
class RenderGraph;

/*! Render pass definition.
 * \ingroup Render
 */
class T_DLLCLASS RenderPass : public RefCountImpl< ITypedObject >
{
	T_RTTI_CLASS;

public:
	typedef std::function< void(const RenderGraph&, RenderContext*) > fn_build_t;

	struct Input
	{
		handle_t resourceId = 0; //!< Resource name.
	};

	struct Output
	{
		handle_t resourceId = ~0; //!< Resource name.
		Clear clear;			  //!< Target clear value and mask (only applicable to render target resources).
		uint32_t load = 0;
		uint32_t store = 0;

		// !note! We're not comparing clear value for now since it depends on the load and store.
		bool operator==(const Output& rh) const
		{
			return resourceId == rh.resourceId && load == rh.load && store == rh.store;
		}

		// !note! We're not comparing clear value for now since it depends on the load and store.
		bool operator!=(const Output& rh) const
		{
			return !(*this == rh);
		}
	};

	explicit RenderPass(const std::wstring& name = L"Unnamed");

	void setName(const std::wstring& name);

	const std::wstring& getName() const { return m_name; }

	//! \{

	/*! Add input to render pass.
	 *
	 * \param resourceId ID of input resource.
	 */
	void addInput(handle_t resourceId);

	/*! Add target set input to render pass.
	 *
	 * \param targetSet Handle to target set.
	 */
	void addInput(RGTargetSet targetSet);

	/*! Add buffer input to render pass.
	 *
	 * \param buffer Handle to buffer.
	 */
	void addInput(RGBuffer buffer);

	/*! Add texture input to render pass.
	 *
	 * \param texture Handle to texture.
	 */
	void addInput(RGTexture texture);

	/*! Add dependency input to render pass.
	 *
	 * \param dependency Handle to dependency.
	 */
	void addInput(RGDependency dependency);

	/*! Add weak input to render pass.
	 *
	 * A weak input doesn't resolve into a dependency between
	 * passes but only register the pass as it will use the
	 * resource during build.
	 *
	 * \param resourceId ID of input resource.
	 */
	void addWeakInput(RGTexture texture);

	const StaticVector< Input, 16 >& getInputs() const { return m_inputs; }

	//! \}

	//! \{

	void setOutput(RGTargetSet targetSet, uint32_t load, uint32_t store);

	void setOutput(RGTargetSet targetSet, const Clear& clear, uint32_t load, uint32_t store);

	void setOutput(RGBuffer buffer);

	void setOutput(RGTexture texture);

	void setOutput(RGDependency dependency);

	bool haveOutput() const { return m_output.resourceId != ~0; }

	const Output& getOutput() const { return m_output; }

	//! \}

	//! \{

	void addBuild(const fn_build_t& build);

	const AlignedVector< fn_build_t >& getBuilds() const { return m_builds; }

	//! \}

	void* operator new(size_t size);

	void operator delete(void* ptr);

protected:
	std::wstring m_name;
	StaticVector< Input, 16 > m_inputs;
	Output m_output;
	AlignedVector< fn_build_t > m_builds;
};

}
