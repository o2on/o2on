/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2SAX2Parser.h
 * description	: 
 *
 */

#pragma once
#include "O2Define.h"
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>

XERCES_CPP_NAMESPACE_USE
class O2Logger;




class O2SAX2Parser
{
protected:
	static wstring xml_message;
	static wstring xml_message_type;
public:
	O2SAX2Parser(void);
	~O2SAX2Parser();
	void SetXMLMessage(const wchar_t *msg, const wchar_t *type)
	{
		xml_message = msg;
		xml_message_type = type;
	}
	void GetXMLMessage(wstring &msg, wstring &type)
	{
		msg = xml_message;
		type = xml_message_type;
		xml_message.erase();
		xml_message_type.erase();
	}
};




class SAX2Handler
	: public DefaultHandler
{
protected:
	wstring ModuleName;
	O2Logger *Logger;
	
public:
	SAX2Handler(const wchar_t *name, O2Logger *lgr);
	~SAX2Handler();

	virtual void warning(const SAXParseException& exc);
	virtual void error(const SAXParseException& exc);
	virtual void fatalError(const SAXParseException& exc);
};
