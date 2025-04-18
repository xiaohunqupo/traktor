/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Io/Reader.h"
#include "Core/Io/Writer.h"
#include "Core/Log/Log.h"
#include "Core/Misc/EnterLeave.h"
#include "Core/Misc/String.h"
#include "Core/Serialization/DeepClone.h"
#include "Core/Serialization/DeepHash.h"
#include "Core/Settings/PropertyGroup.h"
#include "Core/Settings/PropertyInteger.h"
#include "Core/System/OS.h"
#include "Core/Thread/Thread.h"
#include "Core/Thread/ThreadManager.h"
#include "Core/Timer/Timer.h"
#include "Database/Database.h"
#include "Database/Group.h"
#include "Database/Instance.h"
#include "Database/Isolate.h"
#include "Editor/DataAccessCache.h"
#include "Editor/IPipeline.h"
#include "Editor/IPipelineDb.h"
#include "Editor/IPipelineInstanceCache.h"
#include "Editor/PipelineDependency.h"
#include "Editor/PipelineDependencySet.h"
#include "Editor/Pipeline/PipelineBuilder.h"
#include "Editor/Pipeline/PipelineDependsIncremental.h"
#include "Editor/Pipeline/PipelineDependsParallel.h"
#include "Editor/Pipeline/PipelineFactory.h"
#include "Editor/Pipeline/PipelineProfiler.h"
#include "Editor/Pipeline/Memory/MemoryPipelineCache.h"

namespace traktor::editor
{
	namespace
	{

const uint32_t c_cacheVersion = 1;

class LogTargetFilter : public ILogTarget
{
public:
	explicit LogTargetFilter(ILogTarget* target, bool muted)
	:	m_target(target)
	,	m_muted(muted)
	,	m_count(0)
	{
	}

	virtual void log(uint32_t threadId, int32_t level, const wchar_t* str) override final
	{
		++m_count;
		if (m_target && !m_muted)
			m_target->log(threadId, level, str);
	}

	ILogTarget* getTarget() const { return m_target; }

