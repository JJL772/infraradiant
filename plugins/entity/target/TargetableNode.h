#pragma once

#include "selectionlib.h"
#include "scene/Node.h"
#include "entitylib.h"

#include "../Doom3Entity.h"

#include "TargetKeyCollection.h"
#include "RenderableTargetLines.h"

namespace entity
{

class EntityNode;

class TargetLineNode;
typedef std::shared_ptr<TargetLineNode> TargetLineNodePtr;

/**
 * greebo: Each targetable entity (D3Group, Speaker, Lights, etc.) derives from
 *         this class.
 *
 * This registers itself with the contained Doom3Entity and observes its keys.
 * As soon as "name" keys are encountered, the TargetManager is notified about
 * the change, so that the name can be associated with a Target object.
 */
class TargetableNode :
	public Entity::Observer,
	public KeyObserver
{
	Doom3Entity& _d3entity;
	TargetKeyCollection _targetKeys;
	mutable RenderableTargetLines _renderableLines; // TODO: remove this

	// The current name of this entity (used for comparison in "onKeyValueChanged")
	std::string _targetName;

	// The node we're associated with
	EntityNode& _node;

	const ShaderPtr& _wireShader;

    // The targetmanager of the map we're in (is nullptr if not in the scene)
    ITargetManager* _targetManager;

    // The actual scene representation rendering the lines
    TargetLineNodePtr _targetLineNode;

public:
	TargetableNode(Doom3Entity& entity, EntityNode& node, const ShaderPtr& wireShader);

    // This might return nullptr if the node is not inserted in a scene
    ITargetManager* getTargetManager();

	// Connect this class with the Doom3Entity
	void construct();
	// Disconnect this class from the entity
	void destruct();

    const TargetKeyCollection& getTargetKeys() const;

	// Gets called as soon as the "name" keyvalue changes
	void onKeyValueChanged(const std::string& name);

	// Entity::Observer implementation, gets called on key insert
	void onKeyInsert(const std::string& key, EntityKeyValue& value);

	// Entity::Observer implementation, gets called on key erase
	void onKeyErase(const std::string& key, EntityKeyValue& value);

    // scene insert/remove handling
    void onInsertIntoScene(scene::IMapRootNode& root);
    void onRemoveFromScene(scene::IMapRootNode& root);

	void render(RenderableCollector& collector, const VolumeTest& volume) const;

    // Invoked by the TargetKeyCollection when the number of observed has changed
    void onTargetKeyCollectionChanged();

private:
	// Helper method to retrieve the current position
	const Vector3& getWorldPosition() const;
};

} // namespace entity
