/*
CGUI
by Gustav Larsson
gee@pyro.nu
*/

//#include "stdafx."
#include "GUI.h"
#include <string>
#include <assert.h>
#include <stdarg.h>

///#include "nemesis.h"
//#include incCONSOLE

#include <xercesc/dom/DOM.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include "XercesErrorHandler.h"

// namespaces used
XERCES_CPP_NAMESPACE_USE
using namespace std;

#ifdef _MSC_VER
#pragma comment(lib, "xerces-c_2.lib")
#endif


#include "input.h"

//-------------------------------------------------------------------
//	called from main loop when (input) events are received.
//	event is passed to other handlers if false is returned.
//	trampoline: we don't want to make the implementation (in CGUI) static
//-------------------------------------------------------------------
bool gui_handler(const SDL_Event& ev)
{
	return g_GUI.HandleEvent(ev);

//	return false;
}

bool CGUI::HandleEvent(const SDL_Event& ev)
{
	if(ev.type == SDL_MOUSEMOTION)
		m_MouseX = ev.motion.x, m_MouseY = ev.motion.y;

// JW: (pre|post)process omitted; what're they for? why would we need any special button_released handling?

	// Only one object can be hovered
	//  check which one it is, if any !
	CGUIObject *pNearest = NULL;

	GUI<CGUIObject*>::RecurseObject(GUIRR_HIDDEN, m_BaseObject, &CGUIObject::ChooseMouseOverAndClosest, pNearest);
	
	// Now we'll call UpdateMouseOver on *all* objects,
	//  we'll input the one hovered, and they will each
	//  update their own data and send messages accordingly
	GUI<CGUIObject*>::RecurseObject(GUIRR_HIDDEN, m_BaseObject, &CGUIObject::UpdateMouseOver, pNearest);

	if(pNearest)
	{
		if(ev.type == SDL_MOUSEBUTTONDOWN)
			pNearest->HandleMessage(GUIM_MOUSE_PRESS_LEFT);	// JW: want to pass SDL button value, or translate?
		else if(ev.type == SDL_MOUSEBUTTONUP)
			pNearest->HandleMessage(GUIM_MOUSE_RELEASE_LEFT);	// JW: want to pass SDL button value, or translate?
	}

// JW: what's the difference between mPress and mDown? what's the code below responsible for?
/*/* // Generally if just mouse is clicked
	if (m_pInput->mDown(NEMM_BUTTON1) && pNearest)
	{
		pNearest->HandleMessage(GUIM_MOUSE_DOWN_LEFT);
	}
*/

	return false;
}

//-------------------------------------------------------------------
//  Constructor / Destructor
//-------------------------------------------------------------------
CGUI::CGUI()
{
	m_BaseObject = new CGUIDummyObject;
	m_BaseObject->SetGUI(this);

	// This will make this invisible, not add
	//m_BaseObject->SetName(BASE_OBJECT_NAME);
}

CGUI::~CGUI()
{
	if (m_BaseObject)
		delete m_BaseObject;
}

//-------------------------------------------------------------------
//  Functions
//-------------------------------------------------------------------
CGUIObject *CGUI::ConstructObject(const CStr &str)
{
	if (m_ObjectTypes.count(str) > 0)
		return (*m_ObjectTypes[str])();
	else
		return NULL;
}

void CGUI::Initialize(/*/*CInput *pInput*/)
{
///	m_pInput = pInput;

	// Add base types!
	AddObjectType("button", &CButton::ConstructObject);
}

