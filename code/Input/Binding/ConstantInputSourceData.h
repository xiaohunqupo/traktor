/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Input/Binding/IInputSourceData.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_INPUT_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::input
{

/*! Constant value input.
 * \ingroup Input
 */
class T_DLLCLASS ConstantInputSourceData : public IInputSourceData
{
	T_RTTI_CLASS;

public:
	ConstantInputSourceData() = default;

	explicit ConstantInputSourceData(float value);

	virtual Ref< IInputSource > createInstance(DeviceControlManager* deviceControlManager) const override final;

	virtual void serialize(ISerializer& s) override final;

private:
	float m_value = 0.0f;
};

}
