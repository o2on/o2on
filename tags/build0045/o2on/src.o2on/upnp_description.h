/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: upnp_devicedescription.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include "httpheader.h"
#include "dataconv.h"
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>

XERCES_CPP_NAMESPACE_USE




// ---------------------------------------------------------------------------
//	UPnPArgument
// ---------------------------------------------------------------------------
struct UPnPArgument
{
	string name;
	string direction;
	string relatedStateVariable;
	string value;
};
typedef std::vector<UPnPArgument> UPnPArgumentList;
typedef UPnPArgumentList::iterator UPnPArgumentListIt;

// ---------------------------------------------------------------------------
//	UPnPAction
// ---------------------------------------------------------------------------
struct UPnPAction
{
	string name;
	UPnPArgumentList argumentList;

	UPnPArgument *getArgument(const char *name)
	{
		UPnPArgumentListIt it;
		for (it = argumentList.begin(); it != argumentList.end(); it++) {
			if (it->name == name)
				return (&(*it));
		}
		return (NULL);
	}
	char *getArgumentValue(const char *name)
	{
		UPnPArgumentListIt it;
		for (it = argumentList.begin(); it != argumentList.end(); it++) {
			if (it->name == name)
				return (&it->value[0]);
		}
		return (NULL);
	}
	bool setArgumentValue(const char *name, const char *value)
	{
		UPnPArgumentListIt it;
		for (it = argumentList.begin(); it != argumentList.end(); it++) {
			if (it->name == name) {
				it->value = value;
				return true;
			}
		}
		return false;
	}
	bool clearArgumentValue(const char *direction = NULL)
	{
		UPnPArgumentListIt it;
		for (it = argumentList.begin(); it != argumentList.end(); it++) {
			if (!direction || it->direction == direction)
				it->value.clear();
		}
		return false;
	}
};
typedef std::vector<UPnPAction> UPnPActionList;
typedef UPnPActionList::iterator UPnPActionListIt;

// ---------------------------------------------------------------------------
//	UPnPService
// ---------------------------------------------------------------------------
struct UPnPObject;
struct UPnPService
{
	string serviceType;
	string serviceId;
	string SCPDURL;
	string controlURL;
	string eventSubURL;
	UPnPActionList actionList;
	UPnPObject *rootObject;
	
	UPnPService(void)
		: rootObject(NULL)
	{
	}
	UPnPAction *getAction(const char *name)
	{
		for (UPnPActionListIt it = actionList.begin(); it != actionList.end(); it++)
			if (it->name == name) return (&(*it));
		return (NULL);
	}
};
typedef std::vector<UPnPService> UPnPServiceList;
typedef UPnPServiceList::iterator UPnPServiceListIt;

// ---------------------------------------------------------------------------
//	UPnPDevice
// ---------------------------------------------------------------------------
struct UPnPDevice
{
	string deviceType;
	string friendlyName;
	string manufacturer;
	string manufacturerURL;
	string modelDescription;
	string modelName;
	string modelNumber;
	string modelURL;
	string serialNumber;
	string UDN;
	string UPC;
	std::vector<UPnPDevice> deviceList;
	UPnPServiceList serviceList;
	UPnPObject *rootObject;

	UPnPDevice(void)
		: rootObject(NULL)
	{
	}
};
typedef std::vector<UPnPDevice> UPnPDeviceList;
typedef UPnPDeviceList::iterator UPnPDeviceListIt;

// ---------------------------------------------------------------------------
//	UPnPObject
// ---------------------------------------------------------------------------
struct UPnPObject
{
	string location;
	UPnPDevice device;

	bool GetServicesById(const char *id, UPnPService &out)
	{
		return (EnumServicesById(&device, id, out));
	}
	bool EnumServicesById(const UPnPDevice *dv, const char *id, UPnPService &out)
	{
		for (size_t i = 0; i < dv->serviceList.size(); i++) {
			if (dv->serviceList[i].serviceId == id) {
				out = dv->serviceList[i];
				return true;
			}
		}
		for (size_t i = 0; i < dv->deviceList.size(); i++) {
			if (EnumServicesById(&dv->deviceList[i], id, out))
				return true;
		}
		return false;
	}

	size_t GetServicesByType(const char *type, UPnPServiceList &out)
	{
		return (EnumServicesByType(&device, type, out));
	}
	size_t EnumServicesByType(const UPnPDevice *dv, const char *type, UPnPServiceList &out)
	{
		for (size_t i = 0; i < dv->serviceList.size(); i++) {
			if (dv->serviceList[i].serviceType == type)
				out.push_back(dv->serviceList[i]);
		}
		for (size_t i = 0; i < dv->deviceList.size(); i++) {
			EnumServicesByType(&dv->deviceList[i], type, out);
		}
		return (out.size());
	}
};
typedef std::vector<UPnPObject> UPnPObjectList;
typedef UPnPObjectList::iterator UPnPObjectListIt;




