/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Rtti/TypeInfo.h"

#if defined(T_STATIC)
#	include "Script/Lua/ScriptManagerLua.h"

namespace traktor::script
{

#	if defined(T_SCRIPT_LUAJIT_EXPORT)
extern "C" void __module__Traktor_Script_LuaJIT()
#	else
extern "C" void __module__Traktor_Script_Lua()
#	endif
{
	T_FORCE_LINK_REF(ScriptManagerLua);
}

}

#endif
