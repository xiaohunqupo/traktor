/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Serialization/AttributeRange.h"
#include "Core/Serialization/ISerializer.h"
#include "Heightfield/Heightfield.h"
#include "Heightfield/Editor/OcclusionTextureAsset.h"
#include "Resource/Member.h"

namespace traktor::hf
{

T_IMPLEMENT_RTTI_EDIT_CLASS(L"traktor.hf.OcclusionTextureAsset", 2, OcclusionTextureAsset, ISerializable)

void OcclusionTextureAsset::serialize(ISerializer& s)
{
	s >> resource::Member< Heightfield >(L"heightfield", m_heightfield);
	s >> resource::Member< ISerializable >(L"occluderData", m_occluderData);

	if (s.getVersion() >= 1)
	{
		s >> Member< uint32_t >(L"size", m_size, AttributeRange(16));
		s >> Member< float >(L"traceDistance", m_traceDistance, AttributeRange(0.0f));
	}

	if (s.getVersion() >= 2)
		s >> Member< int32_t >(L"blurRadius", m_blurRadius, AttributeRange(0));
}

}