// ---------------------------------------------------------------------------
//	UPnPDeviceDescriptionParser
// ---------------------------------------------------------------------------
class UPnPDeviceDescriptionParser
	: public DefaultHandler
{
protected:
	void (*MessageHandler)(const char *);
	UPnPObject *object;

	wstring cur_element;
	string base_url;
	UPnPDevice *cur_device;
	UPnPService *cur_service;
	std::list<UPnPDevice*> parents;	

	inline bool matchlname(const wchar_t *a, const wchar_t *b)
	{
		return (_wcsicmp(a, b) == 0 ? true : false);
	}
	inline bool matchelement(const wchar_t *a)
	{
		return (_wcsicmp(cur_element.c_str(), a) == 0 ? true : false);
	}

public:
	UPnPDeviceDescriptionParser(UPnPObject *obj, void (*func)(const char *))
		: object(obj)
		, MessageHandler(func)
		, cur_device(NULL)
		, cur_service(NULL)
	{
		//XMLPlatformUtils::Initialize();
	}

	~UPnPDeviceDescriptionParser()
	{
		//XMLPlatformUtils::Terminate();
	}

	bool Parse(const wchar_t	*charset,
			   const char		*in,
			   size_t			inlen)
	{
		SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
		parser->setContentHandler(this);
		parser->setErrorHandler(this);

		HTTPHeader tmphdr;
		tmphdr.SplitURL(object->location.c_str());
		base_url = tmphdr.base_url;

		cur_device = NULL;
		cur_service = NULL;
		parents.clear();

		MemBufInputSource source((const XMLByte*)in, inlen, "");
		bool ret = false;
		wchar_t tmp[256];
		string msg;

		try {
			parser->parse(source);
			ret = true;
		}
		catch (const OutOfMemoryException &e) {
			if (MessageHandler) {
				swprintf_s(tmp, 256, L"SAX2-Error: Out of Memory: %s", e.getMessage());
				unicode2ascii(tmp, wcslen(tmp), msg);
				MessageHandler(msg.c_str());
			}
		}
		catch (const XMLException &e) {
			if (MessageHandler) {
				swprintf_s(tmp, 256, L"SAX2-Error: Exception: %s", e.getMessage());
				unicode2ascii(tmp, wcslen(tmp), msg);
				MessageHandler(msg.c_str());
			}
		}
		catch (...) {
			if (MessageHandler)
				MessageHandler("SAX2-Error: Unexpected exception during parsing.");
		}

		delete parser;
		return (ret);
	}

public:
	void startElement(const XMLCh* const uri
					, const XMLCh* const localname
					, const XMLCh* const qname
					, const Attributes& attrs)
	{
		cur_element = localname;

		if (matchlname(localname, L"device")) {
			if (cur_device == NULL) {
				cur_device = &object->device;
			}
			else {
				parents.push_back(cur_device);
				cur_device = new UPnPDevice;
			}
			cur_device->rootObject = object;
		}
		else if (matchlname(localname, L"service")) {
			cur_service = new UPnPService;
			cur_service->rootObject = object;
		}
	}

	void endElement(const XMLCh* const uri
				  , const XMLCh* const localname
				  , const XMLCh* const qname)
	{
		if (matchlname(localname, L"device")) {
			if (!parents.empty()) {
				parents.back()->deviceList.push_back(*cur_device);
				delete cur_device;
				cur_device = parents.back();
				parents.pop_back();
			}
		}
		else if (matchlname(localname, L"service")) {
			if (cur_device) {
				cur_device->serviceList.push_back(*cur_service);
				delete cur_service;
				cur_service = NULL;
			}
		}

		cur_element = L"";
	}

	void characters(const XMLCh* const chars, const unsigned int length)
	{
		string str;
		unicode2ascii(chars, length, str);

		if (wcsstr(cur_element.c_str(), L"URLBase")) {
			if (str[str.size()-1] == '/')
				str.erase(str.size()-1);
			base_url = str;
		}
		else if (wcsstr(cur_element.c_str(), L"URL")) {
			if (strncmp(str.c_str(), "http://", 7) != 0)
				str = base_url + (str[0] == '/' ? "" : "/") + str;
		}

		if (cur_device) {
			if (matchelement(L"deviceType"))
				cur_device->deviceType = str;
			else if (matchelement(L"friendlyName"))
				cur_device->friendlyName = str;
			else if (matchelement(L"manufacturer"))
				cur_device->manufacturer = str;
			else if (matchelement(L"manufacturerURL"))
				cur_device->manufacturerURL = str;
			else if (matchelement(L"modelDescription"))
				cur_device->modelDescription = str;
			else if (matchelement(L"modelName"))
				cur_device->modelName = str;
			else if (matchelement(L"modelNumber"))
				cur_device->modelNumber = str;
			else if (matchelement(L"modelURL"))
				cur_device->modelURL = str;
			else if (matchelement(L"serialNumber"))
				cur_device->serialNumber = str;
			else if (matchelement(L"UDN"))
				cur_device->UDN = str;
			else if (matchelement(L"UPC"))
				cur_device->UPC = str;
		}
		if (cur_service) {
			if (matchelement(L"serviceType"))
				cur_service->serviceType = str;
			else if (matchelement(L"serviceId"))
				cur_service->serviceId = str;
			else if (matchelement(L"SCPDURL"))
				cur_service->SCPDURL = str;
			else if (matchelement(L"controlURL"))
				cur_service->controlURL = str;
			else if (matchelement(L"eventSubURL"))
				cur_service->eventSubURL = str;
		}
	}

	void warning(const SAXParseException& e)
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Warning: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}

	void error(const SAXParseException& e) 
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Error: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}

	void fatalError(const SAXParseException& e)
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Fatal: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}
};




