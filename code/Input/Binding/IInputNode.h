/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Ref.h"
#include "Core/Serialization/ISerializable.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_INPUT_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::input
{

class InputValueSet;

/*! Abstract input signal node.
 * \ingroup Input
 *
 * Input node is used by the states in order
 * to evaluate source values into state ready
 * output value.
 */
class T_DLLCLASS IInputNode : public ISerializable
{
	T_RTTI_CLASS;

public:
	struct Instance : public IRefCount {};

	virtual Ref< Instance > createInstance() const = 0;

	virtual float evaluate(
		Instance* instance,
		const InputValueSet& valueSet,
		float T,
		float dT
	) const = 0;
};

}
