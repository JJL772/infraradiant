#include "EntityInspector.h"
#include "PropertyEditorFactory.h"
#include "AddPropertyDialog.h"

#include "i18n.h"
#include "ientity.h"
#include "ieclass.h"
#include "iregistry.h"
#include "ieventmanager.h"
#include "iuimanager.h"
#include "igame.h"
#include "igroupdialog.h"
#include "imainframe.h"

#include "modulesystem/StaticModule.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "gtkutil/dialog/MessageBox.h"
#include "gtkutil/StockIconMenuItem.h"
#include "gtkutil/RightAlignedLabel.h"
#include "gtkutil/TreeModel.h"
#include "gtkutil/ScrolledFrame.h"
#include "xmlutil/Document.h"
#include "map/Map.h"
#include "selection/algorithm/Entity.h"
#include "selection/algorithm/General.h"

#include <map>
#include <string>

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/frame.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/splitter.h>
#include <wx/textctrl.h>
#include <wx/bmpbuttn.h>
#include <wx/artprov.h>

#include <gtkmm/stock.h>
#include <gtkmm/separator.h>
#include <gtkmm/treeview.h>
#include <gtkmm/paned.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/frame.h>

#include <boost/bind.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>

namespace ui {

/* CONSTANTS */

namespace {

    const int TREEVIEW_MIN_WIDTH = 220;
    const int TREEVIEW_MIN_HEIGHT = 60;

    const char* const PROPERTY_NODES_XPATH = "/entityInspector//property";

	const std::string RKEY_ROOT = "user/ui/entityInspector/";
	const std::string RKEY_PANE_STATE = RKEY_ROOT + "pane";

