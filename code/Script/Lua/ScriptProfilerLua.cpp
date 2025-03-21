/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Log/Log.h"
#include "Core/Misc/String.h"
#include "Core/Misc/TString.h"
#include "Script/Lua/ScriptContextLua.h"
#include "Script/Lua/ScriptManagerLua.h"
#include "Script/Lua/ScriptProfilerLua.h"
#include "Script/Lua/ScriptUtilitiesLua.h"

namespace traktor::script
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.script.ScriptProfilerLua", ScriptProfilerLua, IScriptProfiler)

ScriptProfilerLua::ScriptProfilerLua(ScriptManagerLua* scriptManager, lua_State* luaState)
:	m_scriptManager(scriptManager)
,	m_luaState(luaState)
{
	m_timer.reset();
}

void ScriptProfilerLua::addListener(IListener* listener)
{
	m_listeners.insert(listener);
}

void ScriptProfilerLua::removeListener(IListener* listener)
{
	m_listeners.erase(listener);
}

void ScriptProfilerLua::notifyCallEnter()
{
	Guid scriptId;
	std::wstring name;
	for (auto listener : m_listeners)
		listener->callEnter(scriptId, name);
}

void ScriptProfilerLua::notifyCallLeave()
{
	Guid scriptId;
	std::wstring name;
	for (auto listener : m_listeners)
		listener->callLeave(scriptId, name);
}

void ScriptProfilerLua::hookCallback(lua_State* L, lua_Debug* ar)
{
	if (ar->event == LUA_HOOKLINE)
		return;

	ScriptContextLua* currentContext = m_scriptManager->m_lockContext;
	if (!currentContext)
		return;

	lua_getinfo(L, "Sln", ar);
	if (!ar->name)
		return;

	const double timeStamp = m_timer.getElapsedTime();

	if (ar->event == LUA_HOOKCALL || ar->event == LUA_HOOKTAILCALL)
	{
		ProfileStack& ps = m_stack.push_back();
		ps.timeStamp = timeStamp;
		ps.childDuration = 0.0;
	}
	else if (ar->event == LUA_HOOKRET)
	{
		// Make sure we don't break if hooks are behaving strange.
		if (m_stack.empty())
			return;

		Guid scriptId;
		std::wstring name = mbstows(ar->name ? ar->name : "(Unnamed)");

		if (*ar->what != 'C')
		{
			int32_t currentLine = 0;
			if (ar->linedefined >= 1)
			{
				currentLine = ar->linedefined - 1;
				scriptId.create(mbstows(ar->source));
			}
			name += L":" + toString(currentLine);
		}

		ProfileStack& ps = m_stack.back();

		const double inclusiveDuration = timeStamp - ps.timeStamp;
		const double exclusiveDuration = inclusiveDuration - ps.childDuration;

		// Notify all listeners about new measurement.
		for (auto listener : m_listeners)
			listener->callMeasured(scriptId, name, 1, inclusiveDuration, exclusiveDuration);

		m_stack.pop_back();

		// Accumulate child call duration so we can isolate how much time is spent in function and not in calls.
		if (!m_stack.empty())
			m_stack.back().childDuration += inclusiveDuration;
	}
}

}
