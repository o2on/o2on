/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * FILENAME		: main.cpp
 * DESCRIPTION	: o2on internal browser
 *
 */

#define WINVER			0x0500
#define _WIN32_WINNT	0x0500
#define _WIN32_IE		0x0500

// Visual Leak Detector
// http://www.codeproject.com/tools/visualleakdetector.asp
#if 0 && defined(_WIN32) && defined(_DEBUG)
#include <vld.h>
#endif

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlhost.h>
#include <exdispid.h>
#include <shlguid.h>
#include <vector>
#include <algorithm>
#include "resource.h"




// ---------------------------------------------------------------------------
//	macros
// ---------------------------------------------------------------------------
#define BROWSER_CLASS_NAME		"o2browser"
#define OPTION_FILENAME			"conf\\Browser.xml"
#define UM_ACTIVATE				(WM_USER+1)
#define PROGRESSBAR_HEIGHT		6




// ---------------------------------------------------------------------------
//	structs
// ---------------------------------------------------------------------------
class WindowParam
{
public:
	HWND hwnd;
	HWND hwndPB;
	CComQIPtr<IWebBrowser2> pWebBrowser2;
	int x;
	int y;
	int w;
	int h;

	WindowParam(void) : hwnd(NULL), x(0), y(0), w(800), h(600) {}
};

#define SINKID_COUNTEREVENTS0 0

class ATL_NO_VTABLE IESink
	: public CComObjectRootEx<CComSingleThreadModel>
	, public IDispEventImpl<SINKID_COUNTEREVENTS0, IESink, &DIID_DWebBrowserEvents2>
{
private:
	HWND hwndFrame;
	HWND hwndPB;
	CComPtr<IUnknown> pUnknownIE;
public:
	IESink(void);
	void SetFrameWindowHandle(HWND hwnd);
	void SetProgressBarWindowHandle(HWND hwnd);
	HRESULT AdviseToIE(CComPtr<IUnknown> pUnknown);

	BEGIN_COM_MAP(IESink)
		COM_INTERFACE_ENTRY_IID(DIID_DWebBrowserEvents2, IESink)
	END_COM_MAP()

	BEGIN_SINK_MAP(IESink)
		SINK_ENTRY_EX(SINKID_COUNTEREVENTS0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2, OnNewWindow2)
		SINK_ENTRY_EX(SINKID_COUNTEREVENTS0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete2)
		SINK_ENTRY_EX(SINKID_COUNTEREVENTS0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, OnDocumentComplete2)
		SINK_ENTRY_EX(SINKID_COUNTEREVENTS0, DIID_DWebBrowserEvents2, DISPID_TITLECHANGE, OnTitleChange)
		SINK_ENTRY_EX(SINKID_COUNTEREVENTS0, DIID_DWebBrowserEvents2, DISPID_PROGRESSCHANGE, OnProgressChange)
		SINK_ENTRY_EX(SINKID_COUNTEREVENTS0, DIID_DWebBrowserEvents2, DISPID_ONQUIT, OnQuit)
	END_SINK_MAP()

	void _stdcall OnNewWindow2(IDispatch** ppDisp, VARIANT_BOOL *cancel);
	void _stdcall OnNavigateComplete2(IDispatch* pDisp, VARIANT* pvUrl);
	void _stdcall OnDocumentComplete2(IDispatch* pDisp, VARIANT* pvUrl);
	void _stdcall OnTitleChange(BSTR text);
	void _stdcall OnProgressChange(long progress, long progressMax);
	void _stdcall OnQuit();
};




// ---------------------------------------------------------------------------
//	file-scope variables
// ---------------------------------------------------------------------------
static HINSTANCE			instance;
static std::vector<HWND>	WindowHandles;
static CComModule			_Module;
BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()




// ---------------------------------------------------------------------------
//	function prototypes
// ---------------------------------------------------------------------------
static bool
InitializeApp(TCHAR *cmdline, int showstat);
static void
FinalizeApp(void);
static WindowParam*
CreateBrowserWindow(TCHAR *url);
static LRESULT CALLBACK
BrowserWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static WindowParam*
GetParam(HWND hwnd);
static void
SetParam(HWND hwnd, WindowParam* param);
static HWND
GetIEWindow(HWND hwnd);
static bool
TranslateWBAccelerator(HWND hwndIE, MSG *msg);
static bool
SaveOption(TCHAR *filename, WindowParam *param);
static bool
LoadOption(TCHAR *filename, WindowParam *param);





