/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Io/FileSystem.h"
#include "Database/Local/ActionSetName.h"
#include "Database/Local/Context.h"
#include "Database/Local/IFileStore.h"
#include "Database/Local/LocalInstanceMeta.h"
#include "Database/Local/PhysicalAccess.h"

namespace traktor::db
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.db.ActionSetName", ActionSetName, Action)

ActionSetName::ActionSetName(const Path& instancePath, const std::wstring& newName)
:	m_instancePath(instancePath)
,	m_instancePathNew(instancePath.getPathOnly() + L"/" + newName)
,	m_removedMeta(false)
,	m_removedObject(false)
{
}

bool ActionSetName::execute(Context& context)
{
	IFileStore* fileStore = context.getFileStore();

	const Path oldInstanceMetaPath = getInstanceMetaPath(m_instancePath);
	const Path oldInstanceObjectPath = getInstanceObjectPath(m_instancePath);

	const Path newInstanceMetaPath = getInstanceMetaPath(m_instancePathNew);
	const Path newInstanceObjectPath = getInstanceObjectPath(m_instancePathNew);

	Ref< LocalInstanceMeta > instanceMeta = readPhysicalObject< LocalInstanceMeta >(oldInstanceMetaPath);
	if (!instanceMeta)
		return false;

	if (!FileSystem::getInstance().copy(newInstanceMetaPath, oldInstanceMetaPath))
		return false;

	if (!FileSystem::getInstance().copy(newInstanceObjectPath, oldInstanceObjectPath))
		return false;

	if (!fileStore->add(newInstanceMetaPath) || !fileStore->add(newInstanceObjectPath))
		return false;

	m_removedMeta = fileStore->remove(oldInstanceMetaPath);
	m_removedObject = fileStore->remove(oldInstanceObjectPath);

	for (const auto& blob : instanceMeta->getBlobs())
	{
		const Path oldInstanceDataPath = getInstanceDataPath(m_instancePath, blob);
		const Path newInstanceDataPath = getInstanceDataPath(m_instancePathNew, blob);

		if (!FileSystem::getInstance().copy(newInstanceDataPath, oldInstanceDataPath))
			return false;

		if (!fileStore->add(newInstanceDataPath))
			return false;

		m_removedData[blob] = fileStore->remove(oldInstanceDataPath);
	}

	return true;
}

bool ActionSetName::undo(Context& context)
{
	IFileStore* fileStore = context.getFileStore();

	const Path oldInstanceMetaPath = getInstanceMetaPath(m_instancePath);
	if (m_removedMeta)
		fileStore->rollback(oldInstanceMetaPath);

	const Path oldInstanceObjectPath = getInstanceObjectPath(m_instancePath);
	if (m_removedObject)
		fileStore->rollback(oldInstanceObjectPath);

	m_removedMeta = false;
	m_removedObject = false;

	for (std::map< std::wstring, bool >::const_iterator i = m_removedData.begin(); i != m_removedData.end(); ++i)
	{
		const Path oldInstanceDataPath = getInstanceDataPath(m_instancePath, i->first);
		fileStore->rollback(oldInstanceDataPath);
	}

	m_removedData.clear();

	return true;
}

void ActionSetName::clean(Context& context)
{
	IFileStore* fileStore = context.getFileStore();

	const Path oldInstanceMetaPath = getInstanceMetaPath(m_instancePath);
	const Path oldInstanceObjectPath = getInstanceObjectPath(m_instancePath);

	if (m_removedMeta)
		fileStore->clean(oldInstanceMetaPath);

	if (m_removedObject)
		fileStore->clean(oldInstanceObjectPath);

	for (std::map< std::wstring, bool >::const_iterator i = m_removedData.begin(); i != m_removedData.end(); ++i)
	{
		const Path oldInstanceDataPath = getInstanceDataPath(m_instancePath, i->first);
		fileStore->clean(oldInstanceDataPath);
	}
}

bool ActionSetName::redundant(const Action* action) const
{
	if (const ActionSetName* actionSetName = dynamic_type_cast< const ActionSetName* >(action))
		return m_instancePath == actionSetName->m_instancePath;
	else
		return false;
}

}