	const std::string HELP_ICON_NAME = "helpicon.png";
}

EntityInspector::EntityInspector() :
	_selectedEntity(NULL),
	_mainWidget(NULL),
	_editorFrame(NULL),
	_showInheritedCheckbox(NULL),
	_showHelpColumnCheckbox(NULL),
	_primitiveNumLabel(NULL),
	_keyValueTreeView(NULL),
	_helpColumn(NULL),
	_keyEntry(NULL),
	_valEntry(NULL)
{}

void EntityInspector::construct()
{
	wxFrame* temporaryParent = new wxFrame(NULL, wxID_ANY, "");

	_mainWidget = new wxPanel(temporaryParent, wxID_ANY);
	_mainWidget->SetName("EntityInspector");
	_mainWidget->SetSizer(new wxBoxSizer(wxVERTICAL));

	wxBoxSizer* topHBox = new wxBoxSizer(wxHORIZONTAL);

	_showInheritedCheckbox = new wxCheckBox(_mainWidget, wxID_ANY, _("Show inherited properties"));
	_showInheritedCheckbox->Connect(wxEVT_CHECKBOX, 
		wxCommandEventHandler(EntityInspector::_onToggleShowInherited), NULL, this);
	
	_showHelpColumnCheckbox = new wxCheckBox(_mainWidget, wxID_ANY, _("Show help icons"));
	_showHelpColumnCheckbox->SetValue(false);
	_showHelpColumnCheckbox->Connect(wxEVT_CHECKBOX, 
		wxCommandEventHandler(EntityInspector::_onToggleShowHelpIcons), NULL, this);

	_primitiveNumLabel = new wxStaticText(_mainWidget, wxID_ANY, "");

	topHBox->Add(_showInheritedCheckbox, 0, wxEXPAND);
	topHBox->Add(_showHelpColumnCheckbox, 0, wxEXPAND);
	topHBox->Add(_primitiveNumLabel, 0, wxEXPAND);

	// Pane with treeview and editor panel
	wxSplitterWindow* pane = new wxSplitterWindow(_mainWidget, wxID_ANY, 
		wxDefaultPosition, wxDefaultSize, wxSP_3D);

	pane->SplitHorizontally(createTreeViewPane(), createPropertyEditorPane());
	pane->SetSashPosition(150);

	_panedPosition.connect(pane);

	_mainWidget->GetSizer()->Add(topHBox, 0, wxEXPAND);
	_mainWidget->GetSizer()->Add(pane, 1, wxEXPAND);

#if 0
	// GTK stuff

	_columns.reset(new ListStoreColumns);
	_kvStore = Gtk::ListStore::create(*_columns);

	_keyValueTreeView = Gtk::manage(new Gtk::TreeView(_kvStore));

	_paned = Gtk::manage(new Gtk::VPaned);
	_contextMenu.reset(new gtkutil::PopupMenu(_keyValueTreeView));

	_mainWidget = Gtk::manage(new Gtk::VBox(false, 0));

	// Pack in GUI components

	Gtk::HBox* topHBox = Gtk::manage(new Gtk::HBox(false, 6));

    // Show inherited properties checkbutton
	_showInheritedCheckbox = Gtk::manage(new Gtk::CheckButton(
        _("Show inherited properties")
    ));
	_showInheritedCheckbox->signal_toggled().connect(sigc::mem_fun(*this, &EntityInspector::_onToggleShowInherited));

	topHBox->pack_start(*_showInheritedCheckbox, false, false, 0);

	_showHelpColumnCheckbox = Gtk::manage(new Gtk::CheckButton(_("Show help icons")));
	_showHelpColumnCheckbox->set_active(false);
	_showHelpColumnCheckbox->signal_toggled().connect(sigc::mem_fun(*this, &EntityInspector::_onToggleShowHelpIcons));

	topHBox->pack_start(*_showHelpColumnCheckbox, false, false, 0);

	// Label showing the primitive number
	_primitiveNumLabel = Gtk::manage(new gtkutil::RightAlignedLabel(""));
	_primitiveNumLabel->set_alignment(0.95f, 0.5f);
	
	topHBox->pack_start(*_primitiveNumLabel, true, true, 0);

    // Add checkbutton hbox to main widget
	_mainWidget->pack_start(*topHBox, false, false, 0);

	// Pack everything into the paned container
	_paned->pack1(createTreeViewPane());
	_paned->pack2(createPropertyEditorPane());

	_mainWidget->pack_start(*_paned, true, true, 0);

	_panedPosition.connect(_paned);
#endif

	// Reload the information from the registry
	restoreSettings();

    // Create the context menu
    // wxTODO createContextMenu();

    // Stimulate initial redraw to get the correct status
    updateGUIElements();

    // Register self to the SelectionSystem to get notified upon selection
    // changes.
    GlobalSelectionSystem().addObserver(this);

	// Observe the Undo system for undo/redo operations, to refresh the
	// keyvalues when this happens
	GlobalUndoSystem().addObserver(this);

	// initialise the properties
	loadPropertyMap();
}

void EntityInspector::restoreSettings()
{
	// Find the information stored in the registry
	if (GlobalRegistry().keyExists(RKEY_PANE_STATE)) {
		_panedPosition.loadFromPath(RKEY_PANE_STATE);
	}
	else {
		// No saved information, apply standard value
		_panedPosition.setPosition(400);
	}

	_panedPosition.applyPosition();
}

// Entity::Observer implementation

void EntityInspector::onKeyInsert(const std::string& key,
                                  EntityKeyValue& value)
{
    onKeyChange(key, value.get());
}

void EntityInspector::onKeyChange(const std::string& key,
                                  const std::string& value)
{
	wxDataViewItem keyValueIter;

    // Check if we already have an iter for this key (i.e. this is a
    // modification).
    TreeIterMap::const_iterator i = _keyValueIterMap.find(key);

    if (i != _keyValueIterMap.end())
    {
        keyValueIter = i->second;
    }
    else
    {
        // Append a new row to the list store and add it to the iter map
		keyValueIter = _kvStore->AddItem().getItem();
        _keyValueIterMap.insert(TreeIterMap::value_type(key, keyValueIter));
    }

    // Look up type for this key. First check the property parm map,
    // then the entity class itself. If nothing is found, leave blank.
	// Get the type for this key if it exists, and the options
	PropertyParms parms = getPropertyParmsForKey(key);

	// Check the entityclass (which will return blank if not found)
	IEntityClassConstPtr eclass = _selectedEntity->getEntityClass();
	const EntityClassAttribute& attr = eclass->getAttribute(key);

    if (parms.type.empty())
    {
        parms.type = attr.getType();
    }

    bool hasDescription = !attr.getDescription().empty();

    // Set the values for the row
	wxutil::TreeModel::Row row(keyValueIter, *_kvStore);

	row[_columns.name] = key;
	row[_columns.value] = value;
	row[_columns.colour] = "black";
	// wxTODO row[_columns.icon] = PropertyEditorFactory::getPixbufFor(parms.type);
	row[_columns.isInherited] = false;
	row[_columns.hasHelpText] = hasDescription;
	/*wxTODO row[_columns.helpIcon] = hasDescription
                          ? GlobalUIManager().getLocalPixbuf(HELP_ICON_NAME)
						  : Glib::RefPtr<Gdk::Pixbuf>();*/

	// Check if we should update the key/value entry boxes
	std::string curKey = _keyEntry->GetValue();

	std::string selectedKey = getListSelection(_columns.name);

	// If the key in the entry box matches the key which got changed,
	// update the value accordingly, otherwise leave it alone. This is to fix
	// the entry boxes not being updated when a PropertyEditor is changing the value.
	// Therefore only do this if the selectedKey is matching too.
	if (curKey == key && selectedKey == key)
	{
		_valEntry->SetValue(value);
	}
}

void EntityInspector::onKeyErase(const std::string& key,
                                 EntityKeyValue& value)
{
    // Look up iter in the TreeIter map, and delete it from the list store
    TreeIterMap::iterator i = _keyValueIterMap.find(key);
    if (i != _keyValueIterMap.end())
    {
        // Erase row from tree store
		_kvStore->RemoveItem(i->second);

        // Erase iter from iter map
        _keyValueIterMap.erase(i);
    }
    else
    {
        std::cerr << "EntityInspector: warning: removed key '" << key
                  << "' not found in map." << std::endl;
    }
}

// Create the context menu
void EntityInspector::createContextMenu()
{
	_contextMenu->addItem(
		Gtk::manage(new gtkutil::StockIconMenuItem(Gtk::Stock::ADD, _("Add property..."))),
		boost::bind(&EntityInspector::_onAddKey, this)
	);
	_contextMenu->addItem(
		Gtk::manage(new gtkutil::StockIconMenuItem(Gtk::Stock::DELETE, _("Delete property"))),
		boost::bind(&EntityInspector::_onDeleteKey, this),
		boost::bind(&EntityInspector::_testDeleteKey, this)
	);

	_contextMenu->addItem(Gtk::manage(new Gtk::SeparatorMenuItem), gtkutil::PopupMenu::Callback());

	_contextMenu->addItem(
		Gtk::manage(new gtkutil::StockIconMenuItem(Gtk::Stock::COPY, _("Copy Spawnarg"))),
		boost::bind(&EntityInspector::_onCopyKey, this),
		boost::bind(&EntityInspector::_testCopyKey, this)
	);
	_contextMenu->addItem(
		Gtk::manage(new gtkutil::StockIconMenuItem(Gtk::Stock::CUT, _("Cut Spawnarg"))),
		boost::bind(&EntityInspector::_onCutKey, this),
		boost::bind(&EntityInspector::_testCutKey, this)
	);
	_contextMenu->addItem(
		Gtk::manage(new gtkutil::StockIconMenuItem(Gtk::Stock::PASTE, _("Paste Spawnarg"))),
		boost::bind(&EntityInspector::_onPasteKey, this),
		boost::bind(&EntityInspector::_testPasteKey, this)
	);
}

void EntityInspector::onRadiantShutdown()
{
	// Remove all previously stored pane information
	_panedPosition.saveToPath(RKEY_PANE_STATE);
}

void EntityInspector::postUndo()
{
	// Clear the previous entity (detaches this class as observer)
	changeSelectedEntity(NULL);

	// Now rescan the selection and update the stores
	updateGUIElements();
}

void EntityInspector::postRedo()
{
	// Clear the previous entity (detaches this class as observer)
	changeSelectedEntity(NULL);

	// Now rescan the selection and update the stores
	updateGUIElements();
}

const std::string& EntityInspector::getName() const
{
	static std::string _name(MODULE_ENTITYINSPECTOR);
	return _name;
}

const StringSet& EntityInspector::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty()) {
		_dependencies.insert(MODULE_XMLREGISTRY);
		_dependencies.insert(MODULE_UIMANAGER);
		_dependencies.insert(MODULE_SELECTIONSYSTEM);
		_dependencies.insert(MODULE_UNDOSYSTEM);
		_dependencies.insert(MODULE_GAMEMANAGER);
		_dependencies.insert(MODULE_COMMANDSYSTEM);
		_dependencies.insert(MODULE_EVENTMANAGER);
		_dependencies.insert(MODULE_MAINFRAME);
	}

	return _dependencies;
}

void EntityInspector::initialiseModule(const ApplicationContext& ctx)
{
	construct();

	GlobalRadiant().signal_radiantShutdown().connect(
        sigc::mem_fun(this, &EntityInspector::onRadiantShutdown)
    );

	GlobalCommandSystem().addCommand("ToggleEntityInspector", toggle);
	GlobalEventManager().addCommand("ToggleEntityInspector", "ToggleEntityInspector");
}

void EntityInspector::registerPropertyEditor(const std::string& key, const IPropertyEditorPtr& editor)
{
	PropertyEditorFactory::registerPropertyEditor(key, editor);
}

IPropertyEditorPtr EntityInspector::getRegisteredPropertyEditor(const std::string& key)
{
	return PropertyEditorFactory::getRegisteredPropertyEditor(key);
}

void EntityInspector::unregisterPropertyEditor(const std::string& key)
{
	PropertyEditorFactory::unregisterPropertyEditor(key);
}

// Return the Gtk widget for the EntityInspector dialog.

wxPanel* EntityInspector::getWidget()
{
    return _mainWidget;
}

// Create the dialog pane
wxWindow* EntityInspector::createPropertyEditorPane()
{
	_editorFrame = new wxPanel(_mainWidget, wxID_ANY);
    return _editorFrame;
}

