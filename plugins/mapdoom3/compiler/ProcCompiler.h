#pragma once

#include "inode.h"
#include "ientity.h"
#include "ProcFile.h"
#include "BspTree.h"
#include "math/Vector3.h"

namespace map
{

/** 
 * Processor class that takes a root node (representing a map)
 * to generate a ProcFile containing the model and shadow data.
 */
class ProcCompiler
{
private:
	// The map node we're processing
	scene::INodePtr _root;

	// The working copy
	ProcFilePtr _procFile;

	struct BspFace
	{
		int					planenum;
		bool				portal;			// all portals will be selected before
											// any non-portals
		bool				checked;		// used by selectSplitPlaneNum()
		ProcWinding			w;

		BspFace() :
			planenum(0),
			portal(false),
			checked(false)
		{}
	};
	typedef boost::shared_ptr<BspFace> BspFacePtr;

	typedef std::vector<BspFacePtr> BspFaces;
	BspFaces _bspFaces;

	std::size_t _numActivePortals;
	std::size_t _numPeakPortals;
	std::size_t _numTinyPortals;

	std::size_t _numUniqueBrushes;
	std::size_t _numClusters;

public:
	ProcCompiler(const scene::INodePtr& root);

	// Generate the .proc file
	ProcFilePtr generateProcFile();

private:
	void generateBrushData();

	bool processModels();
	bool processModel(ProcEntity& entity, bool floodFill);

	// Create a list of all faces that are relevant for faceBSP()
	void makeStructuralProcFaceList(const ProcEntity::Primitives& primitives);

	// Sort all structural faces into a bsp tree
	void faceBsp(ProcEntity& entity);

	// Split the given face list, and assign them to node->children[], then enter recursion
	// The given face list will be emptied before returning
	void buildFaceTreeRecursively(const BspTreeNodePtr& node, BspFaces& faces, BspTree& tree);

	std::size_t selectSplitPlaneNum(const BspTreeNodePtr& node, BspFaces& list);

	void makeTreePortals(BspTree& tree);

	void makeHeadNodePortals(BspTree& tree);
	void makeTreePortalsRecursively(const BspTreeNodePtr& node);

	// create the new portal by taking the full plane winding for the cutting plane
	// and clipping it by all of parents of this node
	void makeNodePortal(const BspTreeNodePtr& node);
	void splitNodePortals(const BspTreeNodePtr& node);

	// Mark the leafs as opaque and areaportals and put brush
	// fragments in each leaf so portal surfaces can be matched to materials
	void filterBrushesIntoTree(ProcEntity& entity);

	// Generates two new brushes, leaving the original unchanged
	void splitBrush(const ProcBrushPtr& brush, std::size_t planenum, ProcBrushPtr& front, ProcBrushPtr& back);
	std::size_t filterBrushIntoTreeRecursively(const ProcBrushPtr& brush, const BspTreeNodePtr& node);

	ProcWinding getBaseWindingForNode(const BspTreeNodePtr& node);

	void calculateNodeBounds(const BspTreeNodePtr& node);
	void addPortalToNodes(const ProcPortalPtr& portal, const BspTreeNodePtr& front, const BspTreeNodePtr& back);
	void removePortalFromNode(const ProcPortalPtr& portal, const BspTreeNodePtr& node);
};

} // namespace