void CGUI::Process()
{
/*/*

	// GeeTODO / check if m_pInput is valid, otherwise return
///	assert(m_pInput);

	// Pre-process all objects
	try
	{
		GUI<EGUIMessage>::RecurseObject(0, m_BaseObject, &CGUIObject::HandleMessage, GUIM_PREPROCESS);
	}
	catch (PS_RESULT e)
	{
		return;
	}

	// Check mouse over
	try
	{
		// Only one object can be hovered
		//  check which one it is, if any !
		CGUIObject *pNearest = NULL;

		GUI<CGUIObject*>::RecurseObject(GUIRR_HIDDEN, m_BaseObject, &CGUIObject::ChooseMouseOverAndClosest, pNearest);
		
		// Now we'll call UpdateMouseOver on *all* objects,
		//  we'll input the one hovered, and they will each
		//  update their own data and send messages accordingly
		GUI<CGUIObject*>::RecurseObject(GUIRR_HIDDEN, m_BaseObject, &CGUIObject::UpdateMouseOver, pNearest);

		// If pressed
		if (m_pInput->mPress(NEMM_BUTTON1) && pNearest)
		{
			pNearest->HandleMessage(GUIM_MOUSE_PRESS_LEFT);
		}
		else
		// If released
		if (m_pInput->mRelease(NEMM_BUTTON1) && pNearest)
		{
			pNearest->HandleMessage(GUIM_MOUSE_RELEASE_LEFT);
		}

		// Generally if just mouse is clicked
		if (m_pInput->mDown(NEMM_BUTTON1) && pNearest)
		{
			pNearest->HandleMessage(GUIM_MOUSE_DOWN_LEFT);
		}

	}
	catch (PS_RESULT e)
	{
		return;
	}

	// Post-process all objects
	try
	{
		GUI<EGUIMessage>::RecurseObject(0, m_BaseObject, &CGUIObject::HandleMessage, GUIM_POSTPROCESS);
	}
	catch (PS_RESULT e)
	{
		return;
	}
*/
}

void CGUI::Draw()
{
	try
	{
		// Recurse CGUIObject::Draw()
		GUI<>::RecurseObject(GUIRR_HIDDEN, m_BaseObject, &CGUIObject::Draw);
	}
	catch (PS_RESULT e)
	{
		return;
	}
}

void CGUI::Destroy()
{
	// We can use the map to delete all
	//  now we don't want to cancel all if one Destroy fails
	map_pObjects::iterator it;
	for (it = m_pAllObjects.begin(); it != m_pAllObjects.end(); ++it)
	{
		try
		{
			it->second->Destroy();
		}
		catch (PS_RESULT e)
		{

		}
		
		delete it->second;
		it->second = NULL;
	}

	// Clear all
	m_pAllObjects.clear();
	m_Sprites.clear();
}

void CGUI::AddObject(CGUIObject* pObject)
{
	try
	{
		// Add CGUI pointer
		GUI<CGUI*>::RecurseObject(0, pObject, &CGUIObject::SetGUI, this);

		// Add child to base object
		m_BaseObject->AddChild(pObject);
	}
	catch (PS_RESULT e)
	{
		throw e;
	}
}

void CGUI::UpdateObjects()
{
	// We'll fill a temporary map until we know everything
	//  succeeded
	map_pObjects AllObjects;

	try
	{
		// Fill freshly
		GUI< map_pObjects >::RecurseObject(0, m_BaseObject, &CGUIObject::AddToPointersMap, AllObjects );
	}
	catch (PS_RESULT e)
	{
		// Throw the same error
		throw e;
	}

	// Else actually update the real one
	m_pAllObjects = AllObjects;
}

bool CGUI::ObjectExists(const CStr &Name) const
{
	if (m_pAllObjects.count(Name))
		return true;
	else
		return false;
}

void CGUI::ReportParseError(const CStr &str, ...)
{
	// Print header
	if (m_Errors==0)
	{
///		g_nemLog("*** GUI Tree Creation Errors");
	}

	// Important, set ParseError to true
	++m_Errors;
/* MEGA TODO Gee
	char buffer[512];
	va_list args;

	// get arguments
	va_start(args, str);
		vsprintf(buffer, str.c_str(), args);
	va_end(args);
*/	
///	g_nemLog(" %s", buffer);
}

