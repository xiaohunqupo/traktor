/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Log/Log.h"
#include "Online/Impl/Tasks/TaskEnumAchievements.h"
#include "Online/Provider/IAchievementsProvider.h"

namespace traktor::online
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.online.TaskEnumAchievements", TaskEnumAchievements, ITask)

TaskEnumAchievements::TaskEnumAchievements(
	IAchievementsProvider* provider,
	Object* sinkObject,
	sink_method_t sinkMethod
)
:	m_provider(provider)
,	m_sinkObject(sinkObject)
,	m_sinkMethod(sinkMethod)
{
}

void TaskEnumAchievements::execute(TaskQueue* taskQueue)
{
	T_ASSERT(m_provider);
	T_DEBUG(L"Online; Begin enumerating achievements");
	std::map< std::wstring, bool > achievements;
	m_provider->enumerate(achievements);
	(m_sinkObject->*m_sinkMethod)(achievements);
	T_DEBUG(L"Online; Finished enumerating achievements");
}

}
