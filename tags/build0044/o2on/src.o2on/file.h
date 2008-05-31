/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: file.h
 * description	: 
 *
 */

#pragma once
#include <windows.h>
#include "dataconv.h"

#define MODE_R	0x00000001
#define MODE_W	0x00000002
#define MODE_A	0x00000003



// ---------------------------------------------------------------------------
//
//	class File
//
//	移植時の注意：
//	  Read時  : バイト単位の共有ロックをかける (Read=OK   / Write=Wait)
//	  Write時 : バイト単位の排他ロックをかける (Read=Wait / Write=Wait)
//
//	  ※Wait無しですぐエラーを返すような実装はダメ
//
// ---------------------------------------------------------------------------
class File
{
protected:
	HANDLE			hFile;

public:
	File(void)
		: hFile(INVALID_HANDLE_VALUE)
	{
	}

	~File()
	{
		close();
	}

	bool open(const char *filename, uint mode)
	{
		return (open_(filename, NULL, mode));
	}

	bool open(const wchar_t *filename, uint mode)
	{
		return (open_(NULL, filename, mode));
	}

	bool open_(const char *filenameA, const wchar_t *filenameW, uint mode)
	{
		close();

		// mode
		//	MODE_R : read only
		//	MODE_W : read/write
		//	MODE_A : read/write (初期ファイルポインタ最後尾)
		//
		bool writemode;
		DWORD aflag;
		DWORD cflag;
		DWORD sflag = FILE_SHARE_READ | FILE_SHARE_WRITE;

		switch (mode) {
			case MODE_R:
				writemode = false;
				aflag = GENERIC_READ;
				cflag = OPEN_EXISTING;
				break;
			case MODE_W:
				writemode = true;
				aflag = GENERIC_READ | GENERIC_WRITE;
				cflag = CREATE_ALWAYS;
				break;
			case MODE_A:
				writemode = true;
				aflag = GENERIC_READ | GENERIC_WRITE;
				cflag = OPEN_ALWAYS;
				break;
			default:
				return false;
		}

		// open
		if (filenameA) {
			hFile = CreateFileA(
				filenameA, aflag, sflag, NULL, cflag, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		else {
			hFile = CreateFileW(
				filenameW, aflag, sflag, NULL, cflag, FILE_ATTRIBUTE_NORMAL, NULL);
		}

		if (hFile == INVALID_HANDLE_VALUE) {
			return false;
		}

		if (mode == MODE_A)
			seek(0, FILE_END);

		return true;
	}

	uint64 read(void *buffer, uint siz)
	{
		// lock
		OVERLAPPED ov;
		memset(&ov, 0, sizeof(OVERLAPPED));
		if (!LockFileEx(hFile, 0, 0, siz, 0, &ov))
			return (0);
		// read
		DWORD ret;
		if (!ReadFile(hFile, buffer, siz, &ret, NULL))
			return (0);
		// unlock
		UnlockFileEx(hFile, 0, siz, 0, &ov);

		return (ret);
	}

	uint64 write(void *buffer, uint siz)
	{
		// lock
		OVERLAPPED ov;
		memset(&ov, 0, sizeof(OVERLAPPED));
		if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, siz, 0, &ov))
			return (0);
		// write
		DWORD ret;
		if (!WriteFile(hFile, buffer, siz, &ret, NULL))
			return (0);
		// unlock
		UnlockFileEx(hFile, 0, siz, 0, &ov);

		return (ret);
	}

	uint64 seek(uint64 offset, DWORD origin)
	{
		LARGE_INTEGER curpos;
		LARGE_INTEGER newpos;

		curpos.QuadPart = offset;

		if (!SetFilePointerEx(hFile, curpos, &newpos, origin))
			return (0);
		return (newpos.QuadPart);
	}

	uint64 ftell(void)
	{
		LARGE_INTEGER curpos;
		LARGE_INTEGER newpos;

		curpos.QuadPart = 0;

		if (!SetFilePointerEx(hFile, curpos, &newpos, FILE_CURRENT))
			return (0);
		return (newpos.QuadPart);
	}

