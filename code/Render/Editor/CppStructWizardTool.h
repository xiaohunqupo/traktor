/*
 * TRAKTOR
 * Copyright (c) 2025 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Editor/IWizardTool.h"

namespace traktor::render
{

class CppStructWizardTool : public editor::IWizardTool
{
	T_RTTI_CLASS;

	virtual std::wstring getDescription() const override final;

	virtual const TypeInfoSet getSupportedTypes() const override final;

	virtual uint32_t getFlags() const override final;

	virtual bool launch(ui::Widget* parent, editor::IEditor* editor, db::Group* group, db::Instance* instance) override final;
};

}
