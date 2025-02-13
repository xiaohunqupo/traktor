/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/RefArray.h"

namespace traktor
{
	namespace model
	{

class Model;

	}

	namespace shape
	{

void splitModel(const model::Model* model, RefArray< model::Model >& outModels);

	}
}