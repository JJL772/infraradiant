#ifndef UIMANAGER_H_
#define UIMANAGER_H_

#include "iuimanager.h"

#include "MenuManager.h"
#include "ToolbarManager.h"

namespace ui {

class UIManager :
	public IUIManager
{
	// Local helper class taking care of the menu
	MenuManager _menuManager;
	
	ToolbarManager _toolbarManager;

public:

	/** greebo: Retrieves the helper class to manipulate the menu.
	 */
	IMenuManager& getMenuManager();
	
	IToolbarManager& getToolbarManager();

}; // class UIManager

} // namespace ui

#endif /*UIMANAGER_H_*/
