#include <windows.h>
#include <mmsystem.h>
#include <string>

#pragma once
#pragma comment(lib, "winmm.lib")


class stopwatch
{
public:
	std::string title;
	DWORD ln;
	DWORD start;
	DWORD d;

	stopwatch(const char *t, DWORD l = 1)
		: title(t)
		, ln(l)
		, start(0)
		, d(0)
	{
		timeBeginPeriod(1);
		start = timeGetTime();
		OutputDebugStringA(title.c_str());
		OutputDebugStringA(" ==>\n");
	}

	~stopwatch()
	{
		end();
	}

	void end(void)
	{
		if (start) {
			d = timeGetTime() - start;
			if (d >= ln) {
				OutputDebugStringA(title.c_str());
				char tmp[32];
				sprintf_s(tmp, 32, ":%d\n", d);
				OutputDebugStringA(tmp);
			}
		}
		timeEndPeriod(1);
	}
};
