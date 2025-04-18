/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include <unordered_map>
#include "Core/Containers/SmallMap.h"
#include "Core/Containers/SmallSet.h"

namespace traktor::model
{

template < typename ValueType, typename HashFunction >
class HashVector
{
public:
	static const uint32_t InvalidIndex = ~0U;

	void clear()
	{
		m_indices.clear();
		m_values.clear();
	}

	void swap(AlignedVector< ValueType >& values)
	{
		m_values.swap(values);
		m_indices.clear();		
		for (uint32_t i = 0; i < (uint32_t)m_values.size(); ++i)
		{
			const uint32_t hash = HashFunction::get(m_values[i]);
			m_indices[hash].insert(i);
		}
	}

	void replace(const AlignedVector< ValueType >& values)
	{
		m_values = values;
		m_indices.clear();
		for (uint32_t i = 0; i < (uint32_t)m_values.size(); ++i)
		{
			const uint32_t hash = HashFunction::get(m_values[i]);
			m_indices[hash].insert(i);
		}
	}

	void reserve(uint32_t capacity)
	{
		m_values.reserve(capacity);
		m_indices.reserve(capacity);
	}

	uint32_t size() const
	{
		return (uint32_t)m_values.size();
	}

	uint32_t add(const ValueType& v)
	{
		const uint32_t hash = HashFunction::get(v);
		const uint32_t index = (uint32_t)m_values.size();
		m_values.push_back(v);
		m_indices[hash].insert(index);
		return index;
	}

	void set(uint32_t index, const ValueType& v)
	{
		// Remove old index.
		{
			const uint32_t hash = HashFunction::get(m_values[index]);
			m_indices[hash].erase(index);
		}

		// Replace value.
		m_values[index] = v;

		// Add new index.
		{
			const uint32_t hash = HashFunction::get(v);
			m_indices[hash].insert(index);
		}
	}

	uint32_t find(const ValueType& v) const
	{
		const uint32_t hash = HashFunction::get(v);

		const auto it = m_indices.find(hash);
		if (it == m_indices.end())
			return InvalidIndex;

		for (uint32_t index : it->second)
		{
			if (m_values[index] == v)
				return index;
		}

		return InvalidIndex;
	}

	const std::unordered_map< uint32_t, SmallSet< uint32_t > >& indices() const
	{
		return m_indices;
	}

	const AlignedVector< ValueType >& values() const
	{
		return m_values;
	}

	const ValueType& operator [] (uint32_t index) const
	{
		return m_values[index];
	}

private:
	std::unordered_map< uint32_t, SmallSet< uint32_t > > m_indices;
	AlignedVector< ValueType > m_values;	
};

}
