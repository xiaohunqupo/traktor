/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Misc/String.h"
#include "Input/Binding/InBoolean.h"
#include "Input/Editor/InBooleanTraits.h"

namespace traktor::input
{

std::wstring InBooleanTraits::getHeader(const IInputNode* node) const
{
	return L"Boolean";
}

std::wstring InBooleanTraits::getDescription(const IInputNode* node) const
{
	const InBoolean* inBoolean = checked_type_cast< const InBoolean*, false >(node);
	switch (inBoolean->m_op)
	{
	case InBoolean::OpNot:
		return L"Not";
	case InBoolean::OpAnd:
		return L"And";
	case InBoolean::OpOr:
		return L"Or";
	case InBoolean::OpXor:
		return L"Xor";
	default:
		return L"";
	}
}

Ref< IInputNode > InBooleanTraits::createNode() const
{
	return new InBoolean();
}

void InBooleanTraits::getInputNodes(const IInputNode* node, std::map< const std::wstring, Ref< const IInputNode > >& outInputNodes) const
{
	const InBoolean* inBoolean = checked_type_cast< const InBoolean*, false >(node);

	const RefArray< IInputNode >& sources = inBoolean->m_source;
	for (uint32_t i = 0; i < uint32_t(sources.size()); ++i)
		outInputNodes[toString(i)] = sources[i];

	outInputNodes[L"*"] = 0;
}

void InBooleanTraits::connectInputNode(IInputNode* node, const std::wstring& inputName, IInputNode* sourceNode) const
{
	InBoolean* inBoolean = checked_type_cast< InBoolean*, false >(node);
	if (inputName == L"*")
		inBoolean->m_source.push_back(sourceNode);
	else
	{
		const int32_t index = parseString< int32_t >(inputName);
		if (index >= 0 && index < int32_t(inBoolean->m_source.size()))
			inBoolean->m_source[index] = sourceNode;
	}
}

void InBooleanTraits::disconnectInputNode(IInputNode* node, const std::wstring& inputName) const
{
	InBoolean* inBoolean = checked_type_cast< InBoolean*, false >(node);
	const int32_t index = parseString< int32_t >(inputName);
	if (index >= 0 && index < int32_t(inBoolean->m_source.size()))
		inBoolean->m_source.erase(inBoolean->m_source.begin() + index);
}

}