void CGUI::LoadXMLFile(const string &Filename)
{
	// Reset parse error
	//  we can later check if this has increased
	m_Errors = 0;

	// Initialize XML library
	XMLPlatformUtils::Initialize();

	// Create parser instance
	XercesDOMParser *parser = new XercesDOMParser();

 	bool ParseFailed = false;

	if (parser)
	{
		// Setup parser
		parser->setValidationScheme(XercesDOMParser::Val_Auto);
		parser->setDoNamespaces(false);
		parser->setDoSchema(false);

		// Set cosutomized error handler
		CXercesErrorHandler *errorHandler = new CXercesErrorHandler();
		parser->setErrorHandler(errorHandler);
		
		parser->setCreateEntityReferenceNodes(false);

		try 
		{
///			g_nemLog("*** Xerces XML Parsing Errors");

			// Get main node
			LocalFileInputSource source( XMLString::transcode( Filename.c_str() ) );

			// parse
			parser->parse(source);

			// Check how many errors
			ParseFailed = parser->getErrorCount() != 0;
			if (ParseFailed)
			{
				// TODO report for real!
///				g_console.submit("echo Xerces XML Parsing Reports %d errors", parser->getErrorCount());
			}
		} 
		catch (const XMLException& toCatch) 
		{
			char* message = XMLString::transcode(toCatch.getMessage());
///			g_console.submit("echo Exception message is: %s", message);
			XMLString::release(&message);
		}
		catch (const DOMException& toCatch) 
		{
			char* message = XMLString::transcode(toCatch.msg);
///			g_console.submit("echo Exception message is: %s", message);
			XMLString::release(&message);
		}
		catch (...) 
		{
///			g_console.submit("echo Unexpected Exception");
		}
		
		// Parse Failed?
		if (!ParseFailed)
		{
			DOMDocument *doc = parser->getDocument();
			DOMElement *node = doc->getDocumentElement();

			// Check root element's (node) name so we know what kind of
			//  data we'll be expecting
			string root_name = XMLString::transcode( node->getNodeName() );

			if (root_name == "objects")
			{
				Xerces_ReadRootObjects(node);
			}
			else
			if (root_name == "sprites")
			{
				Xerces_ReadRootSprites(node);
			}
		}
	}

	// Now report if any other errors occured
	if (m_Errors > 0)
	{
///		g_console.submit("echo GUI Tree Creation Reports %d errors", m_Errors);
	}
	
	XMLPlatformUtils::Terminate();
}


//===================================================================
//	XML Reading Xerces Specific Sub-Routines
//===================================================================

void CGUI::Xerces_ReadRootObjects(XERCES_CPP_NAMESPACE::DOMElement *pElement)
{
	// Iterate main children
	//  they should all be <object> elements
	DOMNodeList *children = pElement->getChildNodes();

	for (int i=0; i<children->getLength(); ++i)
	{
		DOMNode *child = children->item(i);

		if (child->getNodeType() == DOMNode::ELEMENT_NODE)
		{
			// Read in this whole object into the GUI
			DOMElement *element = (DOMElement*)child;
			Xerces_ReadObject(element, m_BaseObject);
		}
	}
}

void CGUI::Xerces_ReadRootSprites(XERCES_CPP_NAMESPACE::DOMElement *pElement)
{
	// Iterate main children
	//  they should all be <sprite> elements
	DOMNodeList *children = pElement->getChildNodes();

	for (int i=0; i<children->getLength(); ++i)
	{
		DOMNode *child = children->item(i);

		if (child->getNodeType() == DOMNode::ELEMENT_NODE)
		{
			// Read in this whole object into the GUI
			DOMElement *element = (DOMElement*)child;
			Xerces_ReadSprite(element);
		}
	}
}

