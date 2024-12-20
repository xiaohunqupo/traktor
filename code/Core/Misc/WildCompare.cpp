/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Misc/WildCompare.h"
#include "Core/Misc/Split.h"
#include "Core/Misc/String.h"

namespace traktor
{

WildCompare::WildCompare(const std::wstring& mask)
:	m_beginWild(false)
,	m_endWild(false)
{
	if (mask.length() > 0)
	{
		Split< std::wstring >::any(mask, L"*", m_nowild);
		m_beginWild = bool(mask[0] == L'*');
		m_endWild = bool(mask[mask.length() - 1] == L'*');
	}
}

bool WildCompare::match(const std::wstring& str, CompareMode mode, std::vector< std::wstring >* outPieces) const
{
	size_t p = 0;
	size_t i = 0;

	if (!m_nowild.size())
		return m_beginWild;

	const std::wstring chk = (mode == IgnoreCase) ? toLower(str) : str;

	if (!m_beginWild)
	{
		const size_t ln = m_nowild[0].length();

		const int res = (mode == IgnoreCase) ? chk.compare(0, ln, toLower(m_nowild[0])) : chk.compare(0, ln, m_nowild[0]);
		if (res != 0)
			return false;

		p = ln;
		i = 1;
	}

	for (; i < m_nowild.size(); ++i)
	{
		const size_t f = (mode == IgnoreCase) ? chk.find(toLower(m_nowild[i]), p) : chk.find(m_nowild[i], p);
		if (f == std::wstring::npos)
			return false;

		if (outPieces)
			outPieces->push_back(str.substr(p, f - p));

		p = f + m_nowild[i].length();
	}

	if (!m_endWild)
	{
		if (p < str.length())
			return false;
	}
	else if (outPieces)
		outPieces->push_back(str.substr(p));

	return true;
}

std::wstring WildCompare::merge(const std::vector< std::wstring >& pieces) const
{
	auto it = pieces.begin();
	std::wstring merged;

	if (m_beginWild && it != pieces.end())
	{
		merged += *it;
		++it;
	}

	for (const auto& nowild : m_nowild)
	{
		merged += nowild;
		if (it != pieces.end())
		{
			merged += *it;
			++it;
		}
	}

	if (m_endWild && it != pieces.end())
		merged += *it;

	return merged;
}

}