// ---------------------------------------------------------------------------
//	_tWinMain
//	Win32エントリ関数
// ---------------------------------------------------------------------------
int APIENTRY
_tWinMain(HINSTANCE inst, HINSTANCE previnst, TCHAR *cmdline, int cmdshow)
{
	HWND hwnd;
	if ((hwnd = FindWindow(_T(BROWSER_CLASS_NAME), NULL)) != NULL) {
		size_t cmdlen = _tcslen(cmdline);
		if (cmdlen > 0) {
			COPYDATASTRUCT cds;
			cds.dwData = 0;
			cds.cbData = (cmdlen+1) * sizeof(TCHAR);
			cds.lpData = cmdline;
			SendMessage(hwnd, WM_COPYDATA, NULL, (LPARAM)&cds);
		}
		else
			SendMessage(hwnd, UM_ACTIVATE, 0, 0);
		return (0);
	}

	instance = inst;
	if (!InitializeApp(cmdline, cmdshow))
		return (0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
			TCHAR classname[256];
			GetClassName(msg.hwnd, classname, 255);
			if(_tcscmp(classname, _T("Internet Explorer_Server")) == 0) {
				if (TranslateWBAccelerator(msg.hwnd, &msg))
					continue;
			}
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return ((int)msg.wParam);
}




// ---------------------------------------------------------------------------
//	InitializeApp
//	アプリケーションの初期化処理
// ---------------------------------------------------------------------------
static bool
InitializeApp(TCHAR *cmdline, int cmdshow)
{
	_Module.Init(ObjectMap, instance);
	AtlAxWinInit();

	if (!CreateBrowserWindow(cmdline))
		return false;

	SetProcessWorkingSetSize(GetCurrentProcess(), 0xffffffff, 0xffffffff);
	return true;
}




// ---------------------------------------------------------------------------
//	FinalizeApp
//	アプリケーションの終了処理
// ---------------------------------------------------------------------------
static void
FinalizeApp(void)
{
}




// ---------------------------------------------------------------------------
//	CreateBrowser
//	ブラウザウィンドウ作成
// ---------------------------------------------------------------------------
static WindowParam*
CreateBrowserWindow(TCHAR *url)
{
	WNDCLASSEX wc;
	if (!GetClassInfoEx(instance, TEXT(BROWSER_CLASS_NAME), &wc)) {
		WNDCLASSEX wc;
		wc.cbSize			= sizeof(WNDCLASSEX);
		wc.style			= CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc		= BrowserWindowProc;
		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= 0;
		wc.hInstance		= instance;
		wc.hIcon			= LoadIcon(instance, MAKEINTRESOURCE(IDI_O2ON));
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground	= NULL;//(HBRUSH)(COLOR_MENU + 1);
		wc.lpszMenuName 	= NULL;
		wc.lpszClassName	= _T(BROWSER_CLASS_NAME);
		wc.hIconSm			= NULL;

		if (!RegisterClassEx(&wc))
			return (NULL);
	}

	WindowParam a;
	if (WindowHandles.empty()) {
		LoadOption(_T(OPTION_FILENAME), &a);
	}
	else {
		RECT rect;
		GetWindowRect(WindowHandles[0], &rect);
		a.x = rect.left;
		a.y = rect.top;
		a.w = rect.right - rect.left;
		a.h = rect.bottom - rect.top;
	}

	// FrameWindow
	HWND hwnd = CreateWindowEx(
		0,
		_T(BROWSER_CLASS_NAME),
		_T(""),
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		a.x, a.y, a.w, a.h,
		NULL,
		(HMENU)0,
		instance,
		NULL);

	if (!hwnd)
		return (NULL);

	// Progress Bar
    HWND hwndPB = CreateWindowEx(
		0,
		PROGRESS_CLASS,
		NULL, 
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | PBS_SMOOTH,
		0, 0, 0, 0,
        hwnd,
		(HMENU)0,
		instance,
		NULL);

	// IE
	HWND hwndBrowser = CreateWindowEx(
		WS_EX_STATICEDGE,
		_T("AtlAxWin80"),
		_T("Shell.Explorer.2"),
		WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0,
		hwnd,
		(HMENU)1,
		instance,
		NULL);

	if (!hwndBrowser)
		return (NULL);

	CComPtr<IUnknown> pUnknownIE;
	if (AtlAxGetControl(hwndBrowser, &pUnknownIE) != S_OK)
		return (NULL);

	CComObject<IESink>* sink;
	CComObject<IESink>::CreateInstance(&sink);
	sink->SetFrameWindowHandle(hwnd);
	sink->SetProgressBarWindowHandle(hwndPB);
	HRESULT hr = sink->AdviseToIE(pUnknownIE);
	if (FAILED(hr))
		return (NULL);

	WindowParam *param = new WindowParam;
	param->hwnd = hwndBrowser;
	param->hwndPB = hwndPB;
	param->pWebBrowser2 = pUnknownIE;

	SetParam(hwnd, param);
	WindowHandles.push_back(hwnd);

	if (url && _tcslen(url) > 0) {
		CComVariant v_url(url);
		CComVariant v_empty;
		param->pWebBrowser2->Navigate2(&v_url, &v_empty, &v_empty, &v_empty, &v_empty);
	}

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	return (param);
}




// ---------------------------------------------------------------------------
//	BrowserWindowProc
//	ブラウザウィンドウのプロシージャ
// ---------------------------------------------------------------------------
static LRESULT CALLBACK
BrowserWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case UM_ACTIVATE: {
			DWORD thread_me  = GetWindowThreadProcessId(hwnd, NULL);
			DWORD thread_cur = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
			AttachThreadInput(thread_me, thread_cur, TRUE);

			DWORD timeout;
			SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &timeout, 0);
			SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (LPVOID)0, 0);
			
			SetForegroundWindow(hwnd);
			
			SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (LPVOID)timeout, 0);
			AttachThreadInput(thread_me, thread_cur, FALSE);
			return (0);
		}

		case WM_COPYDATA: {
			COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lp;
			if (cds->cbData > 0)
				CreateBrowserWindow((TCHAR*)cds->lpData);
			return (0);
		}

		case WM_MOVE:
			if (!IsIconic(hwnd)) {
				WindowParam *param = GetParam(hwnd);
				if (param) {
					RECT rect;
					GetWindowRect(hwnd, &rect);
					param->x = rect.left;
					param->y = rect.top;
				}
			}
			return (0);

		case WM_SIZE:
			if (wp != SIZE_MINIMIZED) {
				WindowParam *param = GetParam(hwnd);
				if (param) {
					MoveWindow(param->hwnd, 0, 0, LOWORD(lp), HIWORD(lp) - PROGRESSBAR_HEIGHT, TRUE);
					MoveWindow(param->hwndPB, 0, HIWORD(lp) - PROGRESSBAR_HEIGHT, LOWORD(lp), PROGRESSBAR_HEIGHT, TRUE);
					RECT rect;
					GetWindowRect(hwnd, &rect);
					param->w = rect.right - rect.left;
					param->h = rect.bottom - rect.top;
				}
			}
			return (0);

		case WM_QUERYENDSESSION:
			return TRUE;

		case WM_ENDSESSION:
			if ((BOOL)wp == TRUE)
				FinalizeApp();
			return (0);

		case WM_CLOSE:
			FinalizeApp();
			DestroyWindow(hwnd);
			return (0);

		case WM_DESTROY: {
			WindowParam *param = GetParam(hwnd);
			if (param->pWebBrowser2)
				param->pWebBrowser2.Release();
			SetParam(hwnd, NULL);

			std::vector<HWND>::iterator it = 
				std::find(WindowHandles.begin(), WindowHandles.end(), hwnd);
			WindowHandles.erase(it);

			if (WindowHandles.empty()) {
				SaveOption(_T(OPTION_FILENAME), param);
				PostQuitMessage(0);
			}
			delete param;
			return (0);
		}
	}
	return (DefWindowProc(hwnd, msg, wp, lp));
}