// ---------------------------------------------------------------------------
//	UPnPServiceDescriptionParser
// ---------------------------------------------------------------------------
class UPnPServiceDescriptionParser
	: public DefaultHandler
{
protected:
	void (*MessageHandler)(const char *);
	UPnPService *service;

	wstring cur_element;
	UPnPAction *cur_action;
	UPnPArgument *cur_argument;

	inline bool matchlname(const wchar_t *a, const wchar_t *b)
	{
		return (_wcsicmp(a, b) == 0 ? true : false);
	}
	inline bool matchelement(const wchar_t *a)
	{
		return (_wcsicmp(cur_element.c_str(), a) == 0 ? true : false);
	}

public:
	UPnPServiceDescriptionParser(UPnPService *sv, void (*func)(const char *))
		: service(sv)
		, MessageHandler(func)
		, cur_action(NULL)
		, cur_argument(NULL)
	{
		//XMLPlatformUtils::Initialize();
	}

	~UPnPServiceDescriptionParser()
	{
		//XMLPlatformUtils::Terminate();
	}

	bool Parse(const wchar_t	*charset,
			   const char		*in,
			   size_t			inlen)
	{
		SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
		parser->setContentHandler(this);
		parser->setErrorHandler(this);

		cur_action = NULL;
		cur_argument = NULL;

		MemBufInputSource source((const XMLByte*)in, inlen, "");
		bool ret = false;
		wchar_t tmp[256];
		string msg;

		try {
			parser->parse(source);
			ret = true;
		}
		catch (const OutOfMemoryException &e) {
			if (MessageHandler) {
				swprintf_s(tmp, 256, L"SAX2-Error: Out of Memory: %s", e.getMessage());
				unicode2ascii(tmp, wcslen(tmp), msg);
				MessageHandler(msg.c_str());
			}
		}
		catch (const XMLException &e) {
			if (MessageHandler) {
				swprintf_s(tmp, 256, L"SAX2-Error: Exception: %s", e.getMessage());
				unicode2ascii(tmp, wcslen(tmp), msg);
				MessageHandler(msg.c_str());
			}
		}
		catch (...) {
			if (MessageHandler)
				MessageHandler("SAX2-Error: Unexpected exception during parsing.");
		}

		delete parser;
		return (ret);
	}

public:
	void startElement(const XMLCh* const uri
					, const XMLCh* const localname
					, const XMLCh* const qname
					, const Attributes& attrs)
	{
		cur_element = localname;

		if (matchlname(localname, L"action")) {
			cur_action = new UPnPAction;
		}
		else if (matchlname(localname, L"argument")) {
			cur_argument = new UPnPArgument;
		}
	}

	void endElement(const XMLCh* const uri
				  , const XMLCh* const localname
				  , const XMLCh* const qname)
	{
		if (matchlname(localname, L"action")) {
			if (cur_action) {
				service->actionList.push_back(*cur_action);
				delete cur_action;
				cur_action = NULL;
			}
		}
		else if (matchlname(localname, L"argument")) {
			if (cur_argument) {
				if (cur_action)
					cur_action->argumentList.push_back(*cur_argument);
				delete cur_argument;
				cur_argument = NULL;
			}
		}

		cur_element = L"";
	}

	void characters(const XMLCh* const chars, const unsigned int length)
	{
		string str;
		unicode2ascii(chars, length, str);

		if (cur_argument) {
			if (matchelement(L"name"))
				cur_argument->name = str;
			else if (matchelement(L"direction"))
				cur_argument->direction = str;
			else if (matchelement(L"relatedStateVariable"))
				cur_argument->relatedStateVariable = str;
		}
		else if (cur_action) {
			if (matchelement(L"name"))
				cur_action->name = str;
		}
	}

	void warning(const SAXParseException& e)
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Warning: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}

	void error(const SAXParseException& e) 
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Error: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}

	void fatalError(const SAXParseException& e)
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Fatal: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}
};




