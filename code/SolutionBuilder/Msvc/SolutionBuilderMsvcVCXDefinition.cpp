/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "SolutionBuilder/Msvc/SolutionBuilderMsvcVCXDefinition.h"

#include "Core/Io/FileSystem.h"
#include "Core/Io/StringOutputStream.h"
#include "Core/Log/Log.h"
#include "Core/Misc/String.h"
#include "Core/Serialization/ISerializer.h"
#include "Core/Serialization/Member.h"
#include "Core/Serialization/MemberComposite.h"
#include "Core/Serialization/MemberStl.h"
#include "Core/System/ResolveEnv.h"
#include "SolutionBuilder/Configuration.h"
#include "SolutionBuilder/ExternalDependency.h"
#include "SolutionBuilder/File.h"
#include "SolutionBuilder/Msvc/GeneratorContext.h"
#include "SolutionBuilder/Project.h"
#include "SolutionBuilder/ProjectDependency.h"
#include "SolutionBuilder/Solution.h"

namespace traktor::sb
{

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.sb.SolutionBuilderMsvcVCXDefinition", 1, SolutionBuilderMsvcVCXDefinition, ISerializable)

bool SolutionBuilderMsvcVCXDefinition::generate(
	GeneratorContext& context,
	const Solution* solution,
	const Project* project,
	const Configuration* configuration,
	OutputStream& os) const
{
	StringOutputStream ssip, ssd, ssl, sslp;

	for (auto includePath : configuration->getIncludePaths())
		ssip << context.getVCProjectRelativePath(includePath, m_resolvePaths) << L";";

	for (auto definition : configuration->getDefinitions())
		ssd << definition << L";";

	std::set< std::wstring > libraries, libraryPaths;
	collectAdditionalLibraries(context.getSolutionPathName(), solution, project, configuration, libraries, libraryPaths);

	for (const std::wstring& library : libraries)
		ssl << library << L";";

	for (std::set< std::wstring >::const_iterator i = libraryPaths.begin(); i != libraryPaths.end(); ++i)
	{
		const std::wstring libraryPath = context.getVCProjectRelativePath(*i, m_resolvePaths);
		sslp << libraryPath << L";";
	}

	context.set(L"PROJECT_NAME", project->getName());
	context.set(L"PROJECT_INCLUDE_PATHS", ssip.str());
	context.set(L"PROJECT_DEFINITIONS", ssd.str());
	context.set(L"PROJECT_LIBRARIES", ssl.str());
	context.set(L"PROJECT_LIBRARY_PATHS", sslp.str());

	const wchar_t* c_warningLevels[] = {
		L"TurnOffAllWarnings", // WlNoWarnings
		L"Level1",			   // WlCriticalOnly
		L"Level3",			   // WlCompilerDefault
		L"EnableAllWarnings",  // WlAllWarnings
	};

	context.set(L"PROJECT_WARNING_LEVEL", c_warningLevels[configuration->getWarningLevel()]);

	const std::wstring aco = configuration->getAdditionalCompilerOptions();
	if (!aco.empty())
		context.set(L"PROJECT_ADDITIONAL_COMPILER_OPTIONS", aco + L" ");
	else
		context.set(L"PROJECT_ADDITIONAL_COMPILER_OPTIONS", L"");

	const std::wstring alo = configuration->getAdditionalLinkerOptions();
	if (!alo.empty())
		context.set(L"PROJECT_ADDITIONAL_LINKER_OPTIONS", alo + L" ");
	else
		context.set(L"PROJECT_ADDITIONAL_LINKER_OPTIONS", L"");

	context.set(L"MODULE_DEFINITION_FILE", L"");
	findDefinitions(context, solution, project, project->getItems());

	os << L"<" << m_name << L">" << Endl;
	os << IncreaseIndent;

	for (const auto& option : m_options)
	{
		const std::wstring value = context.format(option.value);
		os << L"<" << option.name << L">" << resolveEnv(value, nullptr) << L"</" << option.name << L">" << Endl;
	}

	os << DecreaseIndent;
	os << L"</" << m_name << L">" << Endl;

	return true;
}

void SolutionBuilderMsvcVCXDefinition::serialize(ISerializer& s)
{
	T_FATAL_ASSERT(s.getVersion() >= 1);

	s >> Member< std::wstring >(L"name", m_name);
	s >> Member< std::wstring >(L"fileTypes", m_fileTypes);
	s >> Member< bool >(L"resolvePaths", m_resolvePaths);
	s >> MemberStlVector< Option, MemberComposite< Option > >(L"options", m_options);
}

void SolutionBuilderMsvcVCXDefinition::Option::serialize(ISerializer& s)
{
	s >> Member< std::wstring >(L"name", name);
	s >> Member< std::wstring >(L"value", value);
}

void SolutionBuilderMsvcVCXDefinition::collectAdditionalLibraries(
	const Path& solutionPathName,
	const Solution* solution,
	const Project* project,
	const Configuration* configuration,
	std::set< std::wstring >& outAdditionalLibraries,
	std::set< std::wstring >& outAdditionalLibraryPaths) const
{
	outAdditionalLibraries.insert(
		configuration->getLibraries().begin(),
		configuration->getLibraries().end());

	outAdditionalLibraryPaths.insert(
		configuration->getLibraryPaths().begin(),
		configuration->getLibraryPaths().end());

	for (auto dependency : project->getDependencies())
	{
		// Traverse all static library dependencies and at their "additional libraries" as well.
		if (const ProjectDependency* projectDependency = dynamic_type_cast< const ProjectDependency* >(dependency))
		{
			const Configuration* dependentConfiguration = projectDependency->getProject()->getConfiguration(configuration->getName());
			if (!dependentConfiguration)
			{
				log::warning << L"Unable to add dependency \"" << projectDependency->getProject()->getName() << L"\", no matching configuration found" << Endl;
				continue;
			}

			if (dependentConfiguration->getTargetFormat() == Configuration::TfStaticLibrary)
				collectAdditionalLibraries(
					solutionPathName,
					solution,
					projectDependency->getProject(),
					dependentConfiguration,
					outAdditionalLibraries,
					outAdditionalLibraryPaths);

			// If "consumer library path" set then include it as well.
			const std::wstring& consumerLibraryPath = dependentConfiguration->getConsumerLibraryPath();
			if (!consumerLibraryPath.empty())
			{
				const Path libraryPath = Path(solution->getAggregateOutputPath()) + Path(consumerLibraryPath);
				outAdditionalLibraryPaths.insert(libraryPath.getPathName());
			}
		}

		// Add products from external dependencies and their "additional libraries" as well.
		if (const ExternalDependency* externalDependency = dynamic_type_cast< const ExternalDependency* >(dependency))
		{
			const Configuration* externalConfiguration = externalDependency->getProject()->getConfiguration(configuration->getName());
			if (!externalConfiguration)
			{
				log::warning << L"Unable to add external dependency \"" << externalDependency->getProject()->getName() << L"\", no matching configuration found" << Endl;
				continue;
			}

			const std::wstring externalSolutionPathName = externalDependency->getSolutionFileName();

			const Path absoluteExternalSolutionPathName = FileSystem::getInstance().getAbsolutePath(
				Path(solutionPathName),
				Path(externalSolutionPathName));

			if (externalDependency->getLink() != Dependency::LnkNo)
			{
				std::wstring externalRootPath = externalDependency->getSolution()->getRootPath();

				const Path absoluteExternalRootPath = FileSystem::getInstance().getAbsolutePath(
					Path(absoluteExternalSolutionPathName.getPathOnlyOS()),
					Path(externalRootPath));

				const std::wstring externalProjectPath = absoluteExternalRootPath.getPathNameOS() + L"/" + toLower(externalConfiguration->getName());
				const std::wstring externalProjectName = externalDependency->getProject()->getName() + L".lib";

				outAdditionalLibraries.insert(externalProjectName);
				outAdditionalLibraryPaths.insert(externalProjectPath);
			}

			if (externalConfiguration->getTargetFormat() == Configuration::TfStaticLibrary)
				collectAdditionalLibraries(
					absoluteExternalSolutionPathName,
					externalDependency->getSolution(),
					externalDependency->getProject(),
					externalConfiguration,
					outAdditionalLibraries,
					outAdditionalLibraryPaths);

			// If "consumer library path" set then include it as well.
			const std::wstring& consumerLibraryPath = externalConfiguration->getConsumerLibraryPath();
			if (!consumerLibraryPath.empty())
			{
				const Path libraryPath = Path(externalDependency->getSolution()->getAggregateOutputPath()) + Path(consumerLibraryPath);
				outAdditionalLibraryPaths.insert(libraryPath.getPathName());
			}
		}
	}
}

void SolutionBuilderMsvcVCXDefinition::findDefinitions(
	GeneratorContext& context,
	const Solution* solution,
	const Project* project,
	const RefArray< ProjectItem >& items) const
{
	const Path rootPath = FileSystem::getInstance().getAbsolutePath(context.get(L"PROJECT_PATH"));
	for (auto item : items)
	{
		if (const sb::File* file = dynamic_type_cast< const sb::File* >(item))
		{
			std::set< Path > systemFiles;
			file->getSystemFiles(project->getSourcePath(), systemFiles);
			for (std::set< Path >::iterator j = systemFiles.begin(); j != systemFiles.end(); ++j)
			{
				if (compareIgnoreCase(j->getExtension(), L"def") == 0)
				{
					Path relativePath;
					FileSystem::getInstance().getRelativePath(
						*j,
						rootPath,
						relativePath);
					context.set(L"MODULE_DEFINITION_FILE", relativePath.getPathName());
				}
			}
		}
		findDefinitions(context, solution, project, item->getItems());
	}
}

}
