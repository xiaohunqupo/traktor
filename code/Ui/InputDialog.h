/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/RefArray.h"
#include "Ui/ConfigDialog.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_UI_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor::ui
{

class EditValidator;

/*! User input dialog.
 * \ingroup UI
 */
class T_DLLCLASS InputDialog : public ConfigDialog
{
	T_RTTI_CLASS;

public:
	class IValueEnumerator : public IRefCount
	{
	public:
		virtual bool getValue(size_t index, std::wstring& outKey, std::wstring& outValue) const = 0;
	};

	class StringArrayEnumerator : public RefCountImpl< IValueEnumerator >
	{
	public:
		StringArrayEnumerator(const wchar_t** values)
		:	m_values(values)
		{
		}

		virtual bool getValue(size_t index, std::wstring& outKey, std::wstring& outValue) const override
		{
			if (m_values[index])
			{
				outKey = m_values[index];
				outValue = m_values[index];
				return true;
			}
			else
				return false;
		}

	private:
		const wchar_t** m_values;
	};

	class KeyValueArrayEnumerator : public RefCountImpl< IValueEnumerator >
	{
	public:
		KeyValueArrayEnumerator(const wchar_t** values)
		:	m_values(values)
		{
		}

		virtual bool getValue(size_t index, std::wstring& outKey, std::wstring& outValue) const override
		{
			if (m_values[index * 2])
			{
				outKey = m_values[index * 2];
				outValue = m_values[index * 2 + 1];
				return true;
			}
			else
				return false;
		}

	private:
		const wchar_t** m_values;
	};

	struct Field
	{
		std::wstring title;
		std::wstring value;
		Ref< const EditValidator > validator;
		Ref< const IValueEnumerator > valueEnumerator;
		bool browseFile;
		int32_t selectedIndex;

		Field()
		:	browseFile(false)
		,	selectedIndex(-1)
		{
		}

		Field(
			const std::wstring& title_,
			const std::wstring& value_ = L"",
			const EditValidator* validator_ = 0,
			const IValueEnumerator* valueEnumerator_ = 0,
			bool browseFile_ = false
		)
		:	title(title_)
		,	value(value_)
		,	validator(validator_)
		,	valueEnumerator(valueEnumerator_)
		,	browseFile(browseFile_)
		,	selectedIndex(-1)
		{
		}
	};

	bool create(
		Widget* parent,
		const std::wstring& title,
		const std::wstring& message,
		Field* outFields,
		uint32_t outFieldsCount
	);

	virtual DialogResult showModal() override;

private:
	Field* m_outFields = nullptr;
	RefArray< Widget > m_editFields;
};

}