// ---------------------------------------------------------------------------
//	GetParam
//	SetParam
//	GetIEWindow
//	TranslateWBAccelerator
// ---------------------------------------------------------------------------
static WindowParam*
GetParam(HWND hwnd)
{
	return ((WindowParam*)GetWindowLongPtr(hwnd, GWLP_USERDATA));
}
static void
SetParam(HWND hwnd, WindowParam* param)
{
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)param);
}
static HWND
GetIEWindow(HWND hwnd)
{
	HWND hwndIE;
	hwndIE = GetWindow(hwnd, GW_CHILD);
	hwndIE = GetWindow(hwndIE, GW_CHILD);
	hwndIE = GetWindow(hwndIE, GW_CHILD);
	return (hwndIE);
}
static bool
TranslateWBAccelerator(HWND hwndIE, MSG *msg)
{
	CComPtr<IHTMLDocument2> pDoc;
	DWORD_PTR lResult;

	UINT nMsg = RegisterWindowMessage(_T("WM_HTML_GETOBJECT"));
	SendMessageTimeout(hwndIE, nMsg, 0L, 0L, SMTO_ABORTIFHUNG, 1000, (DWORD_PTR*)&lResult);

	HRESULT hr;
	hr = ObjectFromLresult(lResult, __uuidof(IHTMLDocument2),0,(void**)&pDoc);
	if (FAILED(hr))
		return false;

	CComQIPtr<IServiceProvider> psp1 = pDoc;
	if (!psp1)
		return false;

	CComQIPtr<IServiceProvider> psp2;
	psp1->QueryService(SID_STopLevelBrowser, IID_IServiceProvider, (void**)(&psp2));
	if (!psp2)
		return false;

	CComPtr<IWebBrowser2> pWebBrowser2;
	psp2->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, (void**)(&pWebBrowser2));
	if (!pWebBrowser2)
		return false;

	if (msg->message == WM_KEYDOWN) {
		switch ((int)msg->wParam) {
			case VK_ESCAPE:
				pWebBrowser2->Stop();
				return true;
			case VK_F4:
				if (GetKeyState(VK_CONTROL))
					pWebBrowser2->Refresh();
				return true;
			case VK_F5:
				pWebBrowser2->Refresh();
				return true;
		}
	}
	CComQIPtr<IOleInPlaceActiveObject> pIOIPAO = pWebBrowser2;
	return (pIOIPAO->TranslateAccelerator(msg) == S_OK ? true : false);
}