void CGUI::Xerces_ReadObject(DOMElement *pElement, CGUIObject *pParent)
{
	assert(pParent && pElement);
	int i;

	// Our object we are going to create
	CGUIObject *object = NULL;

	// Well first of all we need to determine the type
	string type = XMLString::transcode( pElement->getAttribute( XMLString::transcode("type") ) );

	// Construct object from specified type
	//  henceforth, we need to do a rollback before aborting.
	//  i.e. releasing this object
	object = ConstructObject(type);

	if (!object)
	{
		// Report error that object was unsuccessfully loaded
		ReportParseError("Unrecognized type: " + type);

		delete object;
		return;
	}

	//
	//	Read Attributes
	//

	bool NameSet = false;

	// Now we can iterate all attributes and store
	DOMNamedNodeMap *attributes = pElement->getAttributes();
	for (i=0; i<attributes->getLength(); ++i)
	{
		DOMAttr *attr = (DOMAttr*)attributes->item(i);
		string attr_name = XMLString::transcode( attr->getName() );
		string attr_value = XMLString::transcode( attr->getValue() );

		// Ignore "type", we've already checked it
		if (attr_name == "type")
			continue;

		// Also the name needs some special attention
		if (attr_name == "name")
		{
			object->SetName(attr_value);
			NameSet = true;
			continue;
		}

		// Try setting the value
		try
		{
			object->SetSetting(attr_name, attr_value);
		}
		catch (PS_RESULT e)
		{
			ReportParseError("Can't set \"" + attr_name + "\" to \"" + attr_value + "\"");

			// This is not a fatal error
		}
	}

	// Check if name isn't set, report error in that case
	if (!NameSet)
	{
		// Set Random name! TODO
	}

	//
	//	Read Children
	//

	// Iterate children
	DOMNodeList *children = pElement->getChildNodes();

	for (i=0; i<children->getLength(); ++i)
	{
		// Get node
		DOMNode *child = children->item(i);

		// Check type (it's probably text or element)
		
		// A child element
		if (child->getNodeType() == DOMNode::ELEMENT_NODE)
		{
			// Check what name the elements got
			string element_name = XMLString::transcode( child->getNodeName() );

			if (element_name == "object")
			{
				// First get element and not node
				DOMElement *element = (DOMElement*)child;

				// TODO REPORT ERROR

				// Call this function on the child
				Xerces_ReadObject(element, object);
			}
		}
		else 
		if (child->getNodeType() == DOMNode::TEXT_NODE)
		{
			CStr caption = XMLString::transcode( child->getNodeValue() );

			// Text is only okay if it's the first element i.e. <object>caption ... </object>
			if (i==0)
			{
				// TODO !!!!! CROP STRING!

				// Set the setting caption to this
				GUI<CStr>::SetSetting(object, "caption", caption);
			}
			// TODO check invalid strings?
		}
	}

	//
	//	Input Child
	//

	try
	{
		if (pParent == m_BaseObject)
			AddObject(object);
		else
			pParent->AddChild(object);
	}
	catch (PS_RESULT e)
	{
		ReportParseError(e);
	}
}

void CGUI::Xerces_ReadSprite(XERCES_CPP_NAMESPACE::DOMElement *pElement)
{
	assert(pElement);

	// Sprite object we're adding
	CGUISprite sprite;
	
	// and what will be its reference name
	CStr name;

	//
	//	Read Attributes
	//

	// Get name, we know it exists because of DTD requirements
	name = XMLString::transcode( pElement->getAttribute( XMLString::transcode("name") )  );

	//
	//	Read Children (the images)
	//

	// Iterate children
	DOMNodeList *children = pElement->getChildNodes();

	for (int i=0; i<children->getLength(); ++i)
	{
		// Get node
		DOMNode *child = children->item(i);

		// Check type (it's probably text or element)
		
		// A child element
		if (child->getNodeType() == DOMNode::ELEMENT_NODE)
		{
			// All Elements will be of type "image" by DTD law

			// First get element and not node
			DOMElement *element = (DOMElement*)child;

			// Call this function on the child
			Xerces_ReadImage(element, sprite);
		}
	}

	//
	//	Add Sprite
	//

	m_Sprites[name] = sprite;
}

void CGUI::Xerces_ReadImage(XERCES_CPP_NAMESPACE::DOMElement *pElement, CGUISprite &parent)
{
	assert(pElement);

	// Image object we're adding
	SGUIImage image;
	
	// TODO - Setup defaults here (or maybe they are in the SGUIImage ctor)

	//
	//	Read Attributes
	//

	// Now we can iterate all attributes and store
/*	DOMNamedNodeMap *attributes = pElement->getAttributes();
	for (int i=0; i<attributes->getLength(); ++i)
	{
		DOMAttr *attr = (DOMAttr*)attributes->item(i);
		string attr_name = XMLString::transcode( attr->getName() );
		string attr_value = XMLString::transcode( attr->getValue() );

		// This is the only attribute we want
		if (attr_name == "texture")
		{
			image.m_Texture = attr_value;
		}
		else
		{
			// Log
			g_console.submit("echo Error attribute " + attr_name + " is not expected in <image>");
			return;
		}
	}
*/
	//
	//	Input
	//

	parent.AddImage(image);	
}
