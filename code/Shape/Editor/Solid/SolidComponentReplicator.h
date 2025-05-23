/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "World/Editor/IEntityReplicator.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_SHAPE_EDITOR_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::shape
{

/*!
 * \ingroup Shape
 */
class T_DLLCLASS SolidComponentReplicator : public world::IEntityReplicator
{
	T_RTTI_CLASS;

public:
	virtual bool create(const editor::IPipelineSettings* settings) override final;

	virtual TypeInfoSet getSupportedTypes() const override final;

	virtual RefArray< const world::IEntityComponentData > getDependentComponents(
		const world::EntityData* entityData,
		const world::IEntityComponentData* componentData
	) const override final;

	virtual Ref< model::Model > createModel(
		editor::IPipelineCommon* pipelineCommon,
		const world::EntityData* entityData,
		const world::IEntityComponentData* componentData,
		Usage usage
	) const override final;
};

}
