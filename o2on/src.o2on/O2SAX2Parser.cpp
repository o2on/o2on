/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2SAX2Parser.cpp
 * description	: 
 *
 */

#include "O2SAX2Parser.h"
#include "O2Logger.h"




// ---------------------------------------------------------------------------
//	O2SAX2Parser
// ---------------------------------------------------------------------------
wstring O2SAX2Parser::xml_message;
wstring O2SAX2Parser::xml_message_type;

O2SAX2Parser::
O2SAX2Parser(void)
{
	//XMLPlatformUtils::Initialize();
}

O2SAX2Parser::
~O2SAX2Parser()
{
	//XMLPlatformUtils::Terminate();
}

// ---------------------------------------------------------------------------
//	SAX2Handler
// ---------------------------------------------------------------------------
SAX2Handler::
SAX2Handler(const wchar_t *name, O2Logger *lgr)
	: ModuleName(name)
	, Logger(lgr)
{
}

SAX2Handler::
~SAX2Handler()
{
}

void
SAX2Handler::
warning(const SAXParseException& exc)
{
	if (Logger) {
		Logger->AddLog(
			O2LT_WARNING, ModuleName.c_str(), 0, 0,
			L"XML-Warning: %s", exc.getMessage());
	}
}

void
SAX2Handler::
error(const SAXParseException& exc) 
{
	if (Logger) {
		Logger->AddLog(
			O2LT_ERROR, ModuleName.c_str(), 0, 0,
			L"XML-Error: %s", exc.getMessage());
	}
}

void
SAX2Handler::
fatalError(const SAXParseException& exc)
{
	if (Logger) {
		Logger->AddLog(
			O2LT_ERROR, ModuleName.c_str(), 0, 0,
			L"XML-FATAL: %s", exc.getMessage());
	}
}