// ---------------------------------------------------------------------------
//	UPnPSOAPResponseParser
// ---------------------------------------------------------------------------
class UPnPSOAPResponseParser
	: public DefaultHandler
{
protected:
	void (*MessageHandler)(const char *);
	UPnPService *service;

	UPnPAction *cur_action;
	UPnPArgument *cur_argument;

public:
	UPnPSOAPResponseParser(UPnPService *sv, void (*func)(const char *))
		: service(sv)
		, MessageHandler(func)
		, cur_action(NULL)
		, cur_argument(NULL)
	{
		//XMLPlatformUtils::Initialize();
	}

	~UPnPSOAPResponseParser()
	{
		//XMLPlatformUtils::Terminate();
	}

	bool Parse(const wchar_t	*charset,
			   const char		*in,
			   size_t			inlen)
	{
		SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
		parser->setContentHandler(this);
		parser->setErrorHandler(this);

		cur_action = NULL;
		cur_argument = NULL;

		MemBufInputSource source((const XMLByte*)in, inlen, "");
		bool ret = false;
		wchar_t tmp[256];
		string msg;

		try {
			parser->parse(source);
			ret = true;
		}
		catch (const OutOfMemoryException &e) {
			if (MessageHandler) {
				swprintf_s(tmp, 256, L"SAX2-Error: Out of Memory: %s", e.getMessage());
				unicode2ascii(tmp, wcslen(tmp), msg);
				MessageHandler(msg.c_str());
			}
		}
		catch (const XMLException &e) {
			if (MessageHandler) {
				swprintf_s(tmp, 256, L"SAX2-Error: Exception: %s", e.getMessage());
				unicode2ascii(tmp, wcslen(tmp), msg);
				MessageHandler(msg.c_str());
			}
		}
		catch (...) {
			if (MessageHandler)
				MessageHandler("SAX2-Error: Unexpected exception during parsing.");
		}

		delete parser;
		return (ret);
	}

public:
	void startElement(const XMLCh* const uri
					, const XMLCh* const localname
					, const XMLCh* const qname
					, const Attributes& attrs)
	{
		string name;
		unicode2ascii(localname, wcslen(localname), name);

		if (wcsncmp(qname, L"m:", 2) == 0) {
			const char *p;
			if ((p = strstr(name.c_str(), "Response")) != NULL) {
				string actionName(&name[0], p);
				cur_action = service->getAction(actionName.c_str());
				if (cur_action)
					cur_action->clearArgumentValue("out");
			}
		}
		else if (cur_action) {
			cur_argument = cur_action->getArgument(name.c_str());
			if (cur_argument && cur_argument->direction != "out")
				cur_argument = NULL;
		}
		else if(wcsncmp(qname, L"NewExternalIPAddress",20) == 0){
			cur_action = service->getAction("GetExternalIPAddress");
			if(cur_action)
				cur_argument = cur_action->getArgument("NewExternalIPAddress");
			cur_action = NULL;
		}
	}

	void endElement(const XMLCh* const uri
				  , const XMLCh* const localname
				  , const XMLCh* const qname)
	{
		string name;
		unicode2ascii(localname, wcslen(localname), name);

		if (wcsncmp(qname, L"m:", 2) == 0) {
			cur_action = NULL;
		}
		else if (cur_argument) {
			cur_argument = NULL;
		}
	}

	void characters(const XMLCh* const chars, const unsigned int length)
	{
		string str;
		unicode2ascii(chars, length, str);

		if (cur_argument) {
			unicode2ascii(chars, length, cur_argument->value);
		}
	}

	void warning(const SAXParseException& e)
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Warning: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}

	void error(const SAXParseException& e) 
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Error: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}

	void fatalError(const SAXParseException& e)
	{
		if (MessageHandler) {
			wchar_t tmp[256];
			string msg;
			swprintf_s(tmp, 256, L"SAX2-Fatal: %s", e.getMessage());
			unicode2ascii(tmp, wcslen(tmp), msg);
			MessageHandler(msg.c_str());
		}
	}
};
