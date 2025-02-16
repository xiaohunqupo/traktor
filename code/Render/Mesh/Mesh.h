/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include <string>
#include "Core/Object.h"
#include "Core/Containers/AlignedVector.h"
#include "Core/Containers/SmallMap.h"
#include "Core/Math/Aabb3.h"
#include "Core/Misc/FourCC.h"
#include "Render/VertexElement.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RENDER_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::render
{

class Buffer;
class IVertexLayout;

/*! Render mesh.
 * \ingroup Render
 */
class T_DLLCLASS Mesh : public Object
{
	T_RTTI_CLASS;

public:
	struct Part
	{
		std::wstring name;
		Primitives primitives;
	};

	void setVertexElements(const AlignedVector< VertexElement >& vertexDeclaration);

	void setVertexLayout(const IVertexLayout* vertexLayout);

	void setVertexBuffer(Buffer* vertexBuffer);

	void setIndexType(IndexType indexType);

	void setIndexBuffer(Buffer* indexBuffer);

	void setAuxBuffer(const FourCC& id, Buffer* auxBuffer);

	void setAuxBuffers(const SmallMap< FourCC, Ref< Buffer > >& auxBuffers);

	void setParts(const AlignedVector< Part >& parts);

	void setBoundingBox(const Aabb3& boundingBox);
	
	const AlignedVector< VertexElement >& getVertexElements() const { return m_vertexElements; }

	const IVertexLayout* getVertexLayout() const { return m_vertexLayout; }

	Buffer* getVertexBuffer() const { return m_vertexBuffer; }

	IndexType getIndexType() const { return m_indexType; }

	Buffer* getIndexBuffer() const { return m_indexBuffer; }

	Buffer* getAuxBuffer(const FourCC& id) const { return m_auxBuffers[id]; }

	const SmallMap< FourCC, Ref< Buffer > >& getAuxBuffers() const { return m_auxBuffers; }

	const AlignedVector< Part >& getParts() const { return m_parts; }

	const Aabb3& getBoundingBox() const { return m_boundingBox; }

private:
	AlignedVector< VertexElement > m_vertexElements;
	Ref< const IVertexLayout > m_vertexLayout;
	Ref< Buffer > m_vertexBuffer;
	IndexType m_indexType;
	Ref< Buffer > m_indexBuffer;
	SmallMap< FourCC, Ref< Buffer > > m_auxBuffers;
	AlignedVector< Part > m_parts;
	Aabb3 m_boundingBox;
};

}
