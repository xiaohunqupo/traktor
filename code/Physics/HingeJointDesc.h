/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Physics/JointDesc.h"
#include "Core/Math/Vector4.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_PHYSICS_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::physics
{

/*! Hinge joint description.
 * \ingroup Physics
 */
class T_DLLCLASS HingeJointDesc : public JointDesc
{
	T_RTTI_CLASS;

public:
	HingeJointDesc();

	void setAnchor(const Vector4& anchor);

	const Vector4& getAnchor() const;

	void setAxis(const Vector4& axis);

	const Vector4& getAxis() const;

	void setEnableLimits(bool enableLimits);

	bool getEnableLimits() const;

	void setAngles(float minAngle, float maxAngle);

	void getAngles(float& outMinAngle, float& outMaxAngle) const;

	void setAngularOnly(bool angularOnly);

	bool getAngularOnly() const;

	virtual void serialize(ISerializer& s) override final;

private:
	Vector4 m_anchor;
	Vector4 m_axis;
	bool m_enableLimits;
	float m_minAngle;
	float m_maxAngle;
	bool m_angularOnly;
};

}
