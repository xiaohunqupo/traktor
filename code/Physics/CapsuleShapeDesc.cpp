/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Physics/CapsuleShapeDesc.h"
#include "Core/Serialization/AttributeRange.h"
#include "Core/Serialization/AttributeUnit.h"
#include "Core/Serialization/ISerializer.h"
#include "Core/Serialization/Member.h"

namespace traktor::physics
{

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.physics.CapsuleShapeDesc", ShapeDesc::Version, CapsuleShapeDesc, ShapeDesc)

void CapsuleShapeDesc::setRadius(float radius)
{
	m_radius = radius;
}

float CapsuleShapeDesc::getRadius() const
{
	return m_radius;
}

void CapsuleShapeDesc::setLength(float length)
{
	m_length = length;
}

float CapsuleShapeDesc::getLength() const
{
	return m_length;
}

void CapsuleShapeDesc::serialize(ISerializer& s)
{
	ShapeDesc::serialize(s);

	s >> Member< float >(L"radius", m_radius, AttributeRange(0.0f) | AttributeUnit(UnitType::Metres));
	s >> Member< float >(L"length", m_length, AttributeRange(0.0f) | AttributeUnit(UnitType::Metres));
}

}