// Create the TreeView pane

wxWindow* EntityInspector::createTreeViewPane()
{
	wxPanel* treeViewPanel = new wxPanel(_mainWidget, wxID_ANY);
	treeViewPanel->SetSizer(new wxBoxSizer(wxVERTICAL));

	_kvStore = new wxutil::TreeModel(_columns);
	_keyValueTreeView = new wxDataViewCtrl(treeViewPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE | wxDV_NO_HEADER);

	// Create the Property column
	_keyValueTreeView->AppendBitmapColumn("", _columns.icon.getColumnIndex());
	_keyValueTreeView->AppendTextColumn(_("Property"), _columns.name.getColumnIndex());

	// Create the value column
	_keyValueTreeView->AppendTextColumn(_("Value"), _columns.value.getColumnIndex());

	// wxTODO

	wxBoxSizer* buttonHbox = new wxBoxSizer(wxHORIZONTAL);

	// Pack in the key and value edit boxes
	_keyEntry = new wxTextCtrl(treeViewPanel, wxID_ANY);
	_valEntry = new wxTextCtrl(treeViewPanel, wxID_ANY);

	wxBitmap icon = wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_MENU);
	wxBitmapButton* setButton = new wxBitmapButton(treeViewPanel, wxID_APPLY, icon);

	buttonHbox->Add(_valEntry, 1, wxEXPAND);
	buttonHbox->Add(setButton, 0, wxEXPAND);

	treeViewPanel->GetSizer()->Add(_keyValueTreeView, 1, wxEXPAND);
	treeViewPanel->GetSizer()->Add(_keyEntry, 0, wxEXPAND);
	treeViewPanel->GetSizer()->Add(buttonHbox, 0, wxEXPAND);

	//setButton->Connect(wxEVT_RET).connect(sigc::mem_fun(*this, &EntityInspector::_onSetProperty));
	//_keyEntry->signal_activate().connect(sigc::mem_fun(*this, &EntityInspector::_onEntryActivate));
	//_valEntry->signal_activate().connect(sigc::mem_fun(*this, &EntityInspector::_onEntryActivate));

#if 0
	Gtk::VBox* vbx = Gtk::manage(new Gtk::VBox(false, 3));

    // Create the Property column
	Gtk::TreeViewColumn* nameCol = Gtk::manage(new Gtk::TreeViewColumn(_("Property")));
	nameCol->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
	nameCol->set_spacing(3);

	Gtk::CellRendererPixbuf* pixRenderer = Gtk::manage(new Gtk::CellRendererPixbuf);
	nameCol->pack_start(*pixRenderer, false);
	nameCol->add_attribute(pixRenderer->property_pixbuf(), _columns->icon);

	Gtk::CellRendererText* textRenderer = Gtk::manage(new Gtk::CellRendererText);
	nameCol->pack_start(*textRenderer, false);
	nameCol->add_attribute(textRenderer->property_text(), _columns->name);
	nameCol->add_attribute(textRenderer->property_foreground(), _columns->colour);
    nameCol->set_sort_column(_columns->name);

	_keyValueTreeView->append_column(*nameCol);

	// Create the value column
	Gtk::TreeViewColumn* valCol = Gtk::manage(new Gtk::TreeViewColumn(_("Value")));
	valCol->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);

	Gtk::CellRendererText* valRenderer = Gtk::manage(new Gtk::CellRendererText);
	valCol->pack_start(*valRenderer, true);
	valCol->add_attribute(valRenderer->property_text(), _columns->value);
	valCol->add_attribute(valRenderer->property_foreground(), _columns->colour);
	valCol->set_sort_column(_columns->value);

    _keyValueTreeView->append_column(*valCol);

	// Help column
	_helpColumn = Gtk::manage(new Gtk::TreeViewColumn(_("?")));
	_helpColumn->set_spacing(3);
	_helpColumn->set_visible(false);

	Glib::RefPtr<Gdk::Pixbuf> helpIcon = GlobalUIManager().getLocalPixbuf(HELP_ICON_NAME);

	if (helpIcon)
	{
		_helpColumn->set_fixed_width(helpIcon->get_width());
	}

	// Add the help icon
	Gtk::CellRendererPixbuf* pixRend = Gtk::manage(new Gtk::CellRendererPixbuf);
	_helpColumn->pack_start(*pixRend, false);
	_helpColumn->add_attribute(pixRend->property_pixbuf(), _columns->helpIcon);

	_keyValueTreeView->append_column(*_helpColumn);

	// Connect the tooltip query signal to our custom routine
	_keyValueTreeView->set_has_tooltip(true);
	_keyValueTreeView->signal_query_tooltip().connect(sigc::mem_fun(*this, &EntityInspector::_onQueryTooltip));

    // Set up the signals
	Glib::RefPtr<Gtk::TreeSelection> selection = _keyValueTreeView->get_selection();
	selection->signal_changed().connect(sigc::mem_fun(*this, &EntityInspector::treeSelectionChanged));

    // Embed the TreeView in a scrolled viewport
	Gtk::ScrolledWindow* scrollWin = Gtk::manage(new gtkutil::ScrolledFrame(*_keyValueTreeView));

	_keyValueTreeView->set_size_request(TREEVIEW_MIN_WIDTH, TREEVIEW_MIN_HEIGHT);

	vbx->pack_start(*scrollWin, true, true, 0);

	// Pack in the key and value edit boxes
	_keyEntry = Gtk::manage(new Gtk::Entry);
	_valEntry = Gtk::manage(new Gtk::Entry);

	Gtk::Button* setButton = Gtk::manage(new Gtk::Button);
	setButton->add(*Gtk::manage(new Gtk::Image(Gtk::Stock::APPLY, Gtk::ICON_SIZE_MENU)));

	Gtk::HBox* setButtonBox = Gtk::manage(new Gtk::HBox(false, 0));
	setButtonBox->pack_start(*_valEntry, true, true, 0);
	setButtonBox->pack_start(*setButton, false, false, 0);

	vbx->pack_start(*_keyEntry, false, false, 0);
	vbx->pack_start(*setButtonBox, false, false, 0);
	vbx->pack_start(*Gtk::manage(new Gtk::HSeparator), false, false, 0);

    // Signals for entry boxes
	setButton->signal_clicked().connect(sigc::mem_fun(*this, &EntityInspector::_onSetProperty));
	_keyEntry->signal_activate().connect(sigc::mem_fun(*this, &EntityInspector::_onEntryActivate));
	_valEntry->signal_activate().connect(sigc::mem_fun(*this, &EntityInspector::_onEntryActivate));