// ---------------------------------------------------------------------------
//	SaveOption
//	LoadOption
// ---------------------------------------------------------------------------
static bool
SaveOption(TCHAR *filename, WindowParam *param)
{
	FILE *fp;
	_tfopen_s(&fp, filename, _T("wb"));
	if (!fp)
		return false;

	fprintf_s(fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	fprintf_s(fp, "<option>\r\n");
	fprintf_s(fp, " <x>%d</x>\r\n", param->x);
	fprintf_s(fp, " <y>%d</y>\r\n", param->y);
	fprintf_s(fp, " <w>%d</w>\r\n", param->w);
	fprintf_s(fp, " <h>%d</h>\r\n", param->h);
	fprintf_s(fp, "</option>\r\n");

	fclose(fp);
	return true;
}
static bool
LoadOption(TCHAR *filename, WindowParam *param)
{
	struct _stat st;
	if (_tstat(filename, &st) != 0)
		return false;

	FILE *fp;
	_tfopen_s(&fp, filename, _T("rb"));
	if (!fp)
		return false;

	char *buff = new char[(size_t)st.st_size];
	fread(buff, 1, st.st_size, fp);

	char *p;
	if ((p = strstr(buff, "<x>")) != NULL)
		param->x = atoi(p+3);
	if ((p = strstr(buff, "<y>")) != NULL)
		param->y = atoi(p+3);
	if ((p = strstr(buff, "<w>")) != NULL)
		param->w = atoi(p+3);
	if ((p = strstr(buff, "<h>")) != NULL)
		param->h = atoi(p+3);

	fclose(fp);
	return true;
}




// ---------------------------------------------------------------------------
//	IESink
//	
// ---------------------------------------------------------------------------
IESink::
IESink(void)
	: hwndFrame(NULL)
{
}

void
IESink::
SetFrameWindowHandle(HWND hwnd)
{
	hwndFrame = hwnd;
}

void
IESink::
SetProgressBarWindowHandle(HWND hwnd)
{
	hwndPB = hwnd;
}

HRESULT
IESink::
AdviseToIE(CComPtr<IUnknown> pUnknown)
{
	pUnknownIE = pUnknown;
	AtlGetObjectSourceInterface(pUnknownIE, &m_libid, &m_iid, &m_wMajorVerNum, &m_wMinorVerNum);
	return(this->DispEventAdvise(pUnknownIE));
}

void _stdcall
IESink::
OnNewWindow2(IDispatch** ppDisp, VARIANT_BOOL *cancel)
{
	WindowParam *param = CreateBrowserWindow(NULL);
	if (!param) {
		*cancel = VARIANT_TRUE;
		return;
	}

	param->pWebBrowser2->QueryInterface(IID_IDispatch, (void**)ppDisp);
}

void _stdcall
IESink::
OnNavigateComplete2(IDispatch* pDisp, VARIANT* pvUrl)
{
}

void _stdcall
IESink::
OnDocumentComplete2(IDispatch* pDisp, VARIANT* pvUrl)
{
	CComQIPtr<IWebBrowser2> pWB = pDisp;
	if (!pWB) return;

	CComPtr<IDispatch> pDisp2;
	HRESULT hr = pWB->get_Document(&pDisp2);
	if (FAILED(hr))
		return;

	CComQIPtr<IHTMLDocument2> pDoc = pDisp2;
	if (!pDoc)
		return;

	CComPtr<IHTMLWindow2> pWindow;
	hr = pDoc->get_parentWindow(&pWindow);
	if (FAILED(hr))
		return;

	pWindow->focus();

	SetProcessWorkingSetSize(GetCurrentProcess(), 0xffffffff, 0xffffffff);
}

void _stdcall
IESink::
OnTitleChange(BSTR text)
{
	SetWindowText(hwndFrame, text);
}

void _stdcall
IESink::
OnProgressChange(long progress, long progressMax)
{
	SendMessage(hwndPB, PBM_SETRANGE32, (WPARAM)0, (LPARAM)progressMax);
	SendMessage(hwndPB, PBM_SETPOS, (WPARAM)progress, 0);
}

void _stdcall
IESink::
OnQuit()
{
	DispEventUnadvise(pUnknownIE);
}
