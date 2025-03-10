/*
 * TRAKTOR
 * Copyright (c) 2022-2024 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "Core/Config.h"

namespace traktor::ui
{

class IWidget;
class ISystemBitmap;

/*! NotificationIcon interface.
 * \ingroup UI
 */
class INotificationIcon
{
public:
	virtual bool create(const std::wstring& text, ISystemBitmap* image) = 0;

	virtual void destroy() = 0;

	virtual void setImage(ISystemBitmap* image) = 0;
};

}
