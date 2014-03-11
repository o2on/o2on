/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: debug.h
 * description	: 
 *
 */

#if defined(_DEBUG) || defined(DEBUG)
#define ASSERT(x) \
	if (!(x)) { \
		MyTRACE("Assertion failed! in %s (%d)\n", \
			__FILE__, __LINE__); \
		DebugBreak(); \
	}
#define VERIFY(x)	ASSERT(x)
#define SLASH() /
#define TRACE	OutputDebugString
#define TRACEA	OutputDebugStringA
#define TRACEW	OutputDebugStringW
#else
#define ASSERT(x)
#define VERIFY(x)	x
#define SLASH() /
#define TRACE SLASH()SLASH()
#define TRACEA SLASH()SLASH()
#define TRACEW SLASH()SLASH()
#endif
