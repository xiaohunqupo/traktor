/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Serialization/ISerializer.h"
#include "Core/Serialization/Member.h"
#include "Core/Serialization/MemberRef.h"
#include "Core/Serialization/MemberStl.h"
#include "Core/System/Environment.h"
#include "Pipeline/App/PipelineParameters.h"

namespace traktor
{

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.PipelineParameters", 0, PipelineParameters, ISerializable)

PipelineParameters::PipelineParameters(
	const Environment* environment,
	const std::wstring& workingDirectory,
	const std::wstring& settings,
	bool verbose,
	bool progress,
	bool rebuild,
	bool noCache,
	const std::vector< Guid >& roots
)
:	m_environment(environment)
,	m_workingDirectory(workingDirectory)
,	m_settings(settings)
,	m_verbose(verbose)
,	m_progress(progress)
,	m_rebuild(rebuild)
,	m_noCache(noCache)
,	m_roots(roots)
{
}

void PipelineParameters::serialize(ISerializer& s)
{
	s >> MemberRef< const Environment >(L"environment", m_environment);
	s >> Member< std::wstring >(L"workingDirectory", m_workingDirectory);
	s >> Member< std::wstring >(L"settings", m_settings);
	s >> Member< bool >(L"verbose", m_verbose);
	s >> Member< bool >(L"progress", m_progress);
	s >> Member< bool >(L"rebuild", m_rebuild);
	s >> Member< bool >(L"noCache", m_noCache);
	s >> MemberStlVector< Guid >(L"roots", m_roots);
}

}
