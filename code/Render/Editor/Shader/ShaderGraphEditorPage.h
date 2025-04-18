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
#include "Editor/IEditorPage.h"
#include "Ui/Events/AllEvents.h"
#include "Ui/Point.h"

#include <map>

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RENDER_EDITOR_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor
{
namespace editor
{

class IDocument;
class IEditor;
class IEditorPageSite;
class PropertiesView;

}

namespace ui
{

class Container;
class EdgeConnectEvent;
class EdgeDisconnectEvent;
class GraphControl;
class Group;
class GroupMovedEvent;
class GridItemContentChangeEvent;
class GridRowDoubleClickEvent;
class GridView;
class Menu;
class Node;
class NodeActivateEvent;
class NodeMovedEvent;
class SelectEvent;
class Splitter;
class SyntaxRichEdit;
class ToolBar;
class ToolBarButtonClickEvent;
class ToolBarDropDown;

}

namespace render
{

class Edge;
class External;
class Group;
class INodeFacade;
class Node;
class QuickMenuTool;
class Script;
class ShaderDependencyPane;
class ShaderGraph;
class ShaderViewer;

class T_DLLEXPORT ShaderGraphEditorPage : public editor::IEditorPage
{
	T_RTTI_CLASS;

public:
	explicit ShaderGraphEditorPage(editor::IEditor* editor, editor::IEditorPageSite* site, editor::IDocument* document);

	virtual bool create(ui::Container* parent) override final;

	virtual void destroy() override final;

	virtual bool dropInstance(db::Instance* instance, const ui::Point& position) override final;

	virtual bool handleCommand(const ui::Command& command) override final;

	virtual void handleDatabaseEvent(db::Database* database, const Guid& eventId) override final;

	void editScript(Script* script);

	void createEditorGraph();

private:
	editor::IEditor* m_editor;
	editor::IEditorPageSite* m_site;
	editor::IDocument* m_document;
	Ref< ShaderGraph > m_shaderGraph;
	Ref< ui::Container > m_container;
	Ref< ui::ToolBar > m_toolBar;
	Ref< ui::ToolBarDropDown > m_toolPlatform;
	Ref< ui::ToolBarDropDown > m_toolRenderer;
	Ref< ui::ToolBarDropDown > m_toolTechniques;
	Ref< ui::GraphControl > m_editorGraph;
	Ref< ui::Splitter > m_scriptSplitter;
	Ref< ui::SyntaxRichEdit > m_scriptEditor;
	Ref< editor::PropertiesView > m_propertiesView;
	Ref< ShaderDependencyPane > m_dependencyPane;
	Ref< ShaderViewer > m_shaderViewer;
	Ref< ui::Container > m_dataContainer;
	Ref< ui::GridView > m_variablesGrid;
	Ref< ui::GridView > m_uniformsGrid;
	Ref< ui::GridView > m_portsGrid;
	Ref< ui::GridView > m_nodeCountGrid;
	Ref< ui::Menu > m_menuPopup;
	Ref< QuickMenuTool > m_menuQuick;
	std::map< const TypeInfo*, Ref< INodeFacade > > m_nodeFacades;
	Ref< Script > m_script;

	void createEditorNodes(const RefArray< Node >& shaderNodes, const RefArray< Edge >& shaderEdges, RefArray< ui::Node >* outEditorNodes);

	Ref< ui::Node > createEditorNode(Node* shaderNode);

	Ref< ui::Group > createEditorGroup(Group* shaderGroup);

	void createNode(const TypeInfo* nodeType, const ui::Point& at);

	void refreshGraph();

	void updateGraph();

	void updateExternalNode(External* external);

	void updateVariableHints();

	void eventToolClick(ui::ToolBarButtonClickEvent* event);

	void eventButtonDown(ui::MouseButtonDownEvent* event);

	void eventDoubleClick(ui::MouseDoubleClickEvent* event);

	void eventSelect(ui::SelectEvent* event);

	void eventGroupMoved(ui::GroupMovedEvent* event);

	void eventNodeMoved(ui::NodeMovedEvent* event);

	void eventNodeDoubleClick(ui::NodeActivateEvent* event);

	void eventEdgeConnect(ui::EdgeConnectEvent* event);

	void eventEdgeDisconnect(ui::EdgeDisconnectEvent* event);

	void eventScriptChange(ui::ContentChangeEvent* event);

	void eventPropertiesChanging(ui::ContentChangingEvent* event);

	void eventPropertiesChanged(ui::ContentChangeEvent* event);

	void eventVariableEdit(ui::GridItemContentChangeEvent* event);

	void eventVariableDoubleClick(ui::GridRowDoubleClickEvent* event);

	void eventUniformOrPortDoubleClick(ui::GridRowDoubleClickEvent* event);
};

}
}
