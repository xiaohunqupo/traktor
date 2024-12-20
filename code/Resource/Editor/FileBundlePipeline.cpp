#pragma optimize( "", off )

/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Io/FileSystem.h"
#include "Core/Io/StreamCopy.h"
#include "Core/Log/Log.h"
#include "Core/Misc/String.h"
#include "Core/Serialization/DeepHash.h"
#include "Core/Settings/PropertyString.h"
#include "Database/Database.h"
#include "Database/Group.h"
#include "Database/Instance.h"
#include "Editor/IPipelineBuilder.h"
#include "Editor/PipelineDependencySet.h"
#include "Editor/IPipelineDepends.h"
#include "Editor/IPipelineSettings.h"
#include "Editor/PipelineDependency.h"
#include "Resource/FileBundle.h"
#include "Resource/Editor/FileBundleAsset.h"
#include "Resource/Editor/FileBundlePipeline.h"

namespace traktor::resource
{
	namespace
	{

void collectFiles(const Path& mask, bool recursive, RefArray< File >& outFiles)
{
	RefArray< File > files = FileSystem::getInstance().find(mask);
	for (auto f : files)
	{
		if (!f->isDirectory())
		{
			outFiles.push_back(f);
		}
		else if (recursive)
		{
			auto p = f->getPath();
			auto fn = p.getFileName();
			if (fn != L"." && fn != L"..")
			{
				collectFiles(
					p.getPathName() + L"/" + mask.getFileName(),
					recursive,
					outFiles
				);
			}
		}
	}
}

	}

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.editor.FileBundlePipeline", 4, FileBundlePipeline, editor::IPipeline)

bool FileBundlePipeline::create(const editor::IPipelineSettings* settings, db::Database* database)
{
	m_assetPath = settings->getPropertyExcludeHash< std::wstring >(L"Pipeline.AssetPath", L"");
	return true;
}

void FileBundlePipeline::destroy()
{
}

TypeInfoSet FileBundlePipeline::getAssetTypes() const
{
	return makeTypeInfoSet< FileBundleAsset >();
}

bool FileBundlePipeline::shouldCache() const
{
	return false;
}

uint32_t FileBundlePipeline::hashAsset(const ISerializable* sourceAsset) const
{
	return DeepHash(sourceAsset).get();
}

bool FileBundlePipeline::buildDependencies(
	editor::IPipelineDepends* pipelineDepends,
	const db::Instance* sourceInstance,
	const ISerializable* sourceAsset,
	const std::wstring& outputPath,
	const Guid& outputGuid
) const
{
	const FileBundleAsset* bundleAsset = mandatory_non_null_type_cast< const FileBundleAsset* >(sourceAsset);

	const auto& patterns = bundleAsset->getPatterns();
	for (auto pattern : patterns)
	{
		Path mask = Path(m_assetPath) + Path(pattern.sourceBase) + Path(pattern.sourceMask);

		RefArray< File > files;
		collectFiles(mask, pattern.recursive, files);

		for (auto file : files)
		{
			const Path assetPath = FileSystem::getInstance().getAbsolutePath(Path(m_assetPath));
			const Path filePath = FileSystem::getInstance().getAbsolutePath(file->getPath());

			Path fp;
			if (!FileSystem::getInstance().getRelativePath(filePath, assetPath, fp))
			{
				log::error << L"FileBundlePipeline failed; unable to resolve paths." << Endl;
				return false;
			}

			pipelineDepends->addDependency(Path(m_assetPath), fp.getPathName());
		}
	}

	return true;
}

bool FileBundlePipeline::buildOutput(
	editor::IPipelineBuilder* pipelineBuilder,
	const editor::PipelineDependencySet* dependencySet,
	const editor::PipelineDependency* dependency,
	const db::Instance* sourceInstance,
	const ISerializable* sourceAsset,
	const std::wstring& outputPath,
	const Guid& outputGuid,
	const Object* buildParams,
	uint32_t reason
) const
{
	const FileBundleAsset* bundleAsset = mandatory_non_null_type_cast< const FileBundleAsset* >(sourceAsset);

	Ref< FileBundle > bundle = new FileBundle();

	Ref< db::Instance > outputInstance = pipelineBuilder->createOutputInstance(outputPath, outputGuid);
	if (!outputInstance)
	{
		log::error << L"FileBundlePipeline failed; unable to create output instance" << Endl;
		return false;
	}

	const auto& patterns = bundleAsset->getPatterns();
	for (auto pattern : patterns)
	{
		Path mask = Path(m_assetPath) + pattern.sourceBase + Path(pattern.sourceMask);

		RefArray< File > files;
		collectFiles(mask, pattern.recursive, files);

		for (auto file : files)
		{
			const Path assetPath = FileSystem::getInstance().getAbsolutePath(Path(m_assetPath));
			const Path filePath = FileSystem::getInstance().getAbsolutePath(file->getPath());

			Path fp;
			if (!FileSystem::getInstance().getRelativePath(
				filePath,
				assetPath + pattern.sourceBase,
				fp
			))
			{
				log::error << L"FileBundlePipeline failed; unable to resolve paths." << Endl;
				return false;
			}

			const Path outputPath = pattern.outputBase + fp;
			const Guid dataId = Guid::create();

			Ref< IStream > is = FileSystem::getInstance().open(file->getPath(), File::FmRead);
			if (!is)
			{
				log::error << L"FileBundlePipeline failed; unable to open file \"" << file->getPath().getPathName() << L"\"." << Endl;
				return false;
			}

			Ref< IStream > os = outputInstance->writeData(dataId.format());
			if (!os)
			{
				log::error << L"FileBundlePipeline failed; unable to create data stream." << Endl;
				return false;
			}

			if (!StreamCopy(os, is).execute())
			{
				log::error << L"FileBundlePipeline failed; copy file into data stream." << Endl;
				return false;
			}

			bundle->m_dataIds.insert(std::make_pair(
				toLower(outputPath.getPathName()),
				dataId.format()
			));

			log::info << L"FileBundlePipeline; Source \"" << file->getPath().getPathName() << L"\" copied into blob \"" << outputPath.getPathName() << L"\"." << Endl;
		}
	}

	outputInstance->setObject(bundle);

	if (!outputInstance->commit())
	{
		log::error << L"FileBundlePipeline failed; unable to commit output instance" << Endl;
		return false;
	}

	return true;
}

Ref< ISerializable > FileBundlePipeline::buildProduct(
	editor::IPipelineBuilder* pipelineBuilder,
	const db::Instance* sourceInstance,
	const ISerializable* sourceAsset,
	const Object* buildParams
) const
{
	T_FATAL_ERROR;
	return nullptr;
}

}
