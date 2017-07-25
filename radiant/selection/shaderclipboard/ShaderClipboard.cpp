#include "ShaderClipboard.h"

#include "i18n.h"
#include "iselectiontest.h"
#include "iscenegraph.h"
#include "iuimanager.h"
#include "imediabrowser.h"
#include "ui/texturebrowser/TextureBrowser.h"
#include "ClosestTexturableFinder.h"

#include "patch/PatchNode.h"
#include "brush/BrushNode.h"
#include <boost/format.hpp>

namespace selection 
{

ShaderClipboard::ShaderClipboard() :
	_updatesDisabled(false)
{
	GlobalUIManager().getStatusBarManager().addTextElement(
		"ShaderClipBoard",
		"icon_texture.png",
		IStatusBarManager::POS_SHADERCLIPBOARD,
		_("The name of the shader in the clipboard")
	);

	GlobalUndoSystem().signal_postUndo().connect(
		sigc::mem_fun(this, &ShaderClipboard::onUndoRedoOperation));
	GlobalUndoSystem().signal_postRedo().connect(
		sigc::mem_fun(this, &ShaderClipboard::onUndoRedoOperation));

	GlobalMapModule().signal_mapEvent().connect(
		sigc::mem_fun(*this, &ShaderClipboard::onMapEvent));
}

void ShaderClipboard::clear() 
{
	_source.clear();

	_updatesDisabled = true;

	// Update the status bar information
	updateStatusText();

	_updatesDisabled = false;
}

void ShaderClipboard::onUndoRedoOperation()
{
	// Check if the source is still valid
	if (!_source.checkValid())
	{
		clear();
	}
}

Texturable ShaderClipboard::getTexturable(SelectionTest& test) {
	// Initialise an empty Texturable structure
	Texturable returnValue;

	algorithm::ClosestTexturableFinder finder(test, returnValue);
	GlobalSceneGraph().root()->traverseChildren(finder);

	return returnValue;
}

void ShaderClipboard::updateMediaBrowsers()
{
	// Avoid nasty loopbacks
	_updatesDisabled = true;

	// Set the active shader in the Texture window as well
	GlobalTextureBrowser().setSelectedShader(_source.getShader());

	std::string sourceShader = _source.getShader();
	GlobalMediaBrowser().setSelection(sourceShader);

	_updatesDisabled = false;

	updateStatusText();
}

void ShaderClipboard::updateStatusText()
{
	std::string statusText;

	if (!_source.empty()) {
		statusText = (boost::format(_("ShaderClipboard: %s")) % _source.getShader()).str();

		if (_source.isFace()) {
			statusText += std::string(" (") + _("Face") + ")";
		}
		else if (_source.isPatch()) {
			statusText += std::string(" (") + _("Patch") + ")";
		}
		else if (_source.isShader()) {
			statusText += std::string(" (") + _("Shader") + ")";
		}
	}
	else {
		statusText = _("ShaderClipboard is empty.");
	}

	GlobalUIManager().getStatusBarManager().setText("ShaderClipBoard", statusText);
}

void ShaderClipboard::setSource(SelectionTest& test) 
{
	if (_updatesDisabled) return; // loopback guard

	_source = getTexturable(test);

	updateMediaBrowsers();
    _signalSourceChanged.emit();
}

void ShaderClipboard::setSource(std::string shader)
{
	if (_updatesDisabled) return; // loopback guard

	_source.clear();
	_source.shader = shader;

	// Don't update the media browser without loopback guards
	// if this is desired, one will have to implement them
	updateStatusText();

    _signalSourceChanged.emit();
}

void ShaderClipboard::setSource(Patch& sourcePatch)
{
	if (_updatesDisabled) return; // loopback guard

	_source.clear();
	_source.patch = &sourcePatch;
	_source.node = sourcePatch.getPatchNode().shared_from_this();

	updateMediaBrowsers();
    _signalSourceChanged.emit();
}

void ShaderClipboard::setSource(Face& sourceFace) 
{
	if (_updatesDisabled) return; // loopback guard

	_source.clear();
	_source.face = &sourceFace;
	_source.node = sourceFace.getBrush().getBrushNode().shared_from_this();

	updateMediaBrowsers();

    _signalSourceChanged.emit();
}

Texturable& ShaderClipboard::getSource() {
	return _source;
}

sigc::signal<void> ShaderClipboard::signal_sourceChanged() const
{
    return _signalSourceChanged;
}

void ShaderClipboard::onMapEvent(IMap::MapEvent ev)
{
	if (ev == IMap::MapUnloading || ev == IMap::MapLoaded)
	{
		// Clear the shaderclipboard, the references are most probably invalid now
		clear();
	}
}

} // namespace selection

// global accessor function
selection::ShaderClipboard& GlobalShaderClipboard()
{
	static selection::ShaderClipboard _instance;

	return _instance;
}
