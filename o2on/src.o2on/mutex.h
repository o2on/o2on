/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: mutex.h
 * description	: 
 *
 */

#pragma once
#include <windows.h>




/*
class Mutex
{
protected:
	CRITICAL_SECTION critical_section;

public:
	Mutex(void)
	{
		InitializeCriticalSection(&critical_section);
	}
	~Mutex(void)
	{
		Lock();
		Unlock();
		DeleteCriticalSection(&critical_section);
	}
	void Lock(void)
	{
		EnterCriticalSection(&critical_section);
	}

	void Unlock(void)
	{
		LeaveCriticalSection(&critical_section);
	}
};
*/

#include "stopwatch.h"
class Mutex
{
protected:
	HANDLE mutex_handle;

public:
	Mutex(void)
		: mutex_handle(CreateMutex(NULL, FALSE, NULL))
	{
	}
	~Mutex(void)
	{
		Lock();
		Unlock();
		CloseHandle(mutex_handle);
	}
	void Lock(void)
	{
		//stopwatch sw("LOCK");
		WaitForSingleObject(mutex_handle, INFINITE);
		//sw.end();
		//if (sw.d >= 50)
		//	Sleep(0);
	}
	bool LockTest(void)
	{
		DWORD r = WaitForSingleObject(mutex_handle, 0);
		if (r == WAIT_TIMEOUT)
			return false;
		return true;
	}

	void Unlock(void)
	{
		ReleaseMutex(mutex_handle);
	}

private:
	Mutex(const Mutex& rhs);
	Mutex& operator=(const Mutex& rhs);
};
