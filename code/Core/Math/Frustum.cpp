/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include <cmath>
#include "Core/Math/Frustum.h"
#include "Core/Math/Matrix44.h"

namespace traktor
{

void Frustum::buildFromPlanes(const Plane planes_[6])
{
	planes.resize(6);
	for (int32_t i = 0; i < 6; ++i)
		planes[i] = planes_[i];
	update();
}

void Frustum::buildFromCorners(const Vector4 corners_[8])
{
	planes.resize(6);
	planes[0] = Plane(corners_[0], corners_[3], corners_[4]);	// left
	planes[1] = Plane(corners_[2], corners_[1], corners_[6]);	// right
	planes[2] = Plane(corners_[2], corners_[6], corners_[3]);	// bottom 2 3 6
	planes[3] = Plane(corners_[1], corners_[0], corners_[5]);	// top
	planes[4] = Plane(corners_[3], corners_[0], corners_[2]);	// near
	planes[5] = Plane(corners_[6], corners_[5], corners_[7]);	// far
	update();
}

void Frustum::buildPerspective(float vfov, float aspect, float zn, float zf)
{
	vfov /= 2.0f;

	const float a = sinf(vfov) * aspect;
	const float b = cosf(vfov);
	const float c = sqrtf(a * a + b * b);
	const float hfov = asinf(a / c);

	planes.resize(6);
	planes[0].set(Vector4(cosf(hfov), 0.0f, sinf(hfov)), 0.0_simd);
	planes[1].set(Vector4(-cosf(hfov), 0.0f, sinf(hfov)), 0.0_simd);
	planes[2].set(Vector4(0.0f, cosf(vfov), sinf(vfov)), 0.0_simd);
	planes[3].set(Vector4(0.0f, -cosf(vfov), sinf(vfov)), 0.0_simd);
	planes[4].set(Vector4(0.0f, 0.0f,  1.0f), Scalar(zn));
	planes[5].set(Vector4(0.0f, 0.0f, -1.0f), Scalar(-zf));

	update();
}

void Frustum::buildOrtho(float width, float height, float zn, float zf)
{
	const Scalar hw = Scalar(-width * 0.5f);
	const Scalar hh = Scalar(-height * 0.5f);

	planes.resize(6);
	planes[0].set(Vector4(1.0f, 0.0f, 0.0f), hw);
	planes[1].set(Vector4(-1.0f, 0.0f, 0.0f), hw);
	planes[2].set(Vector4(0.0f, 1.0f, 0.0f), hh);
	planes[3].set(Vector4(0.0f, -1.0f, 0.0f), hh);
	planes[4].set(Vector4(0.0f, 0.0f, 1.0f), Scalar(zn));
	planes[5].set(Vector4(0.0f, 0.0f, -1.0f), Scalar(-zf));

	update();
}

void Frustum::setNearZ(const Scalar& zn)
{
	planes[Near].setDistance(zn);
	update();
}

Scalar Frustum::getNearZ() const
{
	return planes[Near].distance();
}

void Frustum::setFarZ(const Scalar& zf)
{
	planes[Far].setDistance(-zf);
	update();
}

Scalar Frustum::getFarZ() const
{
	return -planes[Far].distance();
}

void Frustum::scale(const Scalar& f)
{
	for (int32_t i = 0; i < planes.size(); ++i)
		planes[i].setDistance(planes[i].distance() - f);
	update();
}

bool Frustum::empty() const
{
	return planes.empty();
}

Frustum::Result Frustum::inside(const Vector4& point) const
{
	for (uint32_t i = 0; i < planes.size(); ++i)
	{
		if (planes[i].distance(point) < 0.0_simd)
			return Result::Outside;
	}
	return Result::Inside;
}

Frustum::Result Frustum::inside(const Vector4& center, const Scalar& radius) const
{
	const Scalar nradius = -radius;
	bool partial = false;

	for (uint32_t i = 0; i < planes.size(); ++i)
	{
		const Scalar distance = dot3(planes[i].normal(), center) - planes[i].distance();
		if (distance < nradius)
			return Result::Outside;
		if (distance < radius)
			partial = true;
	}

	return partial ? Result::Partial : Result::Inside;
}

Frustum::Result Frustum::inside(const Aabb3& aabb) const
{
	bool partial = false;
	for (uint32_t i = 0; i < planes.size(); ++i)
	{
		const Vector4 n = select(planes[i].normal(), aabb.mn, aabb.mx);
		if (planes[i].distance(n) < 0.0_simd)	// outside
			return Result::Outside;

		const Vector4 p = select(planes[i].normal(), aabb.mx, aabb.mn);
		if (planes[i].distance(p) < 0.0_simd)	// intersecting
			partial |= true;
	}
	return partial ? Result::Partial : Result::Inside;
}

Frustum::Result Frustum::inside(const Matrix44& transform, const Frustum& other) const
{
	if (empty() || other.empty())
		return Result::Outside;
	for (uint32_t i = 0; i < sizeof_array(other.corners); ++i)
	{
		if (inside(transform * other.corners[i].xyz1()) != Result::Inside)
			return Result::Outside;
	}
	return Result::Inside;
}

void Frustum::update()
{
	Plane::uniqueIntersectionPoint(planes[Near], planes[Left]  , planes[Top]   , corners[0]);
	Plane::uniqueIntersectionPoint(planes[Near], planes[Top]   , planes[Right] , corners[1]);
	Plane::uniqueIntersectionPoint(planes[Near], planes[Right] , planes[Bottom], corners[2]);
	Plane::uniqueIntersectionPoint(planes[Near], planes[Bottom], planes[Left]  , corners[3]);
	Plane::uniqueIntersectionPoint(planes[Far] , planes[Left]  , planes[Top]   , corners[4]);
	Plane::uniqueIntersectionPoint(planes[Far] , planes[Top]   , planes[Right] , corners[5]);
	Plane::uniqueIntersectionPoint(planes[Far] , planes[Right] , planes[Bottom], corners[6]);
	Plane::uniqueIntersectionPoint(planes[Far] , planes[Bottom], planes[Left]  , corners[7]);

	center = corners[0];
	for (int32_t i = 1; i < 8; ++i)
		center += corners[i];
	center /= 8.0_simd;
}

}
