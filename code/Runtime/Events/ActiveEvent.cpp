/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Runtime/Events/ActiveEvent.h"

namespace traktor::runtime
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.runtime.ActiveEvent", ActiveEvent, Object)

ActiveEvent::ActiveEvent(bool activated)
:	m_activated(activated)
{
}

bool ActiveEvent::becameActivated() const
{
	return m_activated;
}

}