	void close(void)
	{
		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
		}
	}

	uint64 size(void)
	{
		ULARGE_INTEGER fsize;
		GetFileSizeEx(hFile, (LARGE_INTEGER*)&fsize);
		return (fsize.QuadPart);
	}

private:
	File(const File& rhs);
	File& operator=(const File& rhs);
};




// ---------------------------------------------------------------------------
//
//	class MappedFile
//
//	移植時の注意：排他ロックをかけ、後続のアクセスを待たせるように実装すること
//				  Wait無しですぐエラーを返すような実装はダメ。
//
// ---------------------------------------------------------------------------
class MappedFile
{
protected:
	static DWORD MappedFile::AllocationGranularity;

	HANDLE			hFile;
	HANDLE			hMap;
	void			*addrP;
	OVERLAPPED		*ov;
	ULARGE_INTEGER	fsize;

public:
	MappedFile(void)
		: hFile(INVALID_HANDLE_VALUE)
		, hMap(NULL)
		, addrP(NULL)
		, ov(NULL)
	{
		fsize.QuadPart = 0;

		if (!MappedFile::AllocationGranularity)
			GetAllocationGranularity();
	}

	~MappedFile()
	{
		close();
	}

	static void GetAllocationGranularity(void)
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		MappedFile::AllocationGranularity = si.dwAllocationGranularity;
	}

	void *open(const char *filename, DWORD mapbyte_, bool writemode)
	{
		return (open_(filename, NULL, mapbyte_, writemode));
	}
	void *open(const wchar_t *filename, DWORD mapbyte_, bool writemode)
	{
		return (open_(NULL, filename, mapbyte_, writemode));
	}

	void *open_(const char *filenameA, const wchar_t *filenameW, DWORD mapbyte_, bool writemode)
	{
		close();

		// open file
		DWORD aflag = GENERIC_READ | (writemode ? GENERIC_WRITE : 0);
		DWORD cflag = writemode ? OPEN_ALWAYS : OPEN_EXISTING;
		DWORD sflag = FILE_SHARE_READ | FILE_SHARE_WRITE;

		if (filenameA) {
			hFile = CreateFileA(
				filenameA, aflag, sflag, NULL, cflag, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		else {
			hFile = CreateFileW(
				filenameW, aflag, sflag, NULL, cflag, FILE_ATTRIBUTE_NORMAL, NULL);
		}

		if (hFile == INVALID_HANDLE_VALUE) {
			DWORD err = GetLastError();
			return (NULL);
		}

		// get file size
		if (!GetFileSizeEx(hFile, (LARGE_INTEGER*)&fsize)) {
			close();
			return (NULL);
		}

		// lock
		ov = new OVERLAPPED;
		memset(ov, 0, sizeof(OVERLAPPED));
		if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0,
				fsize.LowPart, fsize.HighPart, ov)) {
			close();
			return (NULL);
		}

		// create file mapping
		aflag = writemode ? PAGE_READWRITE : PAGE_READONLY;
		hMap = CreateFileMapping(hFile, NULL, aflag, 0, 0, NULL);
		if (hMap == NULL) {
			close();
			return (NULL);
		}

		// open view
		DWORD mapbyte = fsize.LowPart < mapbyte_ ? 0 : mapbyte_;
		aflag = writemode ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
		addrP = MapViewOfFile(hMap, aflag, 0, 0, mapbyte);
		if (addrP == NULL) {
			close();
			return (NULL);
		}

		return (addrP);
	}

	void close(void)
	{
		if (addrP) {
			UnmapViewOfFile(addrP);
			addrP = NULL;
		}
		if (hMap) {
			CloseHandle(hMap);
			hMap = NULL;
		}
		if (ov) {
			UnlockFileEx(hFile, 0, fsize.LowPart, fsize.HighPart, ov);
			delete ov;
			ov = NULL;
		}
		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
		}
	}

	uint64 size(void)
	{
		return (fsize.QuadPart);
	}

	DWORD allocG(void)
	{
		return (MappedFile::AllocationGranularity);
	}

private:
	MappedFile(const MappedFile& rhs);
	MappedFile& operator=(const MappedFile& rhs);
};