#endif

    return treeViewPanel;
}

// Retrieve the selected string from the given property in the list store

std::string EntityInspector::getListSelection(const wxutil::TreeModel::Column& col)
{
	wxDataViewItem item = _keyValueTreeView->GetSelection();

	if (!item.IsOk()) return "";

	wxutil::TreeModel::Row row(item, *_keyValueTreeView->GetModel());

	return row[col];
}

bool EntityInspector::getListSelectionBool(const wxutil::TreeModel::Column& col)
{
	wxDataViewItem item = _keyValueTreeView->GetSelection();

	if (!item.IsOk()) return false;

	wxutil::TreeModel::Row row(item, *_keyValueTreeView->GetModel());

	return row[col];
}

// Redraw the GUI elements
void EntityInspector::updateGUIElements()
{
    // Update from selection system
    getEntityFromSelectionSystem();

    if (_selectedEntity != NULL)
    {
        _editorFrame->Enable(true);
        _keyValueTreeView->Enable(true);
        _showInheritedCheckbox->Enable(true);
        _showHelpColumnCheckbox->Enable(true);
    }
    else  // no selected entity
    {
        // Remove the displayed PropertyEditor
        if (_currentPropertyEditor)
		{
            _currentPropertyEditor = PropertyEditorPtr();
        }

        // Disable the dialog and clear the TreeView
		_editorFrame->Enable(false);
        _keyValueTreeView->Enable(false);
        _showInheritedCheckbox->Enable(false);
        _showHelpColumnCheckbox->Enable(false);
    }
}

// Selection changed callback
void EntityInspector::selectionChanged(const scene::INodePtr& node, bool isComponent)
{
	updateGUIElements();
}

namespace
{

    // SelectionSystem visitor to set a keyvalue on each entity, checking for
    // func_static-style name=model requirements
    class EntityKeySetter
    : public SelectionSystem::Visitor
    {
        // Key and value to set on all entities
        std::string _key;
        std::string _value;

    public:

        // Construct with key and value to set
        EntityKeySetter(const std::string& k, const std::string& v)
        : _key(k), _value(v)
        { }

        // Required visit function
        void visit(const scene::INodePtr& node) const
        {
            Entity* entity = Node_getEntity(node);
            if (entity)
            {
                // Check if we have a func_static-style entity
                std::string name = entity->getKeyValue("name");
                std::string model = entity->getKeyValue("model");
                bool isFuncType = (!name.empty() && name == model);

                // Set the actual value
                entity->setKeyValue(_key, _value);

                // Check for name key changes of func_statics
                if (isFuncType && _key == "name")
                {
                    // Adapt the model key along with the name
                    entity->setKeyValue("model", _value);
				}
            }
			else if (Node_isPrimitive(node)) {
				// We have a primitve node selected, check its parent
				scene::INodePtr parent(node->getParent());

				if (parent == NULL) return;

				Entity* parentEnt = Node_getEntity(parent);

				if (parentEnt != NULL) {
					// We have child primitive of an entity selected, the change
					// should go right into that parent entity
					parentEnt->setKeyValue(_key, _value);
				}
			}
        }
    };
}

