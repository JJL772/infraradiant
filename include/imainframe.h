#ifndef IMAINFRAME_H_
#define IMAINFRAME_H_

#include "imodule.h"

const std::string MODULE_MAINFRAME("MainFrame");

class IMainFrame :
	public RegisterableModule
{
public:
	// Constructs the toplevel mainframe window and issues the "radiant startup" signal
	virtual void construct() = 0;

	// Destroys the toplevel mainframe window and issues the "radiant shutdown" signal
	virtual void destroy() = 0;

	// Returns TRUE if screen updates are enabled
	virtual bool screenUpdatesEnabled() = 0;

	// Use this to (re-)enable camera and xyview draw updates
	virtual void enableScreenUpdates() = 0;

	// Use this to disable camera and xyview draw updates until enableScreenUpdates is called.
	virtual void disableScreenUpdates() = 0;
}; // class IGridManager

// This is the accessor for the mainframe module
inline IMainFrame& GlobalMainFrame() {
	// Cache the reference locally
	static IMainFrame& _mainFrame(
		*boost::static_pointer_cast<IMainFrame>(
			module::GlobalModuleRegistry().getModule(MODULE_MAINFRAME)
		)
	);
	return _mainFrame;
}

#endif /* IMAINFRAME_H_ */
