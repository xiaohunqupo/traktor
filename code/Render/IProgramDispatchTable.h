/*
 * TRAKTOR
 * Copyright (c) 2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Object.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RENDER_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::render
{

class IProgram;

/*! Table of programs able to be dispatched dynamically from another program.
 * \ingroup Render
 */
class T_DLLCLASS IProgramDispatchTable : public Object
{
	T_RTTI_CLASS;

public:
	virtual int32_t addProgram(IProgram* program) = 0;
};

}