/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Serialization/ISerializer.h"
#include "Core/Serialization/MemberRef.h"
#include "Sound/Processor/Edge.h"
#include "Sound/Processor/InputPin.h"
#include "Sound/Processor/Node.h"
#include "Sound/Processor/OutputPin.h"

namespace traktor::sound
{
	namespace
	{

struct OutputPinAccessor
{
	static const OutputPin* get(Ref< Node >& node, const std::wstring& name) {
		return node->findOutputPin(name);
	}
};

struct InputPinAccessor
{
	static const InputPin* get(Ref< Node >& node, const std::wstring& name) {
		return node->findInputPin(name);
	}
};

template < typename PinType, typename PinAccessor >
class MemberPin : public MemberComplex
{
public:
	explicit MemberPin(const wchar_t* const name, PinType*& pin)
	:	MemberComplex(name, true)
	,	m_pin(pin)
	{
	}

	virtual void serialize(ISerializer& s) const
	{
		if (s.getDirection() == ISerializer::Direction::Write)
		{
			Ref< Node > node = m_pin ? m_pin->getNode() : nullptr;
			std::wstring name = m_pin ? m_pin->getName() : L"";

			s >> MemberRef< Node >(L"node", node);
			s >> Member< std::wstring >(L"name", name);
		}
		else	// Direction::Read
		{
			Ref< Node > node;
			std::wstring name;

			s >> MemberRef< Node >(L"node", node);
			s >> Member< std::wstring >(L"name", name);

			if (node)
				m_pin = PinAccessor::get(node, name);
			else
				m_pin = nullptr;
		}
	}

private:
	PinType*& m_pin;
};

	}

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.sound.Edge", 0, Edge, ISerializable)

Edge::Edge(const OutputPin* source, const InputPin* destination)
:	m_source(source)
,	m_destination(destination)
{
}

void Edge::setSource(const OutputPin* source)
{
	m_source = source;
}

const OutputPin* Edge::getSource() const
{
	return m_source;
}

void Edge::setDestination(const InputPin* destination)
{
	m_destination = destination;
}

const InputPin* Edge::getDestination() const
{
	return m_destination;
}

void Edge::serialize(ISerializer& s)
{
	s >> MemberPin< const OutputPin, OutputPinAccessor >(L"source", m_source);
	s >> MemberPin< const InputPin, InputPinAccessor >(L"destination", m_destination);
}

}