std::string EntityInspector::cleanInputString(const std::string &input)
{
    std::string ret = input;

    boost::algorithm::replace_all( ret, "\n", "" );
    boost::algorithm::replace_all( ret, "\r", "" );
    return ret;
}

// Set entity property from entry boxes
void EntityInspector::setPropertyFromEntries()
{
	// Get the key from the entry box
	std::string key = cleanInputString(_keyEntry->GetValue().ToStdString());
    std::string val = cleanInputString(_valEntry->GetValue().ToStdString());

    // Update the entry boxes
    _keyEntry->SetValue(key);
	_valEntry->SetValue(val);

	// Pass the call to the specialised routine
	applyKeyValueToSelection(key, val);
}

void EntityInspector::applyKeyValueToSelection(const std::string& key, const std::string& val)
{
	// greebo: Instantiate a scoped object to make this operation undoable
	UndoableCommand command("entitySetProperty");

	if (key.empty()) {
		return;
	}

	if (key == "name") {
		// Check the global namespace if this change is ok
		IMapRootNodePtr mapRoot = GlobalMapModule().getRoot();
		if (mapRoot != NULL) {
			INamespacePtr nspace = mapRoot->getNamespace();

			if (nspace != NULL && nspace->nameExists(val))
            {
				// name exists, cancel the change
				gtkutil::Messagebox::ShowError(
					(boost::format(_("The name %s already exists in this map!")) % val).str(),
					GlobalMainFrame().getTopLevelWindow());
				return;
			}
		}
	}

	// Detect classname changes
    if (key == "classname") {
		// Classname changes are handled in a special way
		selection::algorithm::setEntityClassname(val);
	}
	else {
		// Regular key change, use EntityKeySetter to set value on all selected entities
		EntityKeySetter setter(key, val);
		GlobalSelectionSystem().foreachSelected(setter);
	}
}

void EntityInspector::loadPropertyMap()
{
	_propertyTypes.clear();

	xml::NodeList pNodes = GlobalGameManager().currentGame()->getLocalXPath(PROPERTY_NODES_XPATH);

	for (xml::NodeList::const_iterator iter = pNodes.begin();
		 iter != pNodes.end();
		 ++iter)
	{
		PropertyParms parms;
		parms.type = iter->getAttributeValue("type");
		parms.options = iter->getAttributeValue("options");

		_propertyTypes.insert(PropertyParmMap::value_type(
			iter->getAttributeValue("match"), parms)
		);
	}
}

/* Popup menu callbacks (see gtkutil::PopupMenu) */

void EntityInspector::_onAddKey()
{
	// Obtain the entity class to provide to the AddPropertyDialog
	IEntityClassConstPtr ec = _selectedEntity->getEntityClass();

	// Choose a property, and add to entity with a default value
	AddPropertyDialog::PropertyList properties = AddPropertyDialog::chooseProperty(_selectedEntity);

	for (std::size_t i = 0; i < properties.size(); ++i)
	{
		const std::string& key = properties[i];

		// Add all keys, skipping existing ones to not overwrite any values on the entity
		if (_selectedEntity->getKeyValue(key) == "" || _selectedEntity->isInherited(key))
		{
			// Add the keyvalue on the entity (triggering the refresh)
			_selectedEntity->setKeyValue(key, "-");
		}
    }
}

void EntityInspector::_onDeleteKey()
{
	std::string prop = getListSelection(_columns.name);

	if (!prop.empty())
	{
		UndoableCommand cmd("deleteProperty");

		_selectedEntity->setKeyValue(prop, "");
	}
}

bool EntityInspector::_testDeleteKey()
{
	// Make sure the Delete item is only available for explicit
	// (non-inherited) properties
	if (getListSelectionBool(_columns.isInherited) == false)
		return true;
	else
		return false;
}

void EntityInspector::_onCopyKey()
{
	std::string key = getListSelection(_columns.name);
    std::string value = getListSelection(_columns.value);

	if (!key.empty()) {
		_clipBoard.key = key;
		_clipBoard.value = value;
	}
}

bool EntityInspector::_testCopyKey()
{
	return !getListSelection(_columns.name).empty();
}

void EntityInspector::_onCutKey()
{
	std::string key = getListSelection(_columns.name);
    std::string value = getListSelection(_columns.value);

	if (!key.empty() && _selectedEntity != NULL)
	{
		_clipBoard.key = key;
		_clipBoard.value = value;

		UndoableCommand cmd("cutProperty");

		// Clear the key after copying
		_selectedEntity->setKeyValue(key, "");
	}
}