	uint32_t getCount() const { return m_count; }

private:
	Ref< ILogTarget > m_target;
	bool m_muted;
	uint32_t m_count;
};

void calculateGlobalHash(
	const PipelineDependencySet* dependencySet,
	const PipelineDependency* dependency,
	uint32_t& outPipelineHash,
	uint32_t& outSourceAssetHash,
	uint32_t& outSourceDataHash,
	uint32_t& outFilesHash
)
{
	outPipelineHash += dependency->pipelineHash;
	outSourceAssetHash += dependency->sourceAssetHash;
	outSourceDataHash += dependency->sourceDataHash;
	outFilesHash += dependency->filesHash;

	for (auto child : dependency->children)
	{
		const PipelineDependency* childDependency = dependencySet->get(child);
		T_ASSERT(childDependency);

		if (childDependency == dependency)
			continue;

		if ((childDependency->flags & PdfUse) != 0)
			calculateGlobalHash(
				dependencySet,
				childDependency,
				outPipelineHash,
				outSourceAssetHash,
				outSourceDataHash,
				outFilesHash
			);
	}
}

	}

T_IMPLEMENT_RTTI_CLASS(L"traktor.editor.PipelineBuilder", PipelineBuilder, IPipelineBuilder)

PipelineBuilder::PipelineBuilder(
	PipelineFactory* pipelineFactory,
	db::Database* sourceDatabase,
	db::Database* outputDatabase,
	IPipelineCache* cache,
	IPipelineDb* pipelineDb,
	IPipelineInstanceCache* instanceCache,
	IListener* listener,
	bool verbose
)
:	m_pipelineFactory(pipelineFactory)
,	m_sourceDatabase(sourceDatabase)
,	m_outputDatabase(outputDatabase)
,	m_cache(cache)
,	m_pipelineDb(pipelineDb)
,	m_instanceCache(instanceCache)
,	m_listener(listener)
,	m_verbose(verbose)
,	m_rebuild(false)
,	m_profiler(new PipelineProfiler())
,	m_dependencySet(nullptr)
,	m_adHocDepth(0)
,	m_progressEnd(0)
,	m_progress(0)
,	m_succeeded(0)
,	m_succeededBuilt(0)
,	m_failed(0)
,	m_cacheHit(0)
,	m_cacheMiss(0)
,	m_cacheVoid(0)
{
	// If no cache is provided then use a non-persistent, in-memory, cache.
	if (!m_cache)
		m_cache = new MemoryPipelineCache();

	// Create data access memento cache.
	m_dataAccessCache = new DataAccessCache(m_profiler, m_cache);
}

bool PipelineBuilder::build(const PipelineDependencySet* dependencySet, bool rebuild)
{
	T_ANONYMOUS_VAR(ScopeIndent)(log::info);
	T_ANONYMOUS_VAR(ScopeIndent)(log::warning);
	T_ANONYMOUS_VAR(ScopeIndent)(log::error);
	T_ANONYMOUS_VAR(ScopeIndent)(log::debug);

	struct Work
	{
		Ref< const PipelineDependency > dependency;
		Ref< const Object > buildParams;
		uint32_t reason;
	};
	AlignedVector< Work > workSet;
	Timer timer;

	const uint32_t dependencyCount = dependencySet->size();
	uint32_t modifiedCount = 0;

	if (m_verbose && !rebuild)
		log::info << L"Analyzing conditions of " << dependencyCount << L" build item(s)..." << Endl;

	// Determine build reasons.
	AlignedVector< uint32_t > reasons(dependencyCount, 0);
	for (uint32_t i = 0; i < dependencyCount; ++i)
	{
		const PipelineDependency* dependency = dependencySet->get(i);
		T_ASSERT(dependency);

		if ((dependency->flags & PdfFailed) != 0)
			continue;

		// Have source asset been modified?
		if (!rebuild)
		{
			uint32_t pipelineHash = 0;
			uint32_t sourceAssetHash = 0;
			uint32_t sourceDataHash = 0;
			uint32_t filesHash = 0;

			calculateGlobalHash(
				dependencySet,
				dependency,
				pipelineHash,
				sourceAssetHash,
				sourceDataHash,
				filesHash
			);

			// Get hash entry from database.
			PipelineDependencyHash previousDependencyHash;
			if (!m_pipelineDb->getDependency(dependency->outputGuid, previousDependencyHash))
			{
				if (m_verbose)
					log::info << L"Asset \"" << dependency->outputPath << L"\" modified; not hashed." << Endl;
				reasons[i] |= PbrSourceModified;
				++modifiedCount;
			}
			else if (
				previousDependencyHash.pipelineHash != pipelineHash ||
				previousDependencyHash.sourceAssetHash != sourceAssetHash ||
				previousDependencyHash.sourceDataHash != sourceDataHash ||
				previousDependencyHash.filesHash != filesHash
			)
			{
				if (m_verbose)
				{
					log::info << L"Asset \"" << dependency->outputPath << L"\" modified; source has been modified (";

					bool prepend = false;
					if (previousDependencyHash.pipelineHash != pipelineHash)
					{
						log::info << L"pipeline";
						prepend = true;
					}
					if (previousDependencyHash.sourceAssetHash != sourceAssetHash)
					{
						if (prepend)
							log::info << L"-, ";
						log::info << L"source asset";
						prepend = true;
					}
					if (previousDependencyHash.sourceDataHash != sourceDataHash)
					{
						if (prepend)
							log::info << L"-, ";
						log::info << L"source data";
						prepend = true;
					}
					if (previousDependencyHash.filesHash != filesHash)
					{
						if (prepend)
							log::info << L"-, ";
						log::info << L"file";
						prepend = true;
					}

					log::info << L" hash mismatch)." << Endl;
				}

#if defined(_DEBUG)
				log::info << IncreaseIndent;
				log::info << L"Pipeline hash "; FormatHex(log::info, pipelineHash, 8); log::info << L" ("; FormatHex(log::info, previousDependencyHash.pipelineHash, 8); log::info << L")" << Endl;
				log::info << L"Source asset hash "; FormatHex(log::info, sourceAssetHash, 8); log::info << L" ("; FormatHex(log::info, previousDependencyHash.sourceAssetHash, 8); log::info << L")" << Endl;
				log::info << L"Source data hash "; FormatHex(log::info, sourceDataHash, 8); log::info << L" ("; FormatHex(log::info, previousDependencyHash.sourceDataHash, 8); log::info << L")" << Endl;
				log::info << L"File(s) hash "; FormatHex(log::info, filesHash, 8); log::info << L" ("; FormatHex(log::info, previousDependencyHash.filesHash, 8); log::info << L")" << Endl;
				log::info << L"---" << Endl;
				dependency->dump(log::info);
				log::info << DecreaseIndent;
#endif
				reasons[i] |= PbrSourceModified;
				++modifiedCount;
			}
		}
		else
			reasons[i] |= PbrForced;
	}

	// Collect work set.
	for (uint32_t i = 0; i < dependencyCount; ++i)
	{
		const PipelineDependency* dependency = dependencySet->get(i);
		T_ASSERT(dependency);

		SmallSet< uint32_t > visited;
		visited.insert(i);

		AlignedVector< uint32_t > children;
		children.insert(children.end(), dependency->children.begin(), dependency->children.end());

		while (!children.empty())
		{
			if (visited.find(children.back()) != visited.end())
			{
				children.pop_back();
				continue;
			}

			const PipelineDependency* childDependency = dependencySet->get(children.back());
			T_ASSERT(childDependency);

			if ((childDependency->flags & PdfUse) == 0)
			{
				children.pop_back();
				continue;
			}

			if ((reasons[children.back()] & PbrSourceModified) != 0)
				reasons[i] |= PbrDependencyModified;

			visited.insert(children.back());

			children.pop_back();
			children.insert(children.end(), childDependency->children.begin(), childDependency->children.end());
		}

		if (reasons[i] != 0)
			workSet.push_back({ dependency, nullptr, reasons[i] });
	}

	T_DEBUG(L"Pipeline build; analyzed build reasons in " << formatDuration(timer.getDeltaTime()) << L".");

	if (m_verbose && !workSet.empty())
		log::info << L"Dispatching " << (int32_t)workSet.size() << L" build(s)..." << Endl;

	m_rebuild = rebuild;
	m_progress = 0;
	m_progressEnd = (int32_t)workSet.size();
	m_succeeded = dependencyCount - m_progressEnd;
	m_succeededBuilt = 0;
	m_failed = 0;
	m_cacheHit = 0;
	m_cacheMiss = 0;
	m_cacheVoid = 0;	// No hash on source asset will result in a void.
	m_dependencySet = dependencySet;

	for (const auto& w : workSet)
	{
		if (ThreadManager::getInstance().getCurrentThread()->stopped())
			break;

		if (m_listener)
			m_listener->beginBuild(
				m_progress,
				m_progressEnd,
				w.dependency
			);

		const BuildResult result = performBuild(dependencySet, w.dependency, w.buildParams, w.reason);
		if (result == BuildResult::Succeeded || result == BuildResult::SucceededWithWarnings)
			m_succeeded++;
		else
			m_failed++;

		if (m_listener)
			m_listener->endBuild(
				m_progress,
				m_progressEnd,
				w.dependency,
				result
			);

		m_progress++;
	}

	// Log cache performance.
	if (m_cache && m_verbose)
		log::info << L"Pipeline cache; " << m_cacheHit << L" hit(s), " << m_cacheMiss << L" miss(es), " << m_cacheVoid << L" uncachable(s)." << Endl;

	// Log results.
	if (!ThreadManager::getInstance().getCurrentThread()->stopped())
	{
		if (m_verbose)
		{
			AlignedVector< std::pair< const wchar_t*, PipelineProfiler::Duration > > durations(m_profiler->getDurations().begin(), m_profiler->getDurations().end());
			std::sort(durations.begin(), durations.end(), [](const std::pair< const wchar_t*, PipelineProfiler::Duration >& lh, const std::pair< const wchar_t*, PipelineProfiler::Duration >& rh) {
				return lh.second.seconds > rh.second.seconds;
			});
			double totalDuration = 0.0;
			for (const auto& duration : durations)
				totalDuration += duration.second.seconds;
			for (const auto& duration : durations)
				log::info << formatDuration(duration.second.seconds) << L" (" << duration.second.count << L", " << str(L"%.1f", (100.0 * duration.second.seconds) / totalDuration) << L"%) in " << duration.first << Endl;
		}

		if (m_failed == 0)
			log::info << L"Build finished in " << formatDuration(timer.getElapsedTime()) << L"; " << m_succeeded << L" succeeded (" << m_succeededBuilt << L" built), " << m_failed << L" failed." << Endl;
		else
			log::error << L"Build failed in " << formatDuration(timer.getElapsedTime()) << L"; " << m_succeeded << L" succeeded (" << m_succeededBuilt << L" built), " << m_failed << L" failed." << Endl;
	}
	else
		log::info << L"Build finished; aborted." << Endl;

	return m_failed == 0;
}

Ref< ISerializable > PipelineBuilder::buildProduct(const db::Instance* sourceInstance, const ISerializable* sourceAsset, const Object* buildParams)
{
	if (!sourceAsset)
		return nullptr;

	const TypeInfo* pipelineType;
	uint32_t pipelineHash;

	if (!m_pipelineFactory->findPipelineType(type_of(sourceAsset), pipelineType, pipelineHash))
		return nullptr;

	Ref< IPipeline > pipeline = m_pipelineFactory->findPipeline(*pipelineType);
	T_ASSERT(pipeline);

	uint32_t sourceHash = pipeline->hashAsset(sourceAsset);
	if (const ISerializable* sbp = dynamic_type_cast< const ISerializable* >(buildParams))
		sourceHash += DeepHash(sbp).get();

	auto it = m_builtCache.find(sourceHash);
	if (it != m_builtCache.end())
	{
		built_cache_list_t& bcl = it->second;
		T_ASSERT(!bcl.empty());

		// Return same instance as before if pointer and hash match.
		for (built_cache_list_t::const_iterator j = bcl.begin(); j != bcl.end(); ++j)
		{
			if (j->sourceAsset == sourceAsset)
				return j->product;
		}
	}

	m_profiler->begin(*pipelineType);
	Ref< ISerializable > product = pipeline->buildProduct(this, sourceInstance, sourceAsset, buildParams);
	m_profiler->end();
	if (!product)
		return nullptr;

	m_builtCache[sourceHash].push_back({ sourceAsset, product });
	return product;
}

bool PipelineBuilder::buildAdHocOutput(const Guid& outputGuid)
{
	m_adHocBuilds.insert(outputGuid);
	return true;
}

bool PipelineBuilder::buildAdHocOutput(const ISerializable* sourceAsset, const Guid& outputGuid, const Object* buildParams)
{
	const std::wstring outputPath = L"Generated/" + outputGuid.format();
	return buildAdHocOutput(sourceAsset, outputPath, outputGuid, buildParams);
}

bool PipelineBuilder::buildAdHocOutput(const ISerializable* sourceAsset, const std::wstring& outputPath, const Guid& outputGuid, const Object* buildParams)
{
	PipelineDependencySet dependencySet;

	// Exclude filtering; already added dependencies and built ad-hocs should be excluded from further ad-hoc builds.
	auto dependencyFilter = [&](const Guid& id) -> bool {
		if (m_dependencySet->get(id) != PipelineDependencySet::DiInvalid)
			return false;

		if (m_adHocBuilds.find(id) != m_adHocBuilds.end())
			return false;

		return true;
	};

	// Scan dependencies of source asset; exclude dependencies already in work set.
	m_profiler->begin(type_of< PipelineDependsIncremental >());
	PipelineDependsIncremental pipelineDepends(
		m_pipelineFactory,
		m_sourceDatabase,
		m_outputDatabase,
		&dependencySet,
		m_pipelineDb,
		m_instanceCache,
		dependencyFilter
	);
	pipelineDepends.addDependency(
		sourceAsset,
		outputPath,
		outputGuid,
		PdfBuild | PdfForceAdd
	);
	pipelineDepends.waitUntilFinished();
	m_profiler->end();

	// Get ad-hoc asset dependency.
	const uint32_t index = dependencySet.get(outputGuid);
	if (index == PipelineDependencySet::DiInvalid)
		return false;

	T_ANONYMOUS_VAR(ScopeIndent)(log::info);

	// Build dependencies.
	bool result = true;
	for (uint32_t i = 0; i < dependencySet.size() && result; ++i)
	{
		const PipelineDependency* dependency = dependencySet.get(i);
		if ((dependency->flags & PdfBuild) == 0)
			continue;

		if (m_adHocBuilds.find(dependency->outputGuid) != m_adHocBuilds.end())
			continue;

		// Calculate hash entry.
		PipelineDependencyHash dependencyHash;
		calculateGlobalHash(
			&dependencySet,
			dependency,
			dependencyHash.pipelineHash,
			dependencyHash.sourceAssetHash,
			dependencyHash.sourceDataHash,
			dependencyHash.filesHash
		);

		Ref< IPipeline > pipeline = m_pipelineFactory->findPipeline(*dependency->pipelineType);
		T_ASSERT(pipeline);

		// Add hash of build params to source data hash, if
		// source data hash cannot be calculated we cannot permit caching of product.
		bool cachePermitted = true;
		if (index == i && buildParams != nullptr)
		{
			if (is_a< ISerializable >(buildParams))
				dependencyHash.sourceDataHash += DeepHash(static_cast< const ISerializable* >(buildParams)).get();
			else
				cachePermitted = false;
		}

		// Build output instances; keep an array of written instances as we
		// need them to update the cache for this specific build.
		RefArray< db::Instance > previousBuiltInstances;
		m_builtInstances.swap(previousBuiltInstances);
		AlignedVector< CacheKey > previousBuiltAdHocKeys;
		m_builtAdHocKeys.swap(previousBuiltAdHocKeys);

		// Get output instances from memory cache.
		if (m_cache && pipeline->shouldCache() && cachePermitted)
		{
			if (getInstancesFromCache(
				m_cache,
				{ dependency->outputGuid, dependencyHash },
				&m_builtInstances,
				&m_builtAdHocKeys
			))
			{
				for (const auto& child : m_builtAdHocKeys)
				{
					if (!getInstancesFromCache(
						m_cache,
						child,
						nullptr,
						nullptr
					))
						return false;
				}

				m_pipelineDb->setDependency(dependency->outputGuid, dependencyHash);

				previousBuiltAdHocKeys.push_back({ dependency->outputGuid, dependencyHash });
				previousBuiltAdHocKeys.insert(previousBuiltAdHocKeys.end(), m_builtAdHocKeys.begin(), m_builtAdHocKeys.end());

				m_builtInstances.swap(previousBuiltInstances);
				m_builtAdHocKeys.swap(previousBuiltAdHocKeys);

				m_adHocBuilds.insert(dependency->outputGuid);

				m_cacheHit++;
				continue;
			}
			else
				m_cacheMiss++;
		}
		else
			m_cacheVoid++;

		if (m_verbose)
			log::info << L"Building \"" << dependency->outputPath << L"\" (ad-hoc " << m_adHocDepth << L")..." << Endl;
		log::info << IncreaseIndent;

		m_adHocDepth++;
		m_profiler->begin(*dependency->pipelineType);
		result &= pipeline->buildOutput(
			this,
			&dependencySet,
			dependency,
			dependency->sourceInstanceGuid.isNotNull() ? m_sourceDatabase->getInstance(dependency->sourceInstanceGuid) : nullptr,
			dependency->sourceAsset,
			dependency->outputPath,
			dependency->outputGuid,
			(index == i) ? buildParams : nullptr,
			PbrSourceModified
		);
		m_profiler->end();
		m_adHocDepth--;

		if (result && m_cache && pipeline->shouldCache() && cachePermitted)
		{
			putInstancesInCache(
				m_cache,
				{ dependency->outputGuid, dependencyHash },
				m_builtInstances,
				m_builtAdHocKeys
			);
			
			previousBuiltAdHocKeys.push_back({ dependency->outputGuid, dependencyHash });
			previousBuiltAdHocKeys.insert(previousBuiltAdHocKeys.end(), m_builtAdHocKeys.begin(), m_builtAdHocKeys.end());
		}

		// Store dependency hash in database so getInstancesFromCache only touches
		// instances when being called multiple times, each ad-hoc might reference same child dependencies.
		if (result)
			m_pipelineDb->setDependency(dependency->outputGuid, dependencyHash);

		// Restore previous set but also insert built instances from synthesized build;
		// when caching is enabled then synthesized built instances should be included in parent build as well.
		m_builtInstances.swap(previousBuiltInstances);
		m_builtAdHocKeys.swap(previousBuiltAdHocKeys);

		m_adHocBuilds.insert(dependency->outputGuid);

		log::info << DecreaseIndent;
		if (m_verbose)
		{
			if (result)
				log::info << L"Build \"" << dependency->outputPath << L"\" (ad-hoc) successful." << Endl;
			else
				log::info << L"Build \"" << dependency->outputPath << L"\" (ad-hoc, " << type_name(pipeline) << L") failed." << Endl;
		}
	}

	return result;
}

uint32_t PipelineBuilder::calculateInclusiveHash(const ISerializable* sourceAsset) const
{
	// Scan dependencies of source asset.
	PipelineDependencySet dependencySet;
	PipelineDependsIncremental pipelineDepends(m_pipelineFactory, m_sourceDatabase, m_outputDatabase, &dependencySet, m_pipelineDb, m_instanceCache);
	pipelineDepends.addDependency(sourceAsset);
	pipelineDepends.waitUntilFinished();

	// Calculate hash of source and append hashes of all it's dependencies.
	uint32_t hash = DeepHash(sourceAsset).get();
	for (uint32_t i = 0; i < dependencySet.size(); ++i)
	{
		const PipelineDependency* dependency = dependencySet.get(i);
		T_ASSERT(dependency);

		hash += dependency->pipelineHash;
		hash += dependency->sourceAssetHash;
		hash += dependency->sourceDataHash;
		hash += dependency->filesHash;
	}

	return hash;
}

Ref< ISerializable > PipelineBuilder::getBuildProduct(const ISerializable* sourceAsset)
{
	if (!sourceAsset)
		return nullptr;

	const uint32_t sourceHash = DeepHash(sourceAsset).get();

	const auto it = m_builtCache.find(sourceHash);
	if (it == m_builtCache.end())
		return nullptr;

	for (const auto& e : it->second)
	{
		if (e.sourceAsset == sourceAsset)
			return e.product;
	}
	return nullptr;
}

Ref< db::Instance > PipelineBuilder::createOutputInstance(const std::wstring& instancePath, const Guid& instanceGuid)
{
	Ref< db::Instance > instance;

	if (instanceGuid.isNull() || !instanceGuid.isValid())
	{
		log::error << L"Invalid guid for output instance." << Endl;
		return nullptr;
	}

	instance = m_outputDatabase->getInstance(instanceGuid);
	if (instance && instancePath != instance->getPath())
	{
		// Instance with given guid already exist somewhere else, we need to
		// remove it first.
		bool result = false;
		if (instance->checkout())
		{
			result = instance->remove();
			result &= instance->commit();
		}
		if (!result)
		{
			log::error << L"Unable to remove existing instance \"" << instance->getPath() << L"\"." << Endl;
			return nullptr;
		}
	}

	instance = m_outputDatabase->createInstance(
		instancePath,
		db::CifReplaceExisting,
		&instanceGuid
	);
	if (instance)
	{
		m_builtInstances.push_back(instance);
		return instance;
	}
	else
	{
		log::error << L"Unable to create output instance \"" << instancePath << L"\"." << Endl;
		return nullptr;
	}
}

db::Database* PipelineBuilder::getOutputDatabase() const
{
	return m_outputDatabase;
}

db::Database* PipelineBuilder::getSourceDatabase() const
{
	return m_sourceDatabase;
}

Ref< const ISerializable > PipelineBuilder::getObjectReadOnly(const Guid& instanceGuid)
{
	Ref< const ISerializable > object;
	if (instanceGuid.isNotNull())
	{
		m_profiler->begin(L"PipelineBuilder::getObjectReadOnly");
		object = m_instanceCache->getObjectReadOnly(instanceGuid);
		m_profiler->end();
	}
	return object;
}

DataAccessCache* PipelineBuilder::getDataAccessCache() const
{
	return m_dataAccessCache;
}

PipelineProfiler* PipelineBuilder::getProfiler() const
{
	return m_profiler;
}

IPipelineBuilder::BuildResult PipelineBuilder::performBuild(
	const PipelineDependencySet* dependencySet,
	const PipelineDependency* dependency,
	const Object* buildParams,
	uint32_t reason
)
{
	if (!dependency->pipelineType)
		return BuildResult::Failed;

	// Calculate recursive hash entry.
	PipelineDependencyHash currentDependencyHash;
	calculateGlobalHash(
		dependencySet,
		dependency,
		currentDependencyHash.pipelineHash,
		currentDependencyHash.sourceAssetHash,
		currentDependencyHash.sourceDataHash,
		currentDependencyHash.filesHash
	);

	// Skip no-build asset; just update hash.
	if ((dependency->flags & PdfBuild) == 0)
	{
		m_pipelineDb->setDependency(dependency->outputGuid, currentDependencyHash);
		return BuildResult::Succeeded;
	}

	T_ANONYMOUS_VAR(ScopeIndent)(log::info);

	log::info << L"Building \"" << dependency->outputPath << L"\"..." << Endl;
	log::info << IncreaseIndent;

	Ref< IPipeline > pipeline = m_pipelineFactory->findPipeline(*dependency->pipelineType);
	T_ASSERT(pipeline);

	m_builtInstances.resize(0);
	m_builtAdHocKeys.resize(0);

	// Get output instances from cache.
	if (m_cache && pipeline->shouldCache())
	{
		if (getInstancesFromCache(
			m_cache,
			{ dependency->outputGuid, currentDependencyHash },
			&m_builtInstances,
			&m_builtAdHocKeys
		))
		{
			for (const auto& child : m_builtAdHocKeys)
			{
				if (!getInstancesFromCache(
					m_cache,
					child,
					nullptr,
					nullptr
				))
					return BuildResult::Failed;
			}

			m_pipelineDb->setDependency(dependency->outputGuid, currentDependencyHash);

			m_cacheHit++;
			m_succeededBuilt++;
			return BuildResult::Succeeded;
		}
		else
			m_cacheMiss++;
	}
	else
		m_cacheVoid++;

	LogTargetFilter infoTarget(log::info.getLocalTarget(), !m_verbose);
	LogTargetFilter warningTarget(log::warning.getLocalTarget(), false);
	LogTargetFilter errorTarget(log::error.getLocalTarget(), false);

	log::info.setLocalTarget(&infoTarget);
	log::warning.setLocalTarget(&warningTarget);
	log::error.setLocalTarget(&errorTarget);

	Timer timer;

	// Build asset through pipeline, this might call back buildAdHoc or buildProduct.
	// So it's safe to assume this is the outmost build thus we can utilize cache
	// differently where each ad-hoc output is cached individually and referenced from
	// this output cache.
	m_profiler->begin(*dependency->pipelineType);
	const bool result = pipeline->buildOutput(
		this,
		dependencySet,
		dependency,
		dependency->sourceInstanceGuid.isNotNull() ? m_sourceDatabase->getInstance(dependency->sourceInstanceGuid) : nullptr,
		dependency->sourceAsset,
		dependency->outputPath,
		dependency->outputGuid,
		buildParams,
		reason
	);
	m_profiler->end();

	if (result)
		m_succeededBuilt++;

	const double buildTime = timer.getElapsedTime();

	log::info.setLocalTarget(infoTarget.getTarget());
	log::warning.setLocalTarget(warningTarget.getTarget());
	log::error.setLocalTarget(errorTarget.getTarget());

	if (result && m_cache && pipeline->shouldCache())
	{
		putInstancesInCache(
			m_cache,
			{ dependency->outputGuid, currentDependencyHash },
			m_builtInstances,
			m_builtAdHocKeys
		);
	}

	if (result)
		m_pipelineDb->setDependency(dependency->outputGuid, currentDependencyHash);

	log::info << DecreaseIndent;

	if (m_verbose)
	{
		if (result)
			log::info << L"Build \"" << dependency->outputPath << L"\" successful." << Endl;
		else
			log::info << L"Build \"" << dependency->outputPath << L"\" failed (" << type_name(pipeline) << L")." << Endl;
	}

	m_builtInstances.resize(0);
	m_builtAdHocKeys.resize(0);

	if (result)
		return (warningTarget.getCount() + errorTarget.getCount()) > 0 ? BuildResult::SucceededWithWarnings : BuildResult::Succeeded;
	else
		return BuildResult::Failed;
}

bool PipelineBuilder::putInstancesInCache(
	IPipelineCache* cache,
	const CacheKey& key,
	const RefArray< db::Instance >& instances,
	const AlignedVector< CacheKey >& children
) const
{
	T_ANONYMOUS_VAR(EnterLeave)(
		[=]() { m_profiler->begin(L"PipelineBuilder::putInstancesInCache"); },
		[=]() { m_profiler->end(); }
	);

	Ref< IStream > stream = cache->put(key.guid, key.hash);
	if (!stream)
		return false;

	Writer writer(stream);

	writer << (uint32_t)c_cacheVersion;
	writer << (uint32_t)instances.size();
	writer << (uint32_t)children.size();

	// Write directory.
	for (uint32_t i = 0; i < (uint32_t)instances.size(); ++i)
	{
		const Guid instanceId = instances[i]->getGuid();
		const std::wstring instancePath = instances[i]->getPath();
		if (writer.write((const uint8_t*)instanceId, 16) != 16)
			return false;
		writer << instancePath;
	}

	// Write child dependencies.
	for (uint32_t i = 0; i < (uint32_t)children.size(); ++i)
	{
		const Guid childId = children[i].guid;
		if (writer.write((const uint8_t*)childId, 16) != 16)
			return false;
		writer << children[i].hash.pipelineHash;
		writer << children[i].hash.sourceAssetHash;
		writer << children[i].hash.sourceDataHash;
		writer << children[i].hash.filesHash;
	}

	// Write instances.
	for (uint32_t i = 0; i < (uint32_t)instances.size(); ++i)
	{
		if (!db::Isolate::createIsolatedInstance(instances[i], stream))
			return false;
	}

	stream->close();
	
	// Commit cached item.
	if (!cache->commit(key.guid, key.hash))
		return false;

	return true;
}

bool PipelineBuilder::getInstancesFromCache(
	IPipelineCache* cache,
	const CacheKey& key,
	RefArray< db::Instance >* outInstances,
	AlignedVector< CacheKey >* outChildren
) const
{
	T_ANONYMOUS_VAR(EnterLeave)(
		[=]() { m_profiler->begin(L"PipelineBuilder::getInstancesFromCache"); },
		[=]() { m_profiler->end(); }
	);

	struct DirectoryEntry
	{
		Guid instanceId;
		std::wstring instancePath;
	};

	bool create = true;
	bool result = true;

	// Open stream to cached blob.
	Ref< IStream > stream = cache->get(key.guid, key.hash);
	if (!stream)
		return false;

	// Compare hash to last output to determine if we need to create output instances or if
	// they should already exist in output database.
	PipelineDependencyHash lastOutputHash;
	if (m_pipelineDb->getDependency(key.guid, lastOutputHash))
	{
		if (lastOutputHash == key.hash)
			create = false;
	}

	Reader reader(stream);

	// Ensure cache is compatible.
	uint32_t version = 0;
	reader >> version;
	if (version < c_cacheVersion)
	{
		stream->close();
		return false;
	}

	// Read directory from stream.
	uint32_t instanceCount = 0;
	uint32_t childCount = 0;

	reader >> instanceCount;
	reader >> childCount;

	if (instanceCount == 0 && childCount == 0)
	{
		stream->close();
		return true;
	}

	AlignedVector< DirectoryEntry > directory(instanceCount);
	for (uint32_t i = 0; i < instanceCount; ++i)
	{
		auto& dirent = directory[i];

		uint8_t instanceId[16];
		if (reader.read(instanceId, 16) != 16)
			return false;
		if (!(dirent.instanceId = Guid(instanceId)).isNotNull())
			return false;

		reader >> dirent.instancePath;
		if (dirent.instancePath.empty())
			return false;
	}

	AlignedVector< CacheKey > children(childCount);
	for (uint32_t i = 0; i < childCount; ++i)
	{
		auto& child = children[i];

		uint8_t dependencyId[16];
		if (reader.read(dependencyId, 16) != 16)
			return false;
		if (!(child.guid = Guid(dependencyId)).isNotNull())
			return false;

		reader >> child.hash.pipelineHash;
		reader >> child.hash.sourceAssetHash;
		reader >> child.hash.sourceDataHash;
		reader >> child.hash.filesHash;
	}

	// Fetch or create instances.
	if (outInstances)
	{
		outInstances->resize(0);
		outInstances->reserve(instanceCount);
	}

	if (create)
	{
		for (uint32_t i = 0; i < instanceCount; ++i)
		{
			const Path instancePath(directory[i].instancePath);

			Ref< db::Group > group = m_outputDatabase->createGroup(instancePath.getPathOnlyNoVolume());
			if (!group)
			{
				result = false;
				break;
			}

			Ref< db::Instance > instance = db::Isolate::createInstanceFromIsolation(group, stream);
			if (instance)
			{
				if (outInstances)
					outInstances->push_back(instance);
			}
			else
			{
				result = false;
				break;
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < instanceCount; ++i)
		{
			Ref< db::Instance > instance = m_outputDatabase->getInstance(directory[i].instanceId);
			if (instance)
			{
				if (outInstances)
					outInstances->push_back(instance);
			}
			else
			{
				result = false;
				break;
			}
		}
	}

	stream->close();

	if (!result)
		return false;

	if (outChildren)
		*outChildren = children;

	return true;
}

}
