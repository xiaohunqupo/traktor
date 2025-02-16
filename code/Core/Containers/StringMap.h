/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Containers/SmallMap.h"

namespace traktor
{

/*! String map
 * \ingroup Core
 */
template < typename ValueType, typename StringType = std::string, int BucketCount = 128 >
class StringMap
{
public:
	typedef ValueType value_t;
	typedef StringType string_t;
	typedef typename StringType::const_pointer const_pointer_t;

	value_t* find(const string_t& str)
	{
		uint32_t hash = shash(str) & (BucketCount - 1);
		SmallMap< string_t, value_t >& buckets = m_buckets[hash & (BucketCount - 1)];
		typename SmallMap< string_t, value_t >::iterator i = buckets.find(str);
		return i != buckets.end() ? &i->second : nullptr;
	}

	const value_t* find(const string_t& str) const
	{
		uint32_t hash = shash(str) & (BucketCount - 1);
		const SmallMap< string_t, value_t >& buckets = m_buckets[hash & (BucketCount - 1)];
		typename SmallMap< string_t, value_t >::const_iterator i = buckets.find(str);
		return i != buckets.end() ? &i->second : nullptr;
	}

	void insert(const string_t& str, const value_t& value)
	{
		uint32_t hash = shash(str) & (BucketCount - 1);
		m_buckets[hash].insert(str, value);
	}

	bool remove(const string_t& str)
	{
		uint32_t hash = shash(str) & (BucketCount - 1);
		SmallMap< string_t, value_t >& buckets = m_buckets[hash & (BucketCount - 1)];
		typename SmallMap< string_t, value_t >::iterator i = buckets.find(str);
		if (i == buckets.end())
			return false;
		buckets.erase(i);
		return true;
	}

	void clear()
	{
		for (int32_t i = 0; i < BucketCount; ++i)
			m_buckets[i].clear();
	}

	value_t& operator [] (const string_t& str)
	{
		uint32_t hash = shash(str) & (BucketCount - 1);
		return m_buckets[hash][str];
	}

	const value_t& operator [] (const string_t& str) const
	{
		uint32_t hash = shash(str) & (BucketCount - 1);
		return m_buckets[hash][str];
	}

private:
	SmallMap< string_t, value_t > m_buckets[BucketCount];

	uint32_t shash(const string_t& str) const
	{
		uint32_t hash = 0;
		for (const typename string_t::value_type* c = str.c_str(); *c; ++c)
			hash = *c + (hash << 6) + (hash << 16) - hash;
		return hash;
	}
};

}
