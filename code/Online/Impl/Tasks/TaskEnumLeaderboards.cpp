/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Log/Log.h"
#include "Online/Impl/Tasks/TaskEnumLeaderboards.h"

namespace traktor::online
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.online.TaskEnumLeaderboards", TaskEnumLeaderboards, ITask)

TaskEnumLeaderboards::TaskEnumLeaderboards(
	ILeaderboardsProvider* provider,
	Object* sinkObject,
	sink_method_t sinkMethod
)
:	m_provider(provider)
,	m_sinkObject(sinkObject)
,	m_sinkMethod(sinkMethod)
{
}

void TaskEnumLeaderboards::execute(TaskQueue* taskQueue)
{
	T_ASSERT(m_provider);
	T_DEBUG(L"Online; Begin enumerating leaderboards");
	std::map< std::wstring, ILeaderboardsProvider::LeaderboardData > leaderboards;
	m_provider->enumerate(leaderboards);
	(m_sinkObject->*m_sinkMethod)(leaderboards);
	T_DEBUG(L"Online; Finished enumerating leaderboards");
}

}