bool EntityInspector::_testCutKey()
{
	// Make sure the Delete item is only available for explicit
	// (non-inherited) properties
	if (getListSelectionBool(_columns.isInherited) == false)
	{
		// return true only if selection is not empty
		return !getListSelection(_columns.name).empty();
	}
	else
	{
		return false;
	}
}

void EntityInspector::_onPasteKey()
{
	if (!_clipBoard.key.empty() && !_clipBoard.value.empty())
	{
		// Pass the call
		applyKeyValueToSelection(_clipBoard.key, _clipBoard.value);
	}
}

bool EntityInspector::_testPasteKey()
{
	if (GlobalSelectionSystem().getSelectionInfo().entityCount == 0)
	{
		// No entities selected
		return false;
	}

	// Return true if the clipboard contains data
	return !_clipBoard.key.empty() && !_clipBoard.value.empty();
}

/* GTK CALLBACKS */

void EntityInspector::_onSetProperty()
{
	setPropertyFromEntries();
}

// ENTER key in entry boxes
void EntityInspector::_onEntryActivate()
{
	// Set property and move back to key entry
	setPropertyFromEntries();
	_keyEntry->SetFocus();
}

void EntityInspector::_onToggleShowInherited(wxCommandEvent& ev)
{
	if (_showInheritedCheckbox->IsChecked())
    {
		addClassProperties();
	}
	else
    {
		removeClassProperties();
	}
}

void EntityInspector::_onToggleShowHelpIcons(wxCommandEvent& ev)
{
	// Set the visibility of the column accordingly
	_helpColumn->set_visible(_showHelpColumnCheckbox->IsChecked());
}

