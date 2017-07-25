#include "Document.h"
#include "XPathException.h"

#include "itextstream.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>

namespace xml
{

// Construct a wrapper around the provided xmlDocPtr.
Document::Document(xmlDocPtr doc):
    _xmlDoc(doc)
{}

Document::Document(const std::string& filename) :
	_xmlDoc(xmlParseFile(filename.c_str()))
{}

Document::Document(const Document& other) :
	_xmlDoc(other._xmlDoc)
{}

Document::~Document() 
{
	if (_xmlDoc != nullptr)
	{
		// Free the xml document memory
		xmlFreeDoc(_xmlDoc);
	}
}

Document Document::create()
{
	xmlChar* versionStr = xmlCharStrdup("1.0");

	// Create a new xmlDocPtr and return the object
	xmlDocPtr doc = xmlNewDoc(versionStr);

	xmlFree(versionStr);

	return Document(doc);
}

Document Document::clone(const Document& source)
{
	if (source._xmlDoc == nullptr)
	{
		// Nothing to clone, create an empty doc
		return Document(nullptr);
	}

	// Create a deep copy of the other doc
	return Document(xmlCopyDoc(source._xmlDoc, 1));
}

void Document::addTopLevelNode(const std::string& name)
{
	std::lock_guard<std::mutex> lock(_lock);

	if (!isValid()) {
		return; // is not Valid, place an assertion here?
	}

	xmlChar* nameStr = xmlCharStrdup(name.c_str());
	xmlChar* emptyStr = xmlCharStrdup("");

	xmlNodePtr root = xmlNewDocNode(_xmlDoc, NULL, nameStr, emptyStr);
	xmlNodePtr oldRoot = xmlDocSetRootElement(_xmlDoc, root);

	if (oldRoot != NULL)
    {
		// Old root element, remove it
		xmlUnlinkNode(oldRoot);
		xmlFreeNode(oldRoot);
	}

	xmlFree(nameStr);
	xmlFree(emptyStr);
}

Node Document::getTopLevelNode() const
{
	if (!isValid())
	{
		// Invalid Document, return a NULL node
		return Node(NULL);
	}

	return Node(_xmlDoc->children);
}

void Document::importDocument(Document& other, Node& importNode)
{
	std::lock_guard<std::mutex> lock(_lock);

	// Locate the top-level node(s) of the other document
	xml::NodeList topLevelNodes = other.findXPath("/*");

	xmlNodePtr targetNode = importNode.getNodePtr();

	if (targetNode->name == NULL)
	{
		// invalid importnode
		return;
	}

	// greebo: Not all target nodes already have a valid child node, use a modified algorithm
	// to handle that situation as suggested by malex984

	// Add each of the imported nodes to the target importNode
	for (std::size_t i = 0; i < topLevelNodes.size(); ++i)
	{
		if (targetNode->children == NULL)
		{
			xmlUnlinkNode(topLevelNodes[i].getNodePtr());
			xmlAddChild(targetNode, topLevelNodes[i].getNodePtr());
		}
		else
		{
			xmlAddPrevSibling(targetNode->children, topLevelNodes[i].getNodePtr());
		}
	}
}

void Document::copyNodes(const NodeList& nodeList)
{
	std::lock_guard<std::mutex> lock(_lock);

	if (!isValid() || _xmlDoc->children == NULL)
	{
		return; // is not Valid, place an assertion here?
	}

	// Copy the child nodes one by one
	for (std::size_t i = 0; i < nodeList.size(); i++)
	{
		// Copy the node
		xmlNodePtr node = xmlCopyNode(nodeList[i].getNodePtr(), 1);
		// Add this node to the top level node of this document
		xmlAddChild(xmlDocGetRootElement(_xmlDoc), node);
	}
}

bool Document::isValid() const
{
	return _xmlDoc != nullptr;
}

// Evaluate an XPath expression and return matching Nodes.
NodeList Document::findXPath(const std::string& path) const
{
	std::lock_guard<std::mutex> lock(_lock);

    // Set up the XPath context
    xmlXPathContextPtr context = xmlXPathNewContext(_xmlDoc);

    if (context == NULL) {
        rConsoleError() << "ERROR: xml::findPath() failed to create XPath context "
                  << "when searching for " << path << std::endl;
        throw XPathException("Failed to create XPath context");
    }

    // Evaluate the expression
    const xmlChar* xpath = reinterpret_cast<const xmlChar*>(path.c_str());
    xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);

    if (result == NULL) {
        rConsoleError() << "ERROR: xml::findPath() failed to evaluate expression "
                  << path << std::endl;
        throw XPathException("Failed to evaluate XPath expression");
    }

    // Construct the return vector. This may be empty if the provided XPath
    // expression does not identify any nodes.
    NodeList retval;
    xmlNodeSetPtr nodeset = result->nodesetval;
	if (nodeset != NULL) {
	    for (int i = 0; i < nodeset->nodeNr; i++) {
	        retval.push_back(Node(nodeset->nodeTab[i]));
	    }
	}

    xmlXPathFreeObject(result);
    return retval;
}

// Saves the file to the disk via xmlSaveFormatFile
void Document::saveToFile(const std::string& filename) const
{
	std::lock_guard<std::mutex> lock(_lock);

	xmlSaveFormatFile(filename.c_str(), _xmlDoc, 1);
}

}
