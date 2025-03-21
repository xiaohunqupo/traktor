/*
 * TRAKTOR
 * Copyright (c) 2022-2023 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Core/Math/MathUtils.h"
#include "Ui/Application.h"
#include "Ui/Canvas.h"
#include "Ui/StyleSheet.h"
#include "Ui/ToolBar/ToolBar.h"
#include "Ui/ToolBar/ToolBarButton.h"
#include "Ui/ToolBar/ToolBarButtonClickEvent.h"

namespace traktor::ui
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.ui.ToolBarButton", ToolBarButton, ToolBarItem)

ToolBarButton::ToolBarButton(const std::wstring& text, int32_t imageIndex, const Command& command, int32_t style)
:	m_text(text)
,	m_command(command)
,	m_imageIndex(imageIndex)
,	m_style(style)
,	m_state(BstNormal)
{
	if ((m_style & BsToggled) == BsToggled)
		setToggled(true);
}

ToolBarButton::ToolBarButton(const std::wstring& text, const Command& command, int32_t style)
:	m_text(text)
,	m_command(command)
,	m_imageIndex(-1)
,	m_style(style)
,	m_state(BstNormal)
{
	if ((m_style & BsToggled) == BsToggled)
		setToggled(true);
}

void ToolBarButton::setText(const std::wstring& text)
{
	m_text = text;
}

const std::wstring& ToolBarButton::getText() const
{
	return m_text;
}

void ToolBarButton::setImage(int32_t imageIndex)
{
	m_imageIndex = imageIndex;
}

int32_t ToolBarButton::getImage() const
{
	return m_imageIndex;
}

void ToolBarButton::setToggled(bool toggled)
{
	if (toggled)
		m_state |= BstToggled;
	else
		m_state &= ~BstToggled;
}

bool ToolBarButton::isToggled() const
{
	return bool((m_state & BstToggled) == BstToggled);
}

bool ToolBarButton::getToolTip(std::wstring& outToolTip) const
{
	outToolTip = m_text;
	return true;
}

Size ToolBarButton::getSize(const ToolBar* toolBar) const
{
	const Size imageSize = toolBar->getImageSize();

	int32_t width = toolBar->pixel(8_ut);
	int32_t height = width;

	if (m_style & BsIcon)
	{
		width += imageSize.cx;
		height = max(imageSize.cy + toolBar->pixel(8_ut), height);
	}
	if (m_style & BsText)
	{
		Size textExtent = toolBar->getFontMetric().getExtent(m_text);
		width += textExtent.cx;
		height = max(textExtent.cy + toolBar->pixel(8_ut), height);
	}
	if ((m_style & (BsIcon | BsText)) == (BsIcon | BsText))
		width += toolBar->pixel(4_ut);

	return Size(width, height);
}

void ToolBarButton::paint(ToolBar* toolBar, Canvas& canvas, const Point& at, const RefArray< IBitmap >& images)
{
	const StyleSheet* ss = toolBar->getStyleSheet();
	const Size imageSize = toolBar->getImageSize();
	const Size size = getSize(toolBar);
	const bool enabled = isEnable() && toolBar->isEnable(true);

	if (enabled)
	{
		if ((m_state & (BstPushed | BstHover)) != 0)
		{
			if ((m_state & BstPushed) != 0)
				canvas.setBackground(ss->getColor(toolBar, L"item-background-color-pushed"));
			else if ((m_state & BstHover) != 0)
				canvas.setBackground(ss->getColor(toolBar, L"item-background-color-hover"));

			canvas.fillRect(Rect(at, size));
		}
		if ((m_state & BstToggled) != 0)
		{
			canvas.setForeground(ss->getColor(toolBar, L"item-color-toggled"));
			canvas.drawRect(Rect(at, size));
		}
	}

	int32_t centerOffsetX = toolBar->pixel(4_ut);
	if (m_imageIndex >= 0 && m_imageIndex < (int32_t)images.size() && (m_style & BsIcon) != 0)
	{
		const int32_t centerOffsetY = (size.cy - imageSize.cy) / 2;
		canvas.drawBitmap(
			at + Size(centerOffsetX, centerOffsetY),
			Point(0, 0),
			imageSize,
			images[m_imageIndex],
			BlendMode::Alpha
		);
		centerOffsetX += imageSize.cx + toolBar->pixel(4_ut);
	}
	if ((m_style & BsText) != 0)
	{
		const Size textExtent = toolBar->getFontMetric().getExtent(m_text);
		const int32_t centerOffsetY = (size.cy - textExtent.cy) / 2;
		canvas.setForeground(ss->getColor(toolBar, enabled ? L"color" : L"color-disabled"));
		canvas.drawText(
			at + Size(centerOffsetX, centerOffsetY),
			m_text
		);
	}
}

bool ToolBarButton::mouseEnter(ToolBar* toolBar)
{
	if (isEnable())
		m_state |= BstHover;
	return true;
}

void ToolBarButton::mouseLeave(ToolBar* toolBar)
{
	m_state &= ~BstHover;
}

void ToolBarButton::buttonDown(ToolBar* toolBar, MouseButtonDownEvent* mouseEvent)
{
	m_state |= BstPushed;
}

void ToolBarButton::buttonUp(ToolBar* toolBar, MouseButtonUpEvent* mouseEvent)
{
	m_state &= ~BstPushed;
	if (m_style & BsToggle)
	{
		if (m_state & BstToggled)
			m_state &= ~BstToggled;
		else
			m_state |= BstToggled;
	}

	ToolBarButtonClickEvent clickEvent(toolBar, this, m_command);
	toolBar->raiseEvent(&clickEvent);
}

}