bool EntityInspector::_onQueryTooltip(int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
{
	/*wxTODO
	if (_selectedEntity == NULL) return FALSE; // no single entity selected

	// greebo: Important: convert the widget coordinates to bin coordinates first
	int binX, binY;
	_keyValueTreeView->convert_widget_to_bin_window_coords(x, y, binX, binY);

	int cellx, celly;
	Gtk::TreeViewColumn* column = NULL;
	Gtk::TreeModel::Path path;

	if (_keyValueTreeView->get_path_at_pos(binX, binY, path, column, cellx, celly))
	{
		// Get the iter of the row pointed at
		Gtk::TreeModel::iterator iter = _kvStore->get_iter(path);

		if (iter)
		{
			Gtk::TreeModel::Row row = *iter;

			// Get the key pointed at
			bool hasHelp = row[_columns->hasHelpText];

			if (hasHelp)
			{
				std::string key = Glib::ustring(row[_columns->name]);

				IEntityClassConstPtr eclass = _selectedEntity->getEntityClass();
				assert(eclass != NULL);

				// Find the attribute on the eclass, that's where the descriptions are defined
				const EntityClassAttribute& attr = eclass->getAttribute(key);

				if (!attr.getDescription().empty())
				{
					// Check the description of the focused item
					_keyValueTreeView->set_tooltip_row(tooltip, path);
					tooltip->set_markup(attr.getDescription());

					return true; // show tooltip
				}
			}
		}
	}*/

	return false;
}

// Update the PropertyEditor pane, displaying the PropertyEditor if necessary
// and making sure it refers to the currently-selected Entity.
void EntityInspector::treeSelectionChanged()
{
	// Abort if called without a valid entity selection (may happen during
	// various cleanup operations).
	if (_selectedEntity == NULL)
		return;

    // Get the selected key and value in the tree view
    std::string key = getListSelection(_columns.name);
    std::string value = getListSelection(_columns.value);

    // Get the type for this key if it exists, and the options
	PropertyParms parms = getPropertyParmsForKey(key);

    // If the type was not found, also try looking on the entity class
    if (parms.type.empty())
	{
    	IEntityClassConstPtr eclass = _selectedEntity->getEntityClass();
		parms.type = eclass->getAttribute(key).getType();
    }

	// Remove the existing PropertyEditor widget, if there is one
	// wxTODO _editorFrame->remove();

    // Construct and add a new PropertyEditor
    _currentPropertyEditor = PropertyEditorFactory::create(parms.type,
                                                           _selectedEntity,
                                                           key,
                                                           parms.options);

	// If the creation was successful (because the PropertyEditor type exists),
	// add its widget to the editor pane
    if (_currentPropertyEditor)
	{
		//wxTODO_editorFrame->add(_currentPropertyEditor->getWidget());
		//_editorFrame->show_all();
    }

    // Update key and value entry boxes, but only if there is a key value. If
    // there is no selection we do not clear the boxes, to allow keyval copying
    // between entities.
	if (!key.empty())
	{
		_keyEntry->SetValue(key);
		_valEntry->SetValue(value);
	}
}

EntityInspector::PropertyParms EntityInspector::getPropertyParmsForKey(
    const std::string& key
)
{
	PropertyParms returnValue;

	// First, attempt to find the key in the property map
	for (PropertyParmMap::const_iterator i = _propertyTypes.begin();
		 i != _propertyTypes.end(); ++i)
	{
		if (i->first.empty()) continue; // safety check

		// Try to match the entity key against the regex (i->first)
		boost::regex expr(i->first);
		boost::smatch matches;

		if (!boost::regex_match(key, matches, expr)) continue;

		// We have a match
		returnValue.type = i->second.type;
		returnValue.options = i->second.options;
	}

	return returnValue;
}

void EntityInspector::addClassAttribute(const EntityClassAttribute& a)
{
    // Only add properties with values, we don't want the optional
    // "editor_var xxx" properties here.
    if (!a.getValue().empty())
    {
        bool hasDescription = !a.getDescription().empty();

        wxutil::TreeModel::Row row = _kvStore->AddItem();

        row[_columns.name] = a.getName();
        row[_columns.value] = a.getValue();
        row[_columns.colour] = "#707070";
        //wxTODO row[_columns.icon] = Glib::RefPtr<Gdk::Pixbuf>();
        row[_columns.isInherited] = true;
        row[_columns.hasHelpText] = hasDescription;
        /*wxTODO row[_columns.helpIcon] = hasDescription
                              ? GlobalUIManager().getLocalPixbuf(HELP_ICON_NAME)
                              : Glib::RefPtr<Gdk::Pixbuf>();*/
    }
}

// Append inherited (entityclass) properties
void EntityInspector::addClassProperties()
{
	// Get the entityclass for the current entity
	std::string className = _selectedEntity->getKeyValue("classname");
	IEntityClassPtr eclass = GlobalEntityClassManager().findOrInsert(
        className, true
    );

	// Visit the entity class
	eclass->forEachClassAttribute(
        boost::bind(&EntityInspector::addClassAttribute, this, _1)
    );
}

// Remove the inherited properties
void EntityInspector::removeClassProperties()
{
	_kvStore->RemoveItems([&] (const wxutil::TreeModel::Row& row)->bool
	{
		// If this is an inherited row, remove it
		return row[_columns.isInherited];
	});
}

// Update the selected Entity pointer from the selection system
void EntityInspector::getEntityFromSelectionSystem()
{
	// A single entity must be selected
	if (GlobalSelectionSystem().countSelected() != 1)
    {
        changeSelectedEntity(NULL);
		_primitiveNumLabel->SetLabelText("");
		return;
	}

	scene::INodePtr selectedNode = GlobalSelectionSystem().ultimateSelected();

    // The root node must not be selected (this can happen if Invert Selection is
    // activated with an empty scene, or by direct selection in the entity list).
	if (selectedNode->isRoot())
    {
        changeSelectedEntity(NULL);
		_primitiveNumLabel->SetLabelText("");
		return;
	}

    // Try both the selected node (if an entity is selected) or the parent node
    // (if a brush is selected).
    Entity* newSelectedEntity = Node_getEntity(selectedNode);

    if (newSelectedEntity)
    {
        // Node was an entity, use this
        changeSelectedEntity(newSelectedEntity);

		// Just set the entity number
		std::size_t ent(0), prim(0);
		selection::algorithm::getSelectionIndex(ent, prim);

		_primitiveNumLabel->SetLabelText(
			(boost::format(_("Entity %d")) % ent).str());
    }
    else
    {
        // Node was not an entity, try parent instead
        scene::INodePtr selectedNodeParent = selectedNode->getParent();
        changeSelectedEntity(Node_getEntity(selectedNodeParent));
		
		std::size_t ent(0), prim(0);
		selection::algorithm::getSelectionIndex(ent, prim);

		_primitiveNumLabel->SetLabelText(
			(boost::format(_("Entity %d, Primitive %d")) % ent % prim).str());
    }
}

// Change selected entity pointer
void EntityInspector::changeSelectedEntity(Entity* newEntity)
{
    if (newEntity == _selectedEntity)
    {
        // No change, do nothing
        return;
    }
    else
    {
        // Detach only if we already have a selected entity
        if (_selectedEntity)
        {
            _selectedEntity->detachObserver(this);
            removeClassProperties();
        }

        _selectedEntity = newEntity;

        // Attach to new entity if it is non-NULL
        if (_selectedEntity)
        {
            _selectedEntity->attachObserver(this);

            // Add inherited properties if the checkbox is set
            if (_showInheritedCheckbox->IsChecked())
            {
                addClassProperties();
            }
        }
    }
}

void EntityInspector::toggle(const cmd::ArgumentList& args)
{
	GlobalGroupDialog().togglePage("entity");
}

// Define the static EntityInspector module
module::StaticModule<EntityInspector> entityInspectorModule;

} // namespace ui
