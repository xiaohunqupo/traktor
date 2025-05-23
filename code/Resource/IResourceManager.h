/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Object.h"
#include "Core/Guid.h"
#include "Resource/Id.h"
#include "Resource/IdProxy.h"
#include "Resource/Proxy.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RESOURCE_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::resource
{

class IResourceFactory;
class ResourceBundle;
class ResourceHandle;

/*! Resource manager statistics.
 * \ingroup Resource
 */
struct ResourceManagerStatistics
{
	uint32_t residentCount = 0;		//!< Number of resident resources.
	uint32_t exclusiveCount = 0;	//!< Number of exclusive (non-shareable) resources.
};

/*! Resource manager interface.
 * \ingroup Resource
 */
class T_DLLCLASS IResourceManager : public Object
{
	T_RTTI_CLASS;

public:
	virtual void destroy() = 0;

	/*! Add resource factory to manager.
	 *
	 * \param factory Resource factory.
	 */
	virtual void addFactory(const IResourceFactory* factory) = 0;

	/*! Remove resource factory from manager.
	 *
	 * \param factory Resource factory.
	 */
	virtual void removeFactory(const IResourceFactory* factory) = 0;

	/*! Remove all resource factories. */
	virtual void removeAllFactories() = 0;

	/*! Load all resources in bundle.
	 *
	 * \param bundle Resource bundle.
	 * \return True if all resources loaded successfully.
	 */
	virtual bool load(const ResourceBundle* bundle) = 0;

	/*! Bind handle to resource identifier.
	 *
	 * \param productType Type of product.
	 * \param guid Resource identifier.
	 * \return Resource handle.
	 */
	virtual Ref< ResourceHandle > bind(const TypeInfo& productType, const Guid& guid) = 0;

	/*! Reload resource.
	 *
	 * \param guid Resource identifier.
	 * \param flushedOnly Reload flushed resources only.
	 * \retunr True if resource was found and reloaded.
	 */
	virtual bool reload(const Guid& guid, bool flushedOnly) = 0;

	/*! Reload all resources of given type.
	 *
	 * \param productType Type of product.
	 * \param flushedOnly Reload flushed resources only.
	 */
	virtual void reload(const TypeInfo& productType, bool flushedOnly) = 0;

	/*! Unload all resources of given type.
	 *
	 * \param productType Type of product.
	 */
	virtual void unload(const TypeInfo& productType) = 0;

	/*! Unload externally unused, resident, resources.
	 *
	 * Call this when unused resources which are resident can
	 * be unloaded.
	 */
	virtual void unloadUnusedResident() = 0;

	/*! Get statistics. */
	virtual void getStatistics(ResourceManagerStatistics& outStatistics) const = 0;

	/*! Bind handle to resource identifier.
	 *
	 * \param id Resource identifier.
	 * \return Resource proxy.
	 */
	template <
		typename ResourceType,
		typename ProductType
	>
	bool bind(const Id< ResourceType >& id, Proxy< ProductType >& outProxy)
	{
		Ref< ResourceHandle > handle = bind(type_of< ProductType >(), id);
		if (!handle)
			return false;

		outProxy = Proxy< ProductType >(handle);
		return bool(handle->get() != nullptr);
	}

	/*! Bind handle to resource identifier.
	 *
	 * \param proxy Resource identifier proxy.
	 */
	template <
		typename ProductType
	>
	bool bind(IdProxy< ProductType >& outProxy)
	{
		Ref< ResourceHandle > handle = bind(type_of< ProductType >(), outProxy.getId());
		if (!handle)
			return false;

		outProxy.replace(handle);
		return bool(handle->get() != nullptr);
	}
};

}
