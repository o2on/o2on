/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * FILENAME		: main.cpp
 * DESCRIPTION	: o2on client main source code
 *
 */

#define WINVER			0x0500
#define _WIN32_WINNT	0x0500
#define _WIN32_IE		0x0500

//Visual Leak Detector
//http://www.codeproject.com/tools/visualleakdetector.asp
#if 0 && defined(_WIN32) && defined(_DEBUG)
#include <vld.h>
#endif

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <iphlpapi.h>
#include <math.h>
#include "O2DatDB.h"
#include "O2Server_HTTP_P2P.h"
#include "O2Server_HTTP_Proxy.h"
#include "O2Server_HTTP_Admin.h"
#include "O2Agent.h"
#include "O2Job_GetGlobalIP.h"
#include "O2Job_QueryDat.h"
#include "O2Job_DatCollector.h"
#include "O2Job_AskCollection.h"
#include "O2Job_PublishKeys.h"
#include "O2Job_PublishOriginal.h"
#include "O2Job_NodeCollector.h"
#include "O2Job_Search.h"
#include "O2Job_SearchFriends.h"
#include "O2Job_Broadcast.h"
#include "O2Job_ClearWorkset.h"
#include "O2Job_AutoSave.h"
#include "O2PerformanceCounter.h"
#include "O2ReportMaker.h"
#include "O2Boards.h"
#include "O2ProgressInfo.h"
#include "O2Version.h"
#include "file.h"
#include "upnp.h"
#include "resource.h"
#include <boost/dynamic_bitset.hpp>




// ---------------------------------------------------------------------------
//	macros
// ---------------------------------------------------------------------------
#define CLASS_NAME				"o2on"
#define UM_TRAYICON				(WM_USER+1)
#define UM_EMERGENCYHALT		(WM_USER+2)
#define UM_SHOWBALOON			(WM_USER+3)
#define UM_SETICON				(WM_USER+4)
#define UM_DLGREFRESH			(WM_USER+5)
#define UM_UPNP_START_TEST		(WM_USER+7)
#define UM_UPNP_END_TEST		(WM_USER+8)
#define IDT_TRAYICON			0
#define IDT_INICONTIMER			1
#define IDT_OUTICONTIMER 		2
#define IDT_PROGRESSTIMER 		3
#define IDT_AUTORESUME	 		4




// ---------------------------------------------------------------------------
//	file-scope variables
// ---------------------------------------------------------------------------
static HINSTANCE				instance;
static HWND						hwndMain;
static HWND						hwndProgressDlg;
static HWND						hwndUPnPDlg;
static HANDLE					ThreadHandle;
static UINT						TaskbarRestartMsg;
static int						CurrentProperyPage;
static bool						VisibleOptionDialog;
static bool						PropRet;
static UINT						InIconTimer;
static UINT						OutIconTimer;
static bool						Active;
static bool						P2PStopBySuspend;
static UINT						ResumeTimer;
static HANDLE					UPnPThreadHandle;
static bool						UPnPLoop;
static UPnPService				*UPnPServiceUsingForPortMapping;

static O2Logger					*Logger;
static O2Profile				*Profile;
static O2Profile				*ProfBuff;
static O2IPFilter				*IPF_P2P;
static O2IPFilter				*IPF_Proxy;
static O2IPFilter				*IPF_Admin;
static O2DatDB					*DatDB;
static O2DatIO					*DatIO;
static O2NodeDB					*NodeDB;
static O2FriendDB				*FriendDB;
static O2KeyDB					*KeyDB;
static O2KeyDB					*SakuKeyDB;
static O2KeyDB					*QueryDB;
static O2KeyDB					*SakuDB;
static O2IMDB					*IMDB;
static O2IMDB					*BroadcastDB;
static O2Boards					*Boards;
static O2Server_HTTP_P2P		*Server_P2P;
static O2Server_HTTP_Proxy		*Server_Proxy;
static O2Server_HTTP_Admin		*Server_Admin;
static O2Agent					*Agent;
static O2Job_GetGlobalIP		*Job_GetGlobalIP;
static O2Job_DatCollector		*Job_DatCollector;
static O2Job_AskCollection		*Job_AskCollection;
static O2Job_QueryDat			*Job_QueryDat;
static O2Job_NodeCollector		*Job_NodeCollector;
static O2Job_PublishKeys		*Job_PublishKeys;
static O2Job_PublishOriginal	*Job_PublishOriginal;
static O2Job_Search				*Job_Search;
static O2Job_SearchFriends		*Job_SearchFriends;
static O2Job_Broadcast			*Job_Broadcast;
static O2Job_ClearWorkset		*Job_ClearWorkset;
static O2Job_AutoSave			*Job_AutoSave;
static O2PerformanceCounter		*PerformanceCounter;
static O2LagQueryQueue			*LagQueryQueue;
static O2ReportMaker			*ReportMaker;
static O2ProgressInfo			ProgressInfo;





// ---------------------------------------------------------------------------
//	function prototypes
// ---------------------------------------------------------------------------
static bool
InitializeApp(TCHAR *cmdline, int showstat);
static void
FinalizeApp(void);
static uint WINAPI
FinalizeAppThread(void *param);

static LRESULT CALLBACK
MainWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void
OptionDialog(void);
static INT_PTR CALLBACK
GeneralDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static INT_PTR CALLBACK
ProfileDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static INT_PTR CALLBACK
P2PDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static INT_PTR CALLBACK
BrowserDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static INT_PTR CALLBACK
BaloonDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static INT_PTR CALLBACK
QuarterDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void
OpenSelectBrowserDialog(void);
static INT_PTR CALLBACK
SelectBrowserDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void
OpenUPnPDialog(HWND hwnd, ushort port);
static INT_PTR CALLBACK
UPnPDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static INT_PTR CALLBACK
UPnPErrorDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void
AddUPnPInfoMessage(const char *msg);
static uint WINAPI
UPnP_PortMappingTestThread(void *data);
static void
UPnP_PortMappingTest(void *data);
static bool
UPnP_AddPortMapping(ushort port, const char *adapterName, const char *location, const char *serviceId, string &log);
static bool
UPnP_DeletePortMapping(string &log);

static void
CreateProgressDialog(TCHAR *title);
static INT_PTR CALLBACK
ProgressDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void
DlgNumSet(HWND hwnd, uint id, uint64 n, uint64 max);
static bool
DlgNumCheck(HWND hwnd, uint id, uint pageNo, TCHAR *name, uint64 min, uint64 max, uint64 &n);
static bool
IsChecked(HWND hwnd);

static void
AddTrayIcon(UINT id);
static void
ChangeTrayIcon(UINT id);
static void
DeleteTrayIcon(void);
static void
ShowTrayBaloon(const TCHAR *title, const TCHAR *msg, UINT timeout, DWORD infoflag);
static void
MakeTrayIconTipString(NOTIFYICONDATA *nid);

static bool
CheckPort(void);
static bool
StartProxy(wchar_t *addmsg);
static bool
StartAdmin(wchar_t *addmsg);
static bool
StartP2P(bool baloon);
static bool
StopP2P(bool baloon);

static void
OpenBrowser(const wstring &type, const wstring &path);
static void
GetInternalBrowserPath(tstring &path);

static void
SetWindowPosAuto(HWND hwnd);
static void
SetWindowPosToCorner(HWND hwnd);
static void
ChangeToModuleDir(void);
static void
GetModuleDirectory(TCHAR *module_dir);




// ---------------------------------------------------------------------------
//	_tWinMain
//	Win32エントリ関数
// ---------------------------------------------------------------------------
int APIENTRY
_tWinMain(HINSTANCE inst, HINSTANCE previnst, TCHAR *cmdline, int cmdshow)
{
//bench();
//return (0);

	if (FindWindow(_T(CLASS_NAME), NULL))
		return (0);

	instance = inst;
	if (!InitializeApp(cmdline, cmdshow))
		return (0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!hwndProgressDlg || !IsDialogMessage(hwndProgressDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
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
	wchar_t msg[1024];

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icex);

	CoInitialize(NULL); 
	
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);
	XMLPlatformUtils::Initialize();

	TaskbarRestartMsg = RegisterWindowMessage(_T("TaskbarCreated"));
	if (!O2DEBUG) {
		ChangeToModuleDir();
	}

	//
	//	Logger
	//
	Logger = new O2Logger(NULL/*L"logs"*/);

	//
	//	Profile
	//
	Profile = new O2Profile(Logger, true);
	if (!Profile->MakeConfDir()) {
		swprintf_s(msg, 1024,
			L"ディレクトリ「%s」の作成に失敗しました\n起動を中止します",
			Profile->GetConfDirW());
		MessageBoxW(NULL, msg, NULL, MB_ICONERROR | MB_OK);
		return false;
	}
	if (!Profile->MakeDBDir()) {
		swprintf_s(msg, 1024,
			L"ディレクトリ「%s」の作成に失敗しました\n起動を中止します",
			Profile->GetDBDirW());
		MessageBoxW(NULL, msg, NULL, MB_ICONERROR | MB_OK);
		return false;
	}
	if (!Profile->MakeCacheRoot()) {
		swprintf_s(msg, 1024,
			L"ディレクトリ「%s」の作成に失敗しました\n起動を中止します",
			Profile->GetCacheRootW());
		MessageBoxW(NULL, msg, NULL, MB_ICONERROR | MB_OK);
		return false;
	}
	if (!Profile->CheckAdminRoot()) {
		swprintf_s(msg, 1024,
			L"ディレクトリ「%s」が存在しません\n起動を中止します",
			Profile->GetAdminRootW());
		MessageBox(NULL, msg, NULL, MB_ICONERROR | MB_OK);
		return false;
	}
	Logger->SetLimit(LOGGER_LOG, Profile->GetLogLimit());
	Logger->SetLimit(LOGGER_NETLOG, Profile->GetNetLogLimit());
	Logger->SetLimit(LOGGER_HOKANLOG, Profile->GetHokanLogLimit());
	Logger->SetLimit(LOGGER_IPFLOG, Profile->GetIPFLogLimit());

#if 0 && defined(_DEBUG)
	string s;
	Profile->SetIP(inet_addr("192.168.0.99"));
	//Profile->SetIP(inet_addr("61.203.18.50"));
	Profile->GetEncryptedProfile(s);
	Profile->SetIP(0);
	TRACEA(s.c_str());
	TRACEA("\n");
#endif

	//
	//	IPFilter
	//
	IPF_P2P	= new O2IPFilter(L"P2P", Logger);
	if (!IPF_P2P->Load(Profile->GetIPF_P2PFilePath())) {
		IPF_P2P->setdefault(O2_ALLOW);
	}
	IPF_Proxy	= new O2IPFilter(L"Proxy", Logger);
	if (!IPF_Proxy->Load(Profile->GetIPF_ProxyFilePath())) {
		IPF_Proxy->setdefault(O2_DENY);
		IPF_Proxy->add(true, O2_ALLOW, L"127.0.0.1/255.255.255.255");
		IPF_Proxy->add(true, O2_ALLOW, L"192.168.0.0/255.255.0.0");
	}
	IPF_Admin	= new O2IPFilter(L"Admin", Logger);
	if (!IPF_Admin->Load(Profile->GetIPF_AdminFilePath())) {
		IPF_Admin->setdefault(O2_DENY);
		IPF_Admin->add(true, O2_ALLOW, L"127.0.0.1/255.255.255.255");
		IPF_Admin->add(true, O2_ALLOW, L"192.168.0.0/255.255.0.0");
	}

	//
	//	DatDB
	//
	wstring dbfilename(Profile->GetDBDirW());
	dbfilename += L"\\dat.db";
	DatDB = new O2DatDB(Logger, dbfilename.c_str());
	if (!DatDB->create_table()) {
		MessageBox(NULL,
			L"DBオープンに失敗しました\n起動を中止します",
			NULL, MB_ICONERROR | MB_OK);
		return false;
	}
	DatDB->StartUpdateThread();

	//
	//	Agent
	//
	Agent = new O2Agent(L"Agent", Logger);
	Agent->SetRecvSizeLimit(RECV_SIZE_LIMIT);
	Agent->ClientStart();

	//
	//	DBs
	//
	DatIO = new O2DatIO(DatDB, Logger, Profile, &ProgressInfo);
	NodeDB = new O2NodeDB(Logger, Profile, Agent);
	hashT myID;
	Profile->GetID(myID);
	NodeDB->SetSelfNodeID(myID);
	NodeDB->SetSelfNodePort(Profile->GetP2PPort());
#if 0 && defined(_DEBUG)
	StopWatch *sw = new StopWatch("BIGNODE");
	NodeDB->Load(L"doc\\BIGNodeList.xml");
	delete sw;
	sw = new StopWatch("BIGNODE");
	NodeDB->Save(L"doc\\BIGNodeList_save.xml");
	delete sw;
	return false;
#endif
	NodeDB->Load(Profile->GetNodeFilePath());
	FriendDB = new O2FriendDB(Logger, NodeDB);
	FriendDB->Load(Profile->GetFriendFilePath());
	KeyDB = new O2KeyDB(L"KeyDB", false, Logger);
	KeyDB->SetSelfNodeID(myID);
	KeyDB->SetLimit(Profile->GetKeyLimit());
	SakuKeyDB = new O2KeyDB(L"SakuKeyDB", false, Logger);
	SakuKeyDB->SetSelfNodeID(myID);
	SakuKeyDB->SetLimit(O2_SAKUKEY_LIMIT);
	QueryDB = new O2KeyDB(L"QueryDB", true, Logger);
	QueryDB->Load(Profile->GetQueryFilePath());
	SakuDB = new O2KeyDB(L"SakuDB", true, Logger);
	SakuDB->Load(Profile->GetSakuFilePath());
	IMDB = new O2IMDB(Logger);
	IMDB->Load(Profile->GetIMFilePath());
	BroadcastDB = new O2IMDB(Logger);
	LagQueryQueue = new O2LagQueryQueue(Logger, Profile, QueryDB);
	// Boards
	wstring brdfile(Profile->GetConfDirW());
	brdfile += L"\\2channel.brd";
	wstring exbrdfile(Profile->GetConfDirW());
	exbrdfile += L"\\BoardEx.xml";
	Boards = new O2Boards(Logger, Profile, Agent, brdfile.c_str(), exbrdfile.c_str());
	if (!Boards->Load()) {
		brdfile += L".default";
		Boards->Load(brdfile.c_str());
		Boards->Save();
	}
	Boards->LoadEx();
	Profile->SetDatStorageFlag(Boards->SizeEx() ? true : false);

	//
	//	Jobs
	//
	PerformanceCounter = new O2PerformanceCounter(
		L"PerformanceCounter",
		JOB_INTERVAL_PERFORMANCE_COUNTER,
		false,
		Logger);
	PerformanceCounter->Load(Profile->GetReportFilePath());
	Job_GetGlobalIP = new O2Job_GetGlobalIP(
		L"GetGlobalIP",
		JOB_INTERVAL_GET_GLOBAL_IP,
		true,
		Logger,
		Profile,
		NodeDB,
		Agent);
	Job_QueryDat = new O2Job_QueryDat(
		L"QueryDat",
		JOB_INTERVAL_QUERY_DAT,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		QueryDB,
		DatIO,
		Agent);
	Job_DatCollector = new O2Job_DatCollector(
		L"DatCollector",
		JOB_INTERVAL_DAT_COLLECTOR,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		QueryDB,
		DatIO,
		Boards,
		Agent);
	Job_AskCollection = new O2Job_AskCollection(
		L"AskCollection",
		JOB_INTERVAL_ASK_COLLECTION,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		Boards,
		Agent);
	Job_PublishKeys = new O2Job_PublishKeys(
		L"PublishKey",
		JOB_INTERVAL_PUBLISH_KEYS,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		SakuKeyDB,
		Agent);
	Job_PublishOriginal = new O2Job_PublishOriginal(
		L"PublishOriginal",
		JOB_INTERVAL_PUBLISH_ORIGINAL,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		SakuDB,
		DatIO,
		DatDB,
		Agent);
	Job_NodeCollector = new O2Job_NodeCollector(
		L"NodeCollector",
		JOB_INTERVAL_COLLECT_NODE,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		Agent,
		Job_PublishOriginal);
	Job_Search = new O2Job_Search(
		L"Search",
		JOB_INTERVAL_SEARCH,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		QueryDB,
		Agent,
		Job_QueryDat);
	Job_SearchFriends = new O2Job_SearchFriends(
		L"SearchFriends",
		JOB_INTERVAL_SEARCHFRIENDS,
		true,
		Logger,
		Profile,
		NodeDB,
		FriendDB,
		Agent);
	Job_Broadcast = new O2Job_Broadcast(
		L"Broadcast",
		JOB_INTERVAL_BROADCAST,
		false,
		Logger,
		Profile,
		NodeDB,
		KeyDB,
		BroadcastDB,
		Agent);
	Job_ClearWorkset = new O2Job_ClearWorkset(
		L"ClearWorkset",
		JOB_INTERVAL_WORKSET_CLEAR,
		false);
	Job_AutoSave = new O2Job_AutoSave(
		L"AutoSave",
		JOB_INTERVAL_AUTO_SAVE,
		false,
		Profile,
		NodeDB);

	Agent->Add(PerformanceCounter);
	Agent->Add(Job_GetGlobalIP);
	Agent->Add(Job_QueryDat);
	Agent->Add(Job_DatCollector);
	Agent->Add(Job_AskCollection);
	Agent->Add(Job_PublishKeys);
	Agent->Add(Job_PublishOriginal);
	Agent->Add(Job_NodeCollector);
	Agent->Add(Job_Search);
	Agent->Add(Job_SearchFriends);
	Agent->Add(Job_Broadcast);
	Agent->Add(Job_ClearWorkset);
	Agent->Add(Job_AutoSave);

	//
	//	Servers
	//
	Server_P2P = new O2Server_HTTP_P2P(
		Logger,
		IPF_P2P,
		Profile,
		DatIO,
		Boards,
		NodeDB,
		KeyDB,
		SakuKeyDB,
		QueryDB,
		IMDB,
		BroadcastDB,
		Job_Broadcast);
	Server_P2P->SetMultiLinkRejection(false);
	Server_P2P->SetSessionLimit(Profile->GetP2PSessionLimit());
	Server_P2P->SetRecvSizeLimit(RECV_SIZE_LIMIT);
	Server_P2P->SetPort(Profile->GetP2PPort());

	Server_Proxy = new O2Server_HTTP_Proxy(
		Logger,
		IPF_Proxy,
		Profile,
		DatIO,
		Boards,
		LagQueryQueue);
	Server_Proxy->SetMultiLinkRejection(false);
	Server_Proxy->SetPort(Profile->GetProxyPort());

	Server_Admin = new O2Server_HTTP_Admin(
		Logger,
		IPF_Admin,
		Profile,
		DatDB,
		DatIO,
		NodeDB,
		FriendDB,
		KeyDB,
		SakuKeyDB,
		QueryDB,
		SakuDB,
		IMDB,
		BroadcastDB,
		IPF_P2P,
		IPF_Proxy,
		IPF_Admin,
		Job_Broadcast,
		Agent,
		Boards);
	Server_Admin->SetMultiLinkRejection(false);
	Server_Admin->SetPort(Profile->GetAdminPort());

	//
	//	ReportMaker
	//
	ReportMaker = new O2ReportMaker(
		Logger,
		Profile,
		DatDB,
		DatIO,
		NodeDB,
		FriendDB,
		KeyDB,
		SakuKeyDB,
		QueryDB,
		SakuDB,
		IMDB,
		BroadcastDB,
		IPF_P2P,
		IPF_Proxy,
		IPF_Admin,
		PerformanceCounter,
		Server_P2P,
		Server_Proxy,
		Server_Admin,
		Agent,
		Job_QueryDat,
		Job_Broadcast);
	ReportMaker->PushJob(Job_GetGlobalIP);
	ReportMaker->PushJob(Job_NodeCollector);
	ReportMaker->PushJob(Job_PublishKeys);
	ReportMaker->PushJob(Job_PublishOriginal);
	ReportMaker->PushJob(Job_Search);
	ReportMaker->PushJob(Job_SearchFriends);
	ReportMaker->PushJob(Job_Broadcast);
	ReportMaker->PushJob(Job_QueryDat);
	ReportMaker->PushJob(Job_DatCollector);
	ReportMaker->PushJob(Job_AskCollection);

	Server_P2P->SetReportMaker(ReportMaker);
	Server_Admin->SetReportMaker(ReportMaker);

	// メインウィンドウクラス登録
	WNDCLASSEX wc;
	wc.cbSize			= sizeof(WNDCLASSEX);
	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= MainWindowProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= instance;
	wc.hIcon			= LoadIcon(instance, MAKEINTRESOURCE(IDI_O2ON));
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;//(HBRUSH)(COLOR_MENU + 1);
	wc.lpszMenuName 	= NULL;
	wc.lpszClassName	= _T(CLASS_NAME);
	wc.hIconSm			= NULL;

	if (!RegisterClassEx(&wc))
		return false;

	// メインウィンドウ作成
	hwndMain = CreateWindowEx(
		WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
		_T(CLASS_NAME),
		_T(APP_NAME),
		WS_POPUP,
		-1, -1, 0, 0,
		NULL,
		(HMENU)0,
		instance,
		NULL);

	if (!hwndMain)
		return false;

	// コールバックメッセージ登録
	LagQueryQueue->SetBaloonCallbackMsg(hwndMain, UM_SHOWBALOON);
	Server_P2P->SetIconCallbackMsg(hwndMain, UM_SETICON);
	Server_P2P->SetBaloonCallbackMsg(hwndMain, UM_SHOWBALOON);
	Server_Proxy->SetIconCallbackMsg(hwndMain, UM_SETICON);
	Server_Admin->SetIconCallbackMsg(hwndMain, UM_SETICON);
	Server_Admin->SetBaloonCallbackMsg(hwndMain, UM_SHOWBALOON);
	Agent->SetIconCallbackMsg(hwndMain, UM_SETICON);
	Job_DatCollector->SetBaloonCallbackMsg(hwndMain, UM_SHOWBALOON);
	Job_QueryDat->SetBaloonCallbackMsg(hwndMain, UM_SHOWBALOON);

	// ProxyとAdminを起動
	if (!StartProxy(L"\n\no2onの起動を中止します"))
		return false;
	if (!StartAdmin(L"\n\no2onの起動を中止します"))
		return false;

	// トレイアイコン追加
	AddTrayIcon(IDI_DISABLE);

	if (!CheckPort()) {
		ShowTrayBaloon(_T("o2on"),
			_T("オプションでポート番号を設定してください"),
			5*1000, NIIF_INFO);
	}
	else {
		// 自動起動
		if (Profile->IsP2PAutoStart())
			StartP2P(true);
	}

	CLEAR_WORKSET;
	return true;
}




// ---------------------------------------------------------------------------
//	FinalizeApp
//	アプリケーションの終了処理
// ---------------------------------------------------------------------------
static void
FinalizeApp(void)
{
	CreateProgressDialog(_T("o2on終了..."));

	ThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, FinalizeAppThread, NULL, 0, NULL);

	MSG msg;
	while (ThreadHandle) {
		while (PeekMessage(&msg, hwndProgressDlg, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(1);
	}

	HWND hwnd;
	while ((hwnd = FindWindow(_T("o2browser"), NULL)) != NULL) {
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}
}

static uint WINAPI
FinalizeAppThread(void *param)
{
	CoInitialize(NULL);

	DatIO->StopRebuildDB();

	ProgressInfo.Reset(true, false);
	ProgressInfo.AddMax(7);

	if (Profile->IsBaloon_P2P())
		ShowTrayBaloon(L"o2on", L"o2onを終了しています…", 5*1000, NIIF_INFO);

	ProgressInfo.SetMessage(L"P2Pを終了しています");
	StopP2P(false);
	ProgressInfo.AddPos(1);

	ProgressInfo.SetMessage(L"Agentを終了しています");
	Agent->ClientStop();
	ProgressInfo.AddPos(1);

	ProgressInfo.SetMessage(L"Proxyを終了しています");
	Server_Proxy->Stop();
	ProgressInfo.AddPos(1);

	ProgressInfo.SetMessage(L"Adminを終了しています");
	Server_Admin->Stop();
	ProgressInfo.AddPos(1);

	ProgressInfo.SetMessage(L"設定を保存しています");

	Profile->Save();
	NodeDB->Save(Profile->GetNodeFilePath());
	FriendDB->Save(Profile->GetFriendFilePath());
	QueryDB->Save(Profile->GetQueryFilePath());
	SakuDB->Save(Profile->GetSakuFilePath());
	IMDB->Save(Profile->GetIMFilePath(), false);
	IPF_P2P->Save(Profile->GetIPF_P2PFilePath());
	IPF_Proxy->Save(Profile->GetIPF_ProxyFilePath());
	IPF_Admin->Save(Profile->GetIPF_AdminFilePath());
	PerformanceCounter->Save(Profile->GetReportFilePath());
	Boards->Save();
	Boards->SaveEx();

	ProgressInfo.AddPos(1);

	if (Agent)
		delete Agent;

	if (ReportMaker)
		delete ReportMaker;

	if (PerformanceCounter)
		delete PerformanceCounter;
	if (Job_GetGlobalIP)
		delete Job_GetGlobalIP;
	if (Job_QueryDat)
		delete Job_QueryDat;
	if (Job_DatCollector)
		delete Job_DatCollector;
	if (Job_AskCollection)
		delete Job_AskCollection;
	if (Job_PublishKeys)
		delete Job_PublishKeys;
	if (Job_PublishOriginal)
		delete Job_PublishOriginal;
	if (Job_NodeCollector)
		delete Job_NodeCollector;
	if (Job_Search)
		delete Job_Search;
	if (Job_SearchFriends)
		delete Job_SearchFriends;
	if (Job_Broadcast)
		delete Job_Broadcast;
	if (Job_ClearWorkset)
		delete Job_ClearWorkset;
	if (Job_AutoSave)
		delete Job_AutoSave;

	if (Server_P2P)
		delete Server_P2P;
	if (Server_Proxy)
		delete Server_Proxy;
	if (Server_Admin)
		delete Server_Admin;

	if (DatIO)
		delete DatIO;
	if (NodeDB)
		delete NodeDB;
	if (FriendDB)
		delete FriendDB;
	if (KeyDB)
		delete KeyDB;
	if (SakuKeyDB)
		delete SakuKeyDB;
	if (QueryDB)
		delete QueryDB;
	if (SakuDB)
		delete SakuDB;
	if (IMDB)
		delete IMDB;
	if (BroadcastDB)
		delete BroadcastDB;
	if (LagQueryQueue)
		delete LagQueryQueue;
	if (Boards)
		delete Boards;

	if (DatDB) {
		DatDB->StopUpdateThread();
		delete DatDB;
	}

	if (IPF_P2P)
		delete IPF_P2P;
	if (IPF_Proxy)
		delete IPF_Proxy;
	if (IPF_Admin)
		delete IPF_Admin;

	if (Profile)
		delete Profile;
	if (Logger)
		delete Logger;

	ProgressInfo.Reset(false, false);
	DeleteTrayIcon();
	XMLPlatformUtils::Terminate();
	WSACleanup();
	CoUninitialize(); 

	CloseHandle(ThreadHandle);
	ThreadHandle = NULL;
	//_endthreadex(0);
	return (0);
}




// ---------------------------------------------------------------------------
//	MainWindowProc
//	メインウィンドウのプロシージャ
// ---------------------------------------------------------------------------
static LRESULT CALLBACK
MainWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case UM_TRAYICON: {
			switch (lp) {
				case WM_LBUTTONUP:
				case WM_RBUTTONUP:
					if (hwndProgressDlg) {
						SetForegroundWindow(hwndProgressDlg);
					}
					else if (VisibleOptionDialog) {
						NULL;
					}
					else {
						HMENU menu;
						HMENU subm;
						menu = LoadMenu(instance, MAKEINTRESOURCE(IDR_TRAYPOPUP));
						subm = GetSubMenu(menu, 0);
						EnableMenuItem(subm, ID_START, MF_BYCOMMAND | (Active ? MF_GRAYED : MF_ENABLED));
						EnableMenuItem(subm, ID_STOP,  MF_BYCOMMAND | (Active ? MF_ENABLED : MF_GRAYED));
						EnableMenuItem(subm, 6, MF_BYPOSITION | (Active ? MF_GRAYED : MF_ENABLED));
						POINT pt;
						GetCursorPos(&pt);
						SetForegroundWindow(hwndMain);
						TrackPopupMenuEx(subm, 0, pt.x, pt.y, hwndMain, NULL);
						PostMessage(hwnd, WM_NULL, 0, 0);
						DestroyMenu(menu);
					}
					break;
			}
			return (0);
		}

		case UM_EMERGENCYHALT: {
			//必ずPOSTで
			if (Active)
				StopP2P(false);
			if (InIconTimer) {
				KillTimer(hwnd, InIconTimer);
				InIconTimer = 0;
			}
			if (OutIconTimer) {
				KillTimer(hwnd, OutIconTimer);
				OutIconTimer = 0;
			}
			ChangeTrayIcon(IDI_ERR);

			TCHAR *reason;
			switch (wp) {
				case 0:
					reason = _T("dat容量超過");
					break;
				default:
					reason = _T("");
					break;
			}
			if (Logger) {
				Logger->AddLog(O2LT_FATAL, L"o2on", 0, 0,
					L"o2onを緊急停止しました（%s）", reason);
			}
			ShowTrayBaloon(_T("緊急停止"), reason, 60*1000, NIIF_ERROR);
			return (0);
		}

		case UM_SHOWBALOON:
			//必ずSENDで
			ShowTrayBaloon((TCHAR*)wp, (TCHAR*)lp, 5*1000, NIIF_INFO);
			return (0);

		case UM_SETICON: {
			if (!Active)
				return (0);
			UINT icon_id = 0;
			switch (wp) {
			case 0:
				if (InIconTimer) {
					KillTimer(hwnd, InIconTimer);
				}
				InIconTimer = SetTimer(hwnd, IDT_INICONTIMER, 1000, NULL);
				icon_id = (OutIconTimer ? IDI_A_INOUT : IDI_A_IN);
				break;
			case 1:
				if (OutIconTimer) {
					KillTimer(hwnd, OutIconTimer);
				}
				OutIconTimer = SetTimer(hwnd, IDT_OUTICONTIMER, 1000, NULL);
				icon_id = (InIconTimer ? IDI_A_INOUT : IDI_A_OUT);
				break;
			}
			if (icon_id)
				ChangeTrayIcon(icon_id);
			return (0);
		}

		case WM_TIMER: {
			UINT icon_id = 0;
			switch (wp) {
			case IDT_INICONTIMER:
				KillTimer(hwnd, InIconTimer);
				InIconTimer = 0;
				icon_id = (OutIconTimer ? IDI_A_OUT : IDI_A);
				break;
			case IDT_OUTICONTIMER:
				KillTimer(hwnd, OutIconTimer);
				OutIconTimer = 0;
				icon_id = (InIconTimer ? IDI_A_IN : IDI_A);
				break;
			case IDT_AUTORESUME:
				KillTimer(hwnd, ResumeTimer);
				ResumeTimer = 0;
				StartP2P(true);
				Logger->AddLog(O2LT_INFO, L"General", 0, 0, L"OSレジュームにより起動");
				break;
			}
			if (Active && icon_id)
				ChangeTrayIcon(icon_id);
			return (0);
		}

		case WM_COMMAND: {
			UINT code = LOWORD(wp);
			UINT id = HIWORD(wp);
			HWND hwndCtl = (HWND)lp;

			switch (code) {
				case ID_START:
					if (ResumeTimer) {
						KillTimer(hwnd, ResumeTimer);
						ResumeTimer = 0;
					}
					StartP2P(true);
					break;
				case ID_STOP:
					StopP2P(true);
					break;
				case ID_REBUILDDB:
					if (!Active && !hwndProgressDlg) {
						CreateProgressDialog(_T("DB再構築..."));
						DatIO->RebuildDB();
					}
					break;
				case ID_REINDEX:
					if (!Active && !hwndProgressDlg) {
						CreateProgressDialog(_T("reindex..."));
						DatIO->Reindex();
					}
					break;
				case ID_OPENWEBADMIN: {
					wstring type = Profile->GetAdminBrowserType();
					wstring path = Profile->GetAdminBrowserPath();

					if (type != L"select")
						OpenBrowser(type, path);
					else
						OpenSelectBrowserDialog();
					break;
				}
				case ID_OPTION:
					OptionDialog();
					break;
				case ID_ABOUT: {
					TCHAR tmp[128];
					_stprintf_s(tmp, 128,
						_T(APP_VER_FORMAT)_T("\nProtocol version: O2/%.1f\n\nPlatform: %s"),
						_T(APP_NAME), APP_VER_MAJOR, APP_VER_MINOR,
						_T(APP_VER_PREFIX), APP_BUILDNO,
						PROTOCOL_VER, _T(O2_PLATFORM));
					MessageBox(hwnd, tmp, _T("About o2on"), MB_OK|MB_ICONINFORMATION);
					break;
				}
				case ID_EXIT:
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					break;
			}
			return (0);
		}

		case WM_POWERBROADCAST:
			if (wp == PBT_APMQUERYSUSPEND) {
				if (Active && Profile->IsAutoResume()) {
					Logger->AddLog(O2LT_INFO, L"General", 0, 0, L"OSサスペンドにより停止");
					StopP2P(false);
					P2PStopBySuspend = true;
				}
			}
			else if (wp == PBT_APMRESUMESUSPEND) {
				if (P2PStopBySuspend) {
					P2PStopBySuspend = false;
					ResumeTimer = SetTimer(hwnd, IDT_AUTORESUME, Profile->GetResumeDelayMs(), NULL);
				}
			}
			return TRUE;

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

		case WM_DESTROY:
			PostQuitMessage(0);
			return (0);

		default:
			if (msg == TaskbarRestartMsg) {
				AddTrayIcon(Active ? IDI_A : IDI_DISABLE);
			}
			break;
	}
	return (DefWindowProc(hwnd, msg, wp, lp));
}




// ---------------------------------------------------------------------------
//	OptionDialog
//	オプションダイアログ表示
// ---------------------------------------------------------------------------
static void
OptionDialog(void)
{
	if (VisibleOptionDialog)
		return;
	
	std::vector<PROPSHEETPAGE> psps;
	PROPSHEETPAGE psp;

	psp.dwSize = sizeof(PROPSHEETPAGE);
	psp.dwFlags = PSP_USETITLE;
	psp.hInstance = instance;
	psp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_GENERAL);
	psp.pszIcon = NULL;
	psp.pfnDlgProc = GeneralDlgProc;
	psp.pszTitle = L"基本設定";
	psp.lParam = psps.size();
	psp.pfnCallback = NULL;
	psps.push_back(psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_PROFILE);
	psp.pfnDlgProc = ProfileDlgProc;
	psp.pszTitle = L"プロフィール";
	psp.lParam = psps.size();
	psps.push_back(psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_P2P);
	psp.pfnDlgProc = P2PDlgProc;
	psp.pszTitle = L"通信とバッファ";
	psp.lParam = psps.size();
	psps.push_back(psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_BROWSER);
	psp.pfnDlgProc = BrowserDlgProc;
	psp.pszTitle = L"ブラウザ";
	psp.lParam = psps.size();
	psps.push_back(psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_BALOON);
	psp.pfnDlgProc = BaloonDlgProc;
	psp.pszTitle = L"バルーン通知";
	psp.lParam = psps.size();
	psps.push_back(psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_QUARTER);
	psp.pfnDlgProc = QuarterDlgProc;
	psp.pszTitle = L"クォータ";
	psp.lParam = psps.size();
	psps.push_back(psp);

	PROPSHEETHEADER psh;
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_NOAPPLYNOW | PSH_PROPSHEETPAGE;
	psh.hwndParent = hwndMain;
	psh.hInstance = instance;
	psh.pszIcon = NULL;
	psh.pszCaption = L"オプション";
	psh.nPages = psps.size();
	psh.nStartPage = CurrentProperyPage;
	psh.ppsp = (LPCPROPSHEETPAGE)&psps[0];
	psh.pfnCallback = NULL;
	
	ProfBuff = new O2Profile(Logger, false);
	ProfBuff->assign(*Profile);

	VisibleOptionDialog = true;
	PropertySheet(&psh);
	VisibleOptionDialog = false;

	if (!PropRet) {
		delete ProfBuff;
		ProfBuff = NULL;
		return;
	}

	bool p2p_port_changed = false;
	bool proxy_port_changed = false;
	bool admin_port_changed = false;

	if (Profile->GetP2PPort() != ProfBuff->GetP2PPort())
		p2p_port_changed = true;
	if (Profile->GetProxyPort() != ProfBuff->GetProxyPort())
		proxy_port_changed = true;
	if (Profile->GetAdminPort() != ProfBuff->GetAdminPort())
		admin_port_changed = true;

	Profile->assign(*ProfBuff);
	delete ProfBuff;
	ProfBuff = NULL;
	Profile->Save();

	// Proxyのポートが変わっていたら再起動
	if (proxy_port_changed || !Server_Proxy->IsActive()) {
		Server_Proxy->Stop();
		Server_Proxy->SetPort(Profile->GetProxyPort());
		StartProxy(NULL);
	}
	// Adminのポートが変わっていたら再起動
	if (admin_port_changed || !Server_Admin->IsActive()) {
		Server_Admin->Stop();
		Server_Admin->SetPort(Profile->GetAdminPort());
		StartAdmin(NULL);
	}

	// 各種パラメータセット
	NodeDB->SetSelfNodePort(Profile->GetP2PPort());
	KeyDB->SetLimit(Profile->GetKeyLimit());
	QueryDB->SetLimit(Profile->GetQueryLimit());
	Logger->SetLimit(LOGGER_LOG, Profile->GetLogLimit());
	Logger->SetLimit(LOGGER_NETLOG, Profile->GetNetLogLimit());
	Logger->SetLimit(LOGGER_HOKANLOG, Profile->GetHokanLogLimit());
	Logger->SetLimit(LOGGER_IPFLOG, Profile->GetIPFLogLimit());

	if (p2p_port_changed) {
		Server_P2P->SetPort(Profile->GetP2PPort());
		if (Active) {
			if (Profile->IsBaloon_P2P())
				ShowTrayBaloon(L"o2on", L"P2Pを再起動しています…", 5*1000, NIIF_INFO);
			StopP2P(false);
			StartP2P(true);
		}
	}
}




#define GD(a,b) GetDlgItem((a),(b))

// ---------------------------------------------------------------------------
//	GeneralDlgProc
//	基本設定ダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
GeneralDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT pageNo;
	uint64 n = 0;
	
	switch (msg) {
	case WM_INITDIALOG:
		if (ProfBuff->GetP2PPort() != 0)
		DlgNumSet(hwnd, IDC_P2P_PORT,		ProfBuff->GetP2PPort(),			65535);
		DlgNumSet(hwnd, IDC_PROXY_PORT,		ProfBuff->GetProxyPort(),		65535);
		DlgNumSet(hwnd, IDC_ADMIN_PORT,		ProfBuff->GetAdminPort(),		65535);
		DlgNumSet(hwnd, IDC_RESUMEDELAY,	ProfBuff->GetResumeDelayMs()/1000, 99999);

		Button_SetCheck(GD(hwnd, IDC_PORT0),			ProfBuff->IsPort0());
		Button_SetCheck(GD(hwnd, IDC_USE_UPNP),			ProfBuff->UsingUPnP());
		Button_SetCheck(GD(hwnd, IDC_P2PAUTOSTART),		ProfBuff->IsP2PAutoStart());
		Button_SetCheck(GD(hwnd, IDC_AUTORESUME),		ProfBuff->IsAutoResume());
		Button_SetCheck(GD(hwnd, IDC_MARUUSER),			ProfBuff->IsMaruUser());

		EnableWindow(GD(hwnd, IDC_P2P_PORT), !ProfBuff->IsPort0());
		EnableWindow(GD(hwnd, IDC_RESUMEDELAY), ProfBuff->IsAutoResume());

		pageNo = ((PROPSHEETPAGE*)lp)->lParam;
		if (!IsWindowVisible(GetParent(hwnd)))
			SetWindowPosAuto(GetParent(hwnd));
		PostMessage(hwnd, UM_DLGREFRESH, 0, 0);
		return TRUE;

	case UM_DLGREFRESH:
		{
			string adapter = ProfBuff->GetUPnPAdapterName();
			string location = ProfBuff->GetUPnPLocation();
			string serviceId = ProfBuff->GetUPnPServiceId();
			if (adapter.empty() || location.empty() || serviceId.empty()) {
				EnableWindow(GD(hwnd, IDC_USE_UPNP), FALSE);
				Button_SetCheck(GD(hwnd, IDC_USE_UPNP), FALSE);
			}
			else
				EnableWindow(GD(hwnd, IDC_USE_UPNP), TRUE);
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wp)) {
			case IDC_PORT0:
				if (Button_GetCheck(GD(hwnd, IDC_PORT0))) {
					EnableWindow(GD(hwnd, IDC_P2P_PORT), FALSE);
					Edit_SetText(GD(hwnd, IDC_P2P_PORT), _T("0"));
				}
				else {
					EnableWindow(GD(hwnd, IDC_P2P_PORT), TRUE);
				}
				return TRUE;
			case IDC_AUTORESUME:
				EnableWindow(GD(hwnd, IDC_RESUMEDELAY), Button_GetCheck(GD(hwnd, IDC_AUTORESUME)));
				return TRUE;
			case IDC_UPNPCONFIG: {
					TCHAR portstr[16];
					Edit_GetText(GD(hwnd, IDC_P2P_PORT), portstr, 16);
					ulong port = _tcstoul(portstr, NULL, 10);
					if (port == 0UL || port > 65535UL) {
						MessageBox(hwnd,
							L"設定にはP2Pポート番号が必要です",
							NULL, MB_OK);
						return TRUE;
					}
					OpenUPnPDialog(hwnd, (ushort)port);
					PostMessage(hwnd, UM_DLGREFRESH, 0, 0);
				}
				return TRUE;
		}
		break;

	case WM_NOTIFY:
		switch (((NMHDR *)lp)->code) {
		case PSN_SETACTIVE:
			CurrentProperyPage = pageNo;
			return TRUE;
		case PSN_APPLY:

			ProfBuff->SetPort0(IsChecked(GD(hwnd, IDC_PORT0)));
			ProfBuff->SetUseUPnP(IsChecked(GD(hwnd, IDC_USE_UPNP)));
			ProfBuff->SetP2PAutoStart(IsChecked(GD(hwnd, IDC_P2PAUTOSTART)));
			ProfBuff->SetAutoResume(IsChecked(GD(hwnd, IDC_AUTORESUME)));
			ProfBuff->SetMaruUser(IsChecked(GD(hwnd, IDC_MARUUSER)));

			if (!ProfBuff->IsPort0()) {
				if (!DlgNumCheck(hwnd, IDC_P2P_PORT, pageNo, L"ポート番号", 1024, 65535, n))
					return TRUE;
			}
			if (!ProfBuff->SetP2PPort((ushort)n)) {
				MessageBox(hwnd, L"同じポート番号が設定されています", NULL, MB_OK|MB_ICONERROR);
				return TRUE;
			}

			if (!DlgNumCheck(hwnd, IDC_PROXY_PORT, pageNo, L"ポート番号", 1024, 65535, n))
				return TRUE;
			if (!ProfBuff->SetProxyPort((ushort)n)) {
				MessageBox(hwnd, L"同じポート番号が設定されています", NULL, MB_OK|MB_ICONERROR);
				return TRUE;
			}

			if (!DlgNumCheck(hwnd, IDC_ADMIN_PORT, pageNo, L"ポート番号", 1024, 65535, n))
				return TRUE;
			if (!ProfBuff->SetAdminPort((ushort)n)) {
				MessageBox(hwnd, L"同じポート番号が設定されています", NULL, MB_OK|MB_ICONERROR);
				return TRUE;
			}

			if (!DlgNumCheck(hwnd, IDC_RESUMEDELAY, pageNo, L"開始時ディレイ", 0, 9999, n))
				return TRUE;
			ProfBuff->SetResumeDelayMs((uint)n*1000);

			PropRet = true;
			return TRUE;
		case PSN_RESET:
			PropRet = false;
			return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	ProfileDlgProc
//	プロフィールダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
ProfileDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT pageNo;

	switch (msg) {
	case WM_INITDIALOG: {
		Edit_LimitText(GD(hwnd, IDC_NAME), O2_MAX_NAME_LEN);
		SetDlgItemText(hwnd, IDC_NAME, ProfBuff->GetNodeNameW());

		Edit_LimitText(GD(hwnd, IDC_COMMENT), O2_MAX_COMMENT_LEN);
		SetDlgItemText(hwnd, IDC_COMMENT, ProfBuff->GetComment());
		wstring comment(ProfBuff->GetComment());

		Button_SetCheck(GD(hwnd, IDC_PUBLICREPORT),	ProfBuff->IsPublicReport());
		Button_SetCheck(GD(hwnd, IDC_PUBLICRECENTDAT),	ProfBuff->IsPublicRecentDat());

		pageNo = ((PROPSHEETPAGE*)lp)->lParam;
		if (!IsWindowVisible(GetParent(hwnd)))
			SetWindowPosAuto(GetParent(hwnd));
		return TRUE;
	}

	case WM_NOTIFY:
		switch (((NMHDR *)lp)->code) {
		case PSN_SETACTIVE:
			CurrentProperyPage = pageNo;
			return TRUE;
		case PSN_APPLY: {
			HWND h = GD(hwnd, IDC_NAME);
			if (Edit_GetTextLength(h) > O2_MAX_NAME_LEN) {
				MessageBox(hwnd, L"名前が長すぎます", NULL, MB_OK|MB_ICONERROR);
				SetFocus(h);
			}
			wchar_t name[O2_MAX_NAME_LEN+1];
			Edit_GetText(h, name, O2_MAX_NAME_LEN+1);
			//
			h = GD(hwnd, IDC_COMMENT);
			if (Edit_GetTextLength(h) > O2_MAX_COMMENT_LEN) {
				MessageBox(hwnd, L"コメントが長すぎます", NULL, MB_OK|MB_ICONERROR);
				SetFocus(h);
			}
			wchar_t comment[O2_MAX_COMMENT_LEN+1];
			Edit_GetText(h, comment, O2_MAX_COMMENT_LEN+1);
			//
			ProfBuff->SetNodeName(name);
			ProfBuff->SetComment(comment);
			ProfBuff->SetPublicReport(IsChecked(GD(hwnd, IDC_PUBLICREPORT)));
			ProfBuff->SetPublicRecentDat(IsChecked(GD(hwnd, IDC_PUBLICRECENTDAT)));

			PropRet = true;
			return TRUE;
		}
		case PSN_RESET:
			PropRet = false;
			return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	P2PDlgProc
//	通信とバッファ設定ダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
P2PDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT pageNo;
	uint64 n = 0;
	
	switch (msg) {
	case WM_INITDIALOG:
		DlgNumSet(hwnd, IDC_P2P_LIMIT,		ProfBuff->GetP2PSessionLimit(),	99);
		DlgNumSet(hwnd, IDC_KEYLIMIT,		ProfBuff->GetKeyLimit(),		KeyDB->Max());
		DlgNumSet(hwnd, IDC_LOGLIMIT,		ProfBuff->GetLogLimit(),		Logger->Max());
		DlgNumSet(hwnd, IDC_NETLOGLIMIT,	ProfBuff->GetNetLogLimit(),		Logger->Max());
		DlgNumSet(hwnd, IDC_HOKANLOGLIMIT,	ProfBuff->GetHokanLogLimit(),	Logger->Max());
		DlgNumSet(hwnd, IDC_IPFLOGLIMIT,	ProfBuff->GetIPFLogLimit(),		Logger->Max());

		pageNo = ((PROPSHEETPAGE*)lp)->lParam;
		if (!IsWindowVisible(GetParent(hwnd)))
			SetWindowPosAuto(GetParent(hwnd));
		return TRUE;
	/*
	case WM_COMMAND:
		switch (LOWORD(wp)) {
		}
		break;
	*/
	case WM_NOTIFY:
		switch (((NMHDR *)lp)->code) {
		case PSN_SETACTIVE:
			CurrentProperyPage = pageNo;
			return TRUE;
		case PSN_APPLY:
			if (!DlgNumCheck(hwnd, IDC_P2P_LIMIT, pageNo, L"接続数リミット", 5, 99, n))
				return TRUE;
			ProfBuff->SetP2PSessionLimit((uint)n);

			if (!DlgNumCheck(hwnd, IDC_KEYLIMIT, pageNo, L"キー数", KeyDB->Min(), KeyDB->Max() ,n))
				return TRUE;
			ProfBuff->SetKeyLimit((uint)n);

			if (!DlgNumCheck(hwnd, IDC_LOGLIMIT, pageNo, L"ログ数", Logger->Min(), Logger->Max() ,n))
				return TRUE;
			ProfBuff->SetLogLimit((uint)n);

			if (!DlgNumCheck(hwnd, IDC_NETLOGLIMIT, pageNo, L"ログ数", Logger->Min(), Logger->Max() ,n))
				return TRUE;
			ProfBuff->SetNetLogLimit((uint)n);

			if (!DlgNumCheck(hwnd, IDC_HOKANLOGLIMIT, pageNo, L"ログ数", Logger->Min(), Logger->Max() ,n))
				return TRUE;
			ProfBuff->SetHokanLogLimit((uint)n);

			if (!DlgNumCheck(hwnd, IDC_IPFLOGLIMIT, pageNo, L"ログ数", Logger->Min(), Logger->Max() ,n))
				return TRUE;
			ProfBuff->SetIPFLogLimit((uint)n);

			PropRet = true;
			return TRUE;
		case PSN_RESET:
			PropRet = false;
			return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	BrowserDlgProc
//	ブラウザ設定ダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
BrowserDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT pageNo;
	wstring	type;
	wstring path;
	
	switch (msg) {
	case WM_INITDIALOG:
		type = ProfBuff->GetAdminBrowserType();
		path = ProfBuff->GetAdminBrowserPath();

		if (type == L"default")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_DEFAULT), TRUE);
		else if (type == L"internal")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_INTERNAL), TRUE);
		else if (type == L"custom")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_CUSTOM), TRUE);
		else if (type == L"select")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_SELECT), TRUE);
		else
			Button_SetCheck(GD(hwnd, IDC_BROWSER_DEFAULT), TRUE);

		SetDlgItemText(hwnd, IDC_BROWSER_PATH, path.c_str());

		pageNo = ((PROPSHEETPAGE*)lp)->lParam;
		if (!IsWindowVisible(GetParent(hwnd)))
			SetWindowPosAuto(GetParent(hwnd));
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wp)) {
			case IDC_REF: {
				TCHAR curdir[MAX_PATH];
				GetCurrentDirectory(MAX_PATH, curdir);

				OPENFILENAME ofn;
				TCHAR filename[MAX_PATH];
				TCHAR filetitle[MAX_PATH];
				GetWindowText(GD(hwnd, IDC_BROWSER_PATH), filename, MAX_PATH);
				ZeroMemory(&ofn, sizeof(OPENFILENAME));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = TEXT("プログラム (*.exe)\0*.exe\0すべてのファイル (*.*)\0*.*\0\0");
				ofn.lpstrFile = filename;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
				ofn.lpstrDefExt = TEXT("exe");
				ofn.nMaxFileTitle = MAX_PATH;
				ofn.lpstrFileTitle = filetitle;
				ofn.lpstrTitle = TEXT("ブラウザ選択");
				if (GetOpenFileName(&ofn)) {
					SetWindowText(GD(hwnd, IDC_BROWSER_PATH), filename);
				}

				SetCurrentDirectory(curdir);
				return TRUE;
			}
		}
		break;
	case WM_NOTIFY:
		switch (((NMHDR *)lp)->code) {
		case PSN_SETACTIVE:
			CurrentProperyPage = pageNo;
			return TRUE;
		case PSN_APPLY:
			if (Button_GetCheck(GD(hwnd, IDC_BROWSER_DEFAULT)))
				type = L"default";
			else if (Button_GetCheck(GD(hwnd, IDC_BROWSER_INTERNAL)))
				type = L"internal";
			else if (Button_GetCheck(GD(hwnd, IDC_BROWSER_CUSTOM)))
				type = L"custom";
			else if (Button_GetCheck(GD(hwnd, IDC_BROWSER_SELECT)))
				type = L"select";

			path.resize(GetWindowTextLength(GD(hwnd, IDC_BROWSER_PATH))+1);
			GetWindowText(GD(hwnd, IDC_BROWSER_PATH), &path[0], path.size());

			if (type == L"custom" && _waccess(path.c_str(), 0) != 0) {
				MessageBox(hwnd, L"ファイルが存在しません", NULL, MB_ICONERROR|MB_OK);
				SetFocus(GD(hwnd, IDC_BROWSER_PATH));
				SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
				PostMessage(GetParent(hwnd), PSM_SETCURSEL, pageNo, (LPARAM)hwnd);
				return TRUE;
			}

			ProfBuff->SetAdminBrowserType(type.c_str());
			ProfBuff->SetAdminBrowserPath(path.c_str());

			PropRet = true;
			return TRUE;
		case PSN_RESET:
			PropRet = false;
			return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	BaloonDlgProc
//	バルーン設定ダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
BaloonDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT pageNo;
	uint n = 0;
	
	switch (msg) {
	case WM_INITDIALOG:
		Button_SetCheck(GD(hwnd, IDC_BALOON_P2P),	ProfBuff->IsBaloon_P2P());
		Button_SetCheck(GD(hwnd, IDC_BALOON_QUERY),	ProfBuff->IsBaloon_Query());
		Button_SetCheck(GD(hwnd, IDC_BALOON_HOKAN),	ProfBuff->IsBaloon_Hokan());
		Button_SetCheck(GD(hwnd, IDC_BALOON_IM),	ProfBuff->IsBaloon_IM());

		pageNo = ((PROPSHEETPAGE*)lp)->lParam;
		if (!IsWindowVisible(GetParent(hwnd)))
			SetWindowPosAuto(GetParent(hwnd));
		return TRUE;

	case WM_NOTIFY:
		switch (((NMHDR *)lp)->code) {
		case PSN_SETACTIVE:
			CurrentProperyPage = pageNo;
			return TRUE;
		case PSN_APPLY:
			ProfBuff->SetBaloon_P2P(IsChecked(GD(hwnd, IDC_BALOON_P2P)));
			ProfBuff->SetBaloon_Query(IsChecked(GD(hwnd, IDC_BALOON_QUERY)));
			ProfBuff->SetBaloon_Hokan(IsChecked(GD(hwnd, IDC_BALOON_HOKAN)));
			ProfBuff->SetBaloon_IM(IsChecked(GD(hwnd, IDC_BALOON_IM)));

			PropRet = true;
			return TRUE;
		case PSN_RESET:
			PropRet = false;
			return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	QuarterDlgProc
//	クォータ設定ダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
QuarterDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT pageNo;
	static TCHAR *SizeUnit[] = {
		_T("BYTE"),
		_T("KB"),
		_T("MB"),
		_T("GB"),
		_T("TB")
	};

	static TCHAR *Operation[] = {
		_T("o2onを停止する")
	};

	static int current_unit_index = 2;
	uint64 base;
	uint64 prev_size;
	uint64 new_size;
	TCHAR tmp[32];

	switch (msg) {
	case WM_INITDIALOG:
		base = (uint64)pow((double)1024, current_unit_index);
		Button_SetCheck(GD(hwnd, IDC_SETQUARTER), ProfBuff->GetQuarterSize() ? TRUE : FALSE);
		DlgNumSet(hwnd, IDC_QUARTERSIZE, ProfBuff->GetQuarterSize()/base, _UI64_MAX);
		DlgNumSet(hwnd, IDC_CURRENTSIZE, DatDB->select_totaldisksize(), _UI64_MAX);

		for (uint i = 0; i < 5; i++) {
			ComboBox_AddString(GD(hwnd, IDC_QUARTERSIZEUNIT), SizeUnit[i]);
		}
		ComboBox_SetCurSel(GD(hwnd, IDC_QUARTERSIZEUNIT), current_unit_index);

		for (uint i = 0; i < 1; i++) {
			ComboBox_AddString(GD(hwnd, IDC_QUARTERFULLOPERATION), Operation[i]);
		}
		ComboBox_SetCurSel(GD(hwnd, IDC_QUARTERFULLOPERATION), ProfBuff->GetQuarterFullOperation());

		PostMessage(hwnd, UM_DLGREFRESH, 0, 0);

		pageNo = ((PROPSHEETPAGE*)lp)->lParam;
		if (!IsWindowVisible(GetParent(hwnd)))
			SetWindowPosAuto(GetParent(hwnd));
		return TRUE;

	case UM_DLGREFRESH: {
		BOOL flag = Button_GetCheck(GD(hwnd, IDC_SETQUARTER));
		EnableWindow(GD(hwnd, IDC_QUARTERSIZE), flag);
		EnableWindow(GD(hwnd, IDC_QUARTERSIZEUNIT), flag);
		EnableWindow(GD(hwnd, IDC_QUARTERFULLOPERATION), flag);
		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wp)) {
		case IDC_SETQUARTER:
			if (!Button_GetCheck(GD(hwnd, IDC_SETQUARTER))) {
				DlgNumSet(hwnd, IDC_QUARTERSIZE, 0, _UI64_MAX);
			}
			PostMessage(hwnd, UM_DLGREFRESH, 0, 0);
			return TRUE;

		case IDC_QUARTERSIZEUNIT:
			if (HIWORD(wp) == CBN_SELCHANGE) {
				Edit_GetText(GD(hwnd, IDC_QUARTERSIZE), tmp, 32);
				base = (uint64)pow((double)1024, current_unit_index);
				prev_size = _tcstoui64(tmp, NULL, 10) * base;

				current_unit_index = ComboBox_GetCurSel(GD(hwnd, IDC_QUARTERSIZEUNIT));
				base = (uint64)pow((double)1024, current_unit_index);
				new_size = prev_size / base;
				DlgNumSet(hwnd, IDC_QUARTERSIZE, new_size, _UI64_MAX);
			}
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		switch (((NMHDR *)lp)->code) {
		case PSN_SETACTIVE:
			CurrentProperyPage = pageNo;
			return TRUE;
		case PSN_APPLY:
			Edit_GetText(GD(hwnd, IDC_QUARTERSIZE), tmp, 32);
			base = (uint64)pow((double)1024, current_unit_index);
			new_size = _tcstoui64(tmp, NULL, 10) * base;
			ProfBuff->SetQuarterSize(new_size);

			ProfBuff->SetQuarterFullOperation(
				ComboBox_GetCurSel(GD(hwnd, IDC_QUARTERFULLOPERATION)));

			PropRet = true;
			return TRUE;
		case PSN_RESET:
			PropRet = false;
			return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	OpenSelectBrowserDialog
//	ブラウザ選択ダイアログを開く
// ---------------------------------------------------------------------------
void
OpenSelectBrowserDialog(void)
{
	DialogBox(
		instance,
		MAKEINTRESOURCE(IDD_SELECT_BROWSER),
		NULL,
		SelectBrowserDlgProc);
}




// ---------------------------------------------------------------------------
//	SelectBrowserDlgProc
//	ブラウザ選択ダイアログ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
SelectBrowserDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static wstring TmpAdminBrowserType;
	static wstring TmpAdminBrowserPath;
	wstring	type;
	wstring path;
	
	switch (msg) {
	case WM_INITDIALOG:
		if (TmpAdminBrowserType.empty())
			TmpAdminBrowserType = L"default";
		if (TmpAdminBrowserPath.empty())
			TmpAdminBrowserPath = Profile->GetAdminBrowserPath();

		type = TmpAdminBrowserType;
		path = TmpAdminBrowserPath;

		if (type == L"default")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_DEFAULT), TRUE);
		else if (type == L"internal")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_INTERNAL), TRUE);
		else if (type == L"custom")
			Button_SetCheck(GD(hwnd, IDC_BROWSER_CUSTOM), TRUE);
		else
			Button_SetCheck(GD(hwnd, IDC_BROWSER_DEFAULT), TRUE);

		SetDlgItemText(hwnd, IDC_BROWSER_PATH, path.c_str());

		SetWindowPosAuto(hwnd);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wp)) {
			case IDC_REF: {
				TCHAR curdir[MAX_PATH];
				GetCurrentDirectory(MAX_PATH, curdir);

				OPENFILENAME ofn;
				TCHAR filename[MAX_PATH];
				TCHAR filetitle[MAX_PATH];
				GetWindowText(GD(hwnd, IDC_BROWSER_PATH), filename, MAX_PATH);
				ZeroMemory(&ofn, sizeof(OPENFILENAME));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = TEXT("プログラム (*.exe)\0*.exe\0すべてのファイル (*.*)\0*.*\0\0");
				ofn.lpstrFile = filename;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
				ofn.lpstrDefExt = TEXT("exe");
				ofn.nMaxFileTitle = MAX_PATH;
				ofn.lpstrFileTitle = filetitle;
				ofn.lpstrTitle = TEXT("ブラウザ選択");
				if (GetOpenFileName(&ofn)) {
					SetWindowText(GD(hwnd, IDC_BROWSER_PATH), filename);
				}

				SetCurrentDirectory(curdir);
				return TRUE;
			}
			case IDOK:
				if (Button_GetCheck(GD(hwnd, IDC_BROWSER_DEFAULT)))
					type = L"default";
				else if (Button_GetCheck(GD(hwnd, IDC_BROWSER_INTERNAL)))
					type = L"internal";
				else if (Button_GetCheck(GD(hwnd, IDC_BROWSER_CUSTOM)))
					type = L"custom";

				path.resize(GetWindowTextLength(GD(hwnd, IDC_BROWSER_PATH))+1);
				GetWindowText(GD(hwnd, IDC_BROWSER_PATH), &path[0], path.size());

				if (type == L"custom" && _waccess(path.c_str(), 0) != 0) {
					MessageBox(hwnd, L"ファイルが見つかりません", NULL, MB_ICONERROR|MB_OK);
					SetFocus(GD(hwnd, IDC_BROWSER_PATH));
					return TRUE;
				}

				OpenBrowser(type.c_str(), path.c_str());

				TmpAdminBrowserType = type;
				TmpAdminBrowserPath = path;
				EndDialog(hwnd, wp);
				return TRUE;
			case IDCANCEL:
				EndDialog(hwnd, wp);
				return TRUE;
		}
		break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	OpenUPnPDialog
//	UPnPダイアログを開く
// ---------------------------------------------------------------------------
void
OpenUPnPDialog(HWND hwnd, ushort port)
{
	DialogBoxParam(
		instance,
		MAKEINTRESOURCE(IDD_UPNP),
		hwnd,
		UPnPDlgProc,
		(LPARAM)port);
}




// ---------------------------------------------------------------------------
//	UPnPDlgProc
//	UPnPダイアログ
// ---------------------------------------------------------------------------
struct UPnPTestParam {
	ulong ip;
	ushort port;
	UPnPObjectList objects;
	UPnPServiceList validServices;
	string log;
};
struct UPnPErrorDlgParam {
	wstring caption;
	string *log;
};

static INT_PTR CALLBACK
UPnPDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static IP_ADAPTER_INFO *info;
	static UPnPTestParam testparam;

	switch (msg) {
		case WM_INITDIALOG: {
			hwndUPnPDlg = hwnd;
			HWND hwndList = GD(hwnd, IDC_NICLIST);

			testparam.ip = 0;
			testparam.port = (ushort)lp;
			testparam.objects.clear();
			testparam.validServices.clear();

			info = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
			ULONG buffsize = sizeof(IP_ADAPTER_INFO);
			if (GetAdaptersInfo(info, &buffsize) != ERROR_SUCCESS) {
				free(info);
				info = (IP_ADAPTER_INFO*)malloc(buffsize);
			}
			if (GetAdaptersInfo(info, &buffsize) == ERROR_SUCCESS) {
				IP_ADAPTER_INFO *p;
				for (p = info; p != NULL; p = p->Next) {
					string tmp;
					tmp = p->IpAddressList.IpAddress.String;
					tmp += " / ";
					tmp += p->Description;
					wstring str;
					ToUnicode(L"shift_jis", tmp, str);
					int index = ListBox_AddString(hwndList, str.c_str());
					ListBox_SetItemData(hwndList, index, p);
				}
			}

			SetWindowPosAuto(hwnd);
			return TRUE;
		}
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_NICLIST:
					if (HIWORD(wp) == LBN_SELCHANGE) {
						HWND hwndList = GD(hwnd, IDC_NICLIST);
						IP_ADAPTER_INFO *p = (IP_ADAPTER_INFO*)ListBox_GetItemData(hwndList,
																ListBox_GetCurSel(hwndList));
						testparam.ip = inet_addr(p->IpAddressList.IpAddress.String);
						EnableWindow(GD(hwnd, IDC_SEARCHIGD), TRUE);
					}
					return TRUE;
				case IDC_SERVICELIST:
					if (HIWORD(wp) == LBN_SELCHANGE)
						EnableWindow(GD(hwnd, IDOK), TRUE);
					return TRUE;
				case IDC_SEARCHIGD:
					PostMessage(hwnd, UM_UPNP_START_TEST, 0, 0);
					return TRUE;
				case IDOK: {
						HWND hwndList = GD(hwnd, IDC_NICLIST);
						IP_ADAPTER_INFO *p = (IP_ADAPTER_INFO*)ListBox_GetItemData(hwndList,
																ListBox_GetCurSel(hwndList));
						ProfBuff->SetUPnPAdapterName(p->AdapterName);

						hwndList = GD(hwnd, IDC_SERVICELIST);
						UPnPService *svc = (UPnPService*)ListBox_GetItemData(hwndList,
																ListBox_GetCurSel(hwndList));
						ProfBuff->SetUPnPLocation(svc->rootObject->location.c_str());
						ProfBuff->SetUPnPServiceId(svc->serviceId.c_str());
					}
					//↓スルー
				case IDCANCEL:
					EndDialog(hwnd, wp);
					hwndUPnPDlg = NULL;
					free(info);
					info = NULL;
					return TRUE;
			}
			break;

		case UM_UPNP_START_TEST:
			EnableWindow(GD(hwnd, IDC_NICLIST), FALSE);
			EnableWindow(GD(hwnd, IDC_SERVICELIST), FALSE);
			EnableWindow(GD(hwnd, IDC_SEARCHIGD), FALSE);
			EnableWindow(GD(hwnd, IDOK), FALSE);
			EnableWindow(GD(hwnd, IDCANCEL), FALSE);
			Edit_SetText(GD(hwnd, IDC_INFO), L"");
			testparam.validServices.clear();
			testparam.log.clear();
			ListBox_ResetContent(GD(hwnd, IDC_SERVICELIST));
			UPnPLoop = true;
			UPnPThreadHandle = (HANDLE)_beginthreadex(
				NULL, 0, UPnP_PortMappingTestThread, (void*)&testparam, 0, NULL);
			return TRUE;

		case UM_UPNP_END_TEST:
			EnableWindow(GD(hwnd, IDC_NICLIST), TRUE);
			EnableWindow(GD(hwnd, IDC_SERVICELIST), TRUE);
			EnableWindow(GD(hwnd, IDC_SEARCHIGD), TRUE);
			EnableWindow(GD(hwnd, IDCANCEL), TRUE);
			if (!testparam.validServices.empty()) {
				HWND hwndList = GD(hwnd, IDC_SERVICELIST);
				for (size_t i = 0; i < testparam.validServices.size(); i++) {
					string ipstr;
					UPnPAction *action = testparam.validServices[i].getAction("GetExternalIPAddress");
					if (action) {
						UPnPArgument *arg = action->getArgument("NewExternalIPAddress");
						if (arg)
							ipstr = arg->value;
					}
					if (ipstr.empty())
						ipstr = "(グローバルIP不明)";
					string tmp;
					tmp = testparam.validServices[i].rootObject->device.modelName + " / " + ipstr;
					wstring str;
					ToUnicode(L"shift_jis", tmp, str);
					int index = ListBox_AddString(hwndList, str.c_str());
					ListBox_SetItemData(hwndList, index, (LPARAM)&testparam.validServices[i]);
				}
			}
			else {
				UPnPErrorDlgParam param;
				param.caption = L"利用可能なUPnPサービスが見つかりませんでした";
				param.log = &testparam.log;
				DialogBoxParam(
					instance,
					MAKEINTRESOURCE(IDD_UPNP_ERROR),
					hwnd,
					UPnPErrorDlgProc,
					(LPARAM)&param);
			}
			return TRUE;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	UPnPErrorDlgProc
//	
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
UPnPErrorDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UPnPErrorDlgParam *param;

	switch (msg) {
		case WM_INITDIALOG: {
				param = (UPnPErrorDlgParam*)lp;
				SetDlgItemText(hwnd, IDC_CAPTION, param->caption.c_str());
				wstring str;
				ToUnicode(L"shift_jis", *param->log, str);
				Edit_SetText(GD(hwnd, IDC_LOG), str.c_str());
				SetWindowPosAuto(hwnd);
			}
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_COPYLOG: {
					OpenClipboard(NULL);
					HGLOBAL h = GlobalAlloc(GMEM_FIXED, param->log->size()+1);
					strcpy_s((char*)h, param->log->size()+1, param->log->c_str());
					EmptyClipboard();
					SetClipboardData(CF_TEXT, h);
					CloseClipboard();
					return TRUE;
				}
				case IDC_SAVELOG: {
					TCHAR curdir[MAX_PATH];
					GetCurrentDirectory(MAX_PATH, curdir);

					OPENFILENAME ofn;
					TCHAR filename[MAX_PATH];
					TCHAR filetitle[MAX_PATH];
					_tcscpy_s(filename, MAX_PATH, _T("upnplog.txt"));
					ZeroMemory(&ofn, sizeof(OPENFILENAME));
					ofn.lStructSize = sizeof(OPENFILENAME);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFilter = TEXT("テキスト文書(*.txt)\0*.txt\0すべてのファイル (*.*)\0*.*\0\0");
					ofn.lpstrFile = filename;
					ofn.nMaxFile = MAX_PATH;
					ofn.Flags = OFN_OVERWRITEPROMPT;
					ofn.lpstrDefExt = TEXT("txt");
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrFileTitle = filetitle;
					ofn.lpstrTitle = TEXT("ログの保存");
					if (GetSaveFileName(&ofn)) {
						File f;
						f.open(filename, MODE_W);
						f.write((void*)param->log->c_str(), param->log->size());
						f.close();
					}
					SetCurrentDirectory(curdir);
					return TRUE;
				}
				case IDOK:
				case IDCANCEL:
					EndDialog(hwnd, wp);
					return TRUE;
			}
			break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	AddUPnPInfoMessage
//	
// ---------------------------------------------------------------------------
static void
AddUPnPInfoMessage(const char *msg)
{
	if (hwndUPnPDlg) {
		HWND hwndEdit = GD(hwndUPnPDlg,IDC_INFO);

		wstring str;
		ToUnicode(L"shift_jis", msg, strlen(msg), str);

		DWORD i = Edit_GetTextLength(hwndEdit);
		Edit_SetSel(hwndEdit, i, i);
		SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)str.c_str());

		i = Edit_GetTextLength(hwndEdit);
		Edit_SetSel(hwndEdit, i, i);
		SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
	}
}




// ---------------------------------------------------------------------------
//	UPnP_PortMappingTestThread
//	
// ---------------------------------------------------------------------------
static uint WINAPI
UPnP_PortMappingTestThread(void *data)
{
	CoInitialize(NULL);

	UPnP_PortMappingTest(data);
	PostMessage(hwndUPnPDlg, UM_UPNP_END_TEST, 0, 0);

	CoUninitialize();

	CloseHandle(UPnPThreadHandle);
	UPnPThreadHandle = NULL;
	//_endthreadex(0);
	return (0);
}




// ---------------------------------------------------------------------------
//	UPnP_PortMappingTest
//	
// ---------------------------------------------------------------------------
static void
UPnP_PortMappingTest(void* data)
{
	UPnPTestParam *testparam = (UPnPTestParam*)data;

	UPnP upnp;
	upnp.SetMessageHandler(AddUPnPInfoMessage);
	upnp.SetLogBuffer(&testparam->log);

	UPnPObjectList &objects = testparam->objects;
	objects.clear();
	if (!UPnPLoop || !upnp.SearchIGDs(testparam->objects))
		return;
	if (!UPnPLoop || !upnp.GetDeviceDescriptions(testparam->objects))
		return;

	UPnPServiceList WANConnSvcs;
	for (size_t i = 0; i < objects.size(); i++)
		objects[i].GetServicesByType("urn:schemas-upnp-org:service:WANPPPConnection:1", WANConnSvcs);
	for (size_t i = 0; i < objects.size(); i++)
		objects[i].GetServicesByType("urn:schemas-upnp-org:service:WANIPConnection:1", WANConnSvcs);
	if (WANConnSvcs.empty()) {
		AddUPnPInfoMessage("ERROR: 有効なサービスが存在しません");
		return;
	}

	if (!UPnPLoop || !upnp.GetServiceDescriptions(WANConnSvcs))
		return;
/*
	size_t count = 0;
	for (size_t i = 0; i < WANConnSvcs.size(); i++) {
		UPnPAction *action = WANConnSvcs[i].getAction("GetExternalIPAddress");
		if (action)
			count++;
	}
	if (count == 0) {
		AddUPnPInfoMessage("ERROR: 有効なActionが存在しません");
		return;
	}
*/
	if (UPnPLoop)
		upnp.DoServiceActions(WANConnSvcs, "GetExternalIPAddress");

	UPnPServiceList &validServices = testparam->validServices;
	validServices.clear();
	for (size_t i = 0; i < WANConnSvcs.size() && UPnPLoop; i++) {
		string ipstr = "(グローバルIP不明)";
		UPnPAction *action = WANConnSvcs[i].getAction("GetExternalIPAddress");
		if (action) {
			UPnPArgument *NewExternalIPAddress = action->getArgument("NewExternalIPAddress");
			if (NewExternalIPAddress && !NewExternalIPAddress->value.empty()) {
				ipstr = NewExternalIPAddress->value;
			}
		}
		string msg;
		msg = "テスト => ";
		msg += ipstr;
		AddUPnPInfoMessage(msg.c_str());

		ulong2ipstr(testparam->ip, ipstr);
		char portstr[16];
		sprintf_s(portstr, "%u", testparam->port);

		action = WANConnSvcs[i].getAction("AddPortMapping");
		if (action && UPnPLoop) {
			action->clearArgumentValue();
			action->setArgumentValue("NewEnabled", "1");
			action->setArgumentValue("NewProtocol", "TCP");
			action->setArgumentValue("NewRemoteHost", "");
			action->setArgumentValue("NewInternalClient", ipstr.c_str());
			action->setArgumentValue("NewExternalPort", portstr);
			action->setArgumentValue("NewInternalPort", portstr);
			action->setArgumentValue("NewPortMappingDescription", "o2on UPnP");
			action->setArgumentValue("NewLeaseDuration", "0");
			if (upnp.DoServiceAction(WANConnSvcs[i], "AddPortMapping"))
				AddUPnPInfoMessage("やりました！ポート開放に成功しました！");
			else
				continue;
		}
		action = WANConnSvcs[i].getAction("DeletePortMapping");
		if (action && UPnPLoop) {
			action->clearArgumentValue();
			action->setArgumentValue("NewRemoteHost", "");
			action->setArgumentValue("NewExternalPort", portstr);
			action->setArgumentValue("NewProtocol", "TCP");
			if (upnp.DoServiceAction(WANConnSvcs[i], "DeletePortMapping"))
				AddUPnPInfoMessage("やりました！ポート閉じるのに成功しました！");
			else
				continue;
		}
		validServices.push_back(WANConnSvcs[i]);
	}

	if (validServices.empty()) {
		AddUPnPInfoMessage("\r\n-- 利用可能なUPnPサービスが見つかりませんでした");
	}
	else {
		char tmp[64];
		sprintf_s(tmp, 64, "\r\n-- 利用可能なUPnPサービスが%d件見つかりました", validServices.size());
		AddUPnPInfoMessage(tmp);
	}
}




// ---------------------------------------------------------------------------
//	UPnP_AddPortMapping
//	
// ---------------------------------------------------------------------------
static bool
UPnP_AddPortMapping(ushort port, const char *adapterName, const char *location, const char *serviceId, string &log)
{
	if (!adapterName || strlen(adapterName) == 0) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"adapterNameが指定されていません");
		return false;
	}
	if (!location || strlen(location) == 0) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"locationが指定されていません");
		return false;
	}
	if (!serviceId || strlen(serviceId) == 0) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"serviceIdが指定されていません");
		return false;
	}

	IP_ADAPTER_INFO *info = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
	ULONG buffsize = sizeof(IP_ADAPTER_INFO);
	if (GetAdaptersInfo(info, &buffsize) != ERROR_SUCCESS) {
		free(info);
		info = (IP_ADAPTER_INFO*)malloc(buffsize);
	}
	if (GetAdaptersInfo(info, &buffsize) != ERROR_SUCCESS) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"ネットワークアダプタの情報取得に失敗しました");
		free(info);
		return false;
	}
	IP_ADAPTER_INFO *p = NULL;
	for (p = info; p != NULL; p = p->Next) {
		if (strcmp(p->AdapterName, adapterName) == 0)
			break;
	}
	if (p == NULL) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"指定されたネットワークアダプタが見つかりません(%s)", adapterName);
		free(info);
		return false;
	}
	string ipstr = p->IpAddressList.IpAddress.String;
	free(info);
	if (inet_addr(ipstr.c_str()) == 0) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"指定されたネットワークアダプタのIPアドレスが0です");
		return false;
	}

	UPnP upnp;
	upnp.SetMessageHandler(NULL);
	upnp.SetLogBuffer(&log);

	UPnPObject object;
	object.location = location;
	if (!upnp.GetDeviceDescription(object)) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"デバイス情報取得失敗(%s)", location);
		return false;
	}

	UPnPService service;
	if (!object.GetServicesById(serviceId, service)) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"サービスが見つかりません(%s)", serviceId);
		return false;
	}

	if (!upnp.GetServiceDescription(service)) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"サービス情報取得失敗(%s)", service.SCPDURL.c_str());
		return false;
	}

	UPnPAction *action = service.getAction("AddPortMapping");
	if (!action) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"アクションAddPortMappingが見つかりません(%s)", service.serviceId.c_str());
		return false;
	}

	char portstr[16];
	sprintf_s(portstr, "%u", port);

	action->clearArgumentValue();
	action->setArgumentValue("NewEnabled", "1");
	action->setArgumentValue("NewProtocol", "TCP");
	action->setArgumentValue("NewRemoteHost", "");
	action->setArgumentValue("NewInternalClient", ipstr.c_str());
	action->setArgumentValue("NewExternalPort", portstr);
	action->setArgumentValue("NewInternalPort", portstr);
	action->setArgumentValue("NewPortMappingDescription", "o2on UPnP");
	action->setArgumentValue("NewLeaseDuration", "0");
	if (!upnp.DoServiceAction(service, "AddPortMapping")) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0, "AddPortMapping失敗(%s)",portstr);
		return false;
	}

	Logger->AddLog(O2LT_INFO, L"UPnP", 0, 0, "ポートを開放しました(%s)",portstr);
	if (!UPnPServiceUsingForPortMapping)
		delete UPnPServiceUsingForPortMapping;
	UPnPServiceUsingForPortMapping = new UPnPService(service);
	return true;
}




// ---------------------------------------------------------------------------
//	UPnP_DeletePortMapping
//	
// ---------------------------------------------------------------------------
static bool
UPnP_DeletePortMapping(string &log)
{
	if (!UPnPServiceUsingForPortMapping)
		return false;
	UPnPService *service = UPnPServiceUsingForPortMapping;

	UPnPAction *action = service->getAction("AddPortMapping");
	ushort port = 0;

	if (action) {
		char *portstr = action->getArgumentValue("NewExternalPort");
		if (portstr) {
			port = (ushort)strtoul(portstr, NULL, 10);
		}
	}

	if (port == 0) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"前回のAddPortMapping情報が見つかりませんでした(%s)",
			service->serviceId.c_str());
		delete UPnPServiceUsingForPortMapping;
		UPnPServiceUsingForPortMapping = NULL;
		return false;
	}

	action = service->getAction("DeletePortMapping");
	if (!action) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0,
			"DeletePortMappingを利用できません(%s)",
			service->serviceId.c_str());
		delete UPnPServiceUsingForPortMapping;
		UPnPServiceUsingForPortMapping = NULL;
		return false;
	}

	char portstr[16];
	sprintf_s(portstr, "%u", port);

	UPnP upnp;
	upnp.SetMessageHandler(NULL);
	upnp.SetLogBuffer(&log);

	action->clearArgumentValue();
	action->setArgumentValue("NewRemoteHost", "");
	action->setArgumentValue("NewExternalPort", portstr);
	action->setArgumentValue("NewProtocol", "TCP");
	if (!upnp.DoServiceAction(*service, "DeletePortMapping")) {
		Logger->AddLog(O2LT_ERROR, L"UPnP", 0, 0, "DeletePortMapping失敗(%s)",portstr);
		delete UPnPServiceUsingForPortMapping;
		UPnPServiceUsingForPortMapping = NULL;
		return false;
	}

	Logger->AddLog(O2LT_INFO, L"UPnP", 0, 0, "ポートを閉じました(%s)",portstr);
	delete UPnPServiceUsingForPortMapping;
	UPnPServiceUsingForPortMapping = NULL;
	return true;
}




// ---------------------------------------------------------------------------
//	CreateProgressDialog
//	プログレスバーダイアログ作成（モードレス）
// ---------------------------------------------------------------------------
void
CreateProgressDialog(TCHAR *title)
{
	ProgressInfo.Reset(true, false);
	hwndProgressDlg = CreateDialogParam(
		instance,
		MAKEINTRESOURCE(IDD_PROGRESS),
		NULL,
		ProgressDlgProc,
		(LPARAM)title);
}




// ---------------------------------------------------------------------------
//	ProgressDlgProc
//	プログレスバーダイアログのプロシージャ
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK
ProgressDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static HWND hProgress;
	static HWND hInfo;
	static UINT timerID;

	switch (msg) {
		case WM_INITDIALOG:
			hProgress = GD(hwnd, IDC_PROGRESS);
			hInfo = GD(hwnd, IDC_INFO);
			SendMessage(hProgress, PBM_SETRANGE32, 0, 0);
			SetWindowText(hwnd, (TCHAR*)lp);
			SetWindowPosToCorner(hwnd);
			timerID = SetTimer(hwnd, IDT_PROGRESSTIMER, 500, NULL);
			return TRUE;

		case WM_LBUTTONDOWN:
	        ReleaseCapture();
		    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
			return TRUE;

		case WM_CLOSE:
			KillTimer(hwnd, timerID);
			DestroyWindow(hwnd);
			hwndProgressDlg = NULL;
			return TRUE;

		case WM_TIMER:
			switch (wp) {
			case IDT_PROGRESSTIMER:
				ProgressInfo.Lock();
				{
					if (ProgressInfo.active == false)
						PostMessage(hwnd, WM_CLOSE, 0, 0);
					if (!IsWindowEnabled(GD(hwnd,IDC_CANCEL)) && ProgressInfo.stoppable)
						EnableWindow(GD(hwnd,IDC_CANCEL), TRUE);
					SendMessage(hProgress, PBM_SETPOS, (WPARAM)ProgressInfo.pos, 0);
					SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM)ProgressInfo.maxpos);
					SetWindowText(hInfo, ProgressInfo.message.c_str());
				}
				ProgressInfo.Unlock();
				break;
			}
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_MINIMIZED:
					ShowWindow(hwnd, SW_SHOWMINIMIZED);
					return TRUE;
				case IDC_CANCEL:
					DatIO->StopRebuildDB();
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					return TRUE;
			}
			break;
	}
	return FALSE;
}




// ---------------------------------------------------------------------------
//	DlgNumSet
//	数字入力欄に数値をセット
// ---------------------------------------------------------------------------
static void
DlgNumSet(HWND hwnd, uint id, uint64 n, uint64 max)
{
	TCHAR tmp[64];
	HWND hItem = GD(hwnd, id);

	_stprintf_s(tmp, 64, _T("%I64u"), max);
	Edit_LimitText(hItem, _tcslen(tmp));

	_stprintf_s(tmp, 64, _T("%I64u"), n);
	Edit_SetText(hItem, tmp);
}




// ---------------------------------------------------------------------------
//	DlgNumCheck
//	数字入力欄の数値をチェック
// ---------------------------------------------------------------------------
static bool
DlgNumCheck(HWND hwnd, uint id, uint pageNo, TCHAR *name, uint64 min, uint64 max, uint64 &n)
{
	TCHAR tmp[64];

	HWND hItem = GD(hwnd, id);
	Edit_GetText(hItem, tmp, 64);

	n = _tcstoul(tmp, NULL, 10);
	if (n < min || n > max) {
		_stprintf_s(tmp, 64, _T("%sは%I64u～%I64uの範囲で入力してください"), name, min, max);
		MessageBox(hwnd, tmp, NULL, MB_OK|MB_ICONERROR);
		SetFocus(hItem);
		SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
		PostMessage(GetParent(hwnd), PSM_SETCURSEL, pageNo, (LPARAM)hwnd);
		return false;
	}
	return true;
}




// ---------------------------------------------------------------------------
//	IsChecked
//	チェックボックスの状態を返す
// ---------------------------------------------------------------------------
static bool
IsChecked(HWND hwnd)
{
	return (Button_GetCheck(hwnd) == 0 ? false : true);
}



// ---------------------------------------------------------------------------
//	AddTrayIcon
//	トレイアイコンを追加
// ---------------------------------------------------------------------------
static void
AddTrayIcon(UINT id)
{
	HICON icon = (HICON)LoadImage(
		instance,
		MAKEINTRESOURCE(id), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON), 0);

	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwndMain;
	nid.uID = IDT_TRAYICON;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = UM_TRAYICON;
	nid.hIcon = icon;
	MakeTrayIconTipString(&nid);

	Shell_NotifyIcon(NIM_ADD, &nid);

	if (icon) 
		DestroyIcon(icon); 
}


// ---------------------------------------------------------------------------
//	ChangeTrayIcon
//	トレイアイコンを変更
// ---------------------------------------------------------------------------
static void
ChangeTrayIcon(UINT id)
{
	if (time(NULL) - Server_P2P->GetLastAcceptTime() < (5*60)) {
		switch (id) {
			case IDI_A:			id = IDI_B;			break;
			case IDI_A_IN:		id = IDI_B_IN;		break;
			case IDI_A_OUT:		id = IDI_B_OUT;		break;
			case IDI_A_INOUT:	id = IDI_B_INOUT;	break;
		}
	}

	HICON icon = (HICON)LoadImage(
		instance,
		MAKEINTRESOURCE(id), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON), 0);

	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwndMain;
	nid.uID = IDT_TRAYICON;
	nid.uFlags = NIF_ICON | NIF_TIP;
	nid.hIcon = icon;
	MakeTrayIconTipString(&nid);

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	if (icon) 
		DestroyIcon(icon); 
}


// ---------------------------------------------------------------------------
//	DeleteTrayIcon
//	トレイアイコンを削除
// ---------------------------------------------------------------------------
static void
DeleteTrayIcon(void)
{
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwndMain;
	nid.uID = IDT_TRAYICON;

	Shell_NotifyIcon(NIM_DELETE, &nid);
}


// ---------------------------------------------------------------------------
//	ShowTrayBaloon
//	トレイアイコンのバルーン表示
// ---------------------------------------------------------------------------
static void
ShowTrayBaloon(const TCHAR *title, const TCHAR *msg, UINT timeout, DWORD infoflag)
{
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwndMain;
	nid.uID = IDT_TRAYICON;
	nid.uFlags = NIF_INFO;

	nid.uTimeout = timeout;
	_tcscpy_s(nid.szInfo, 256, msg ? msg : _T(""));
	_tcscpy_s(nid.szInfoTitle, 64, title ? title : _T(""));
	nid.dwInfoFlags = infoflag;
	
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}




// ---------------------------------------------------------------------------
//	MakeTrayIconTipString
//	トレイアイコンのツールチップ文字列
// ---------------------------------------------------------------------------
static void
MakeTrayIconTipString(NOTIFYICONDATA *nid)
{
	_stprintf_s(nid->szTip, 64, _T(APP_VER_FORMAT)_T("\nPort: %d"),
		_T(APP_NAME), APP_VER_MAJOR, APP_VER_MINOR,
		_T(APP_VER_PREFIX), APP_BUILDNO,
		Profile->GetP2PPort());
}




// ---------------------------------------------------------------------------
//	StartProxy
//	Proxy開始
// ---------------------------------------------------------------------------
static bool
StartProxy(wchar_t *addmsg)
{
	if (!Server_Proxy->Start()) {
		wstring reason;
		Logger->GetLogMessage(LOGGER_LOG, L"ProxyServer", reason);

		wchar_t msg[256];
		swprintf_s(msg, 256, L"Proxy起動失敗:\n%s%s",
			reason.c_str(), addmsg ? addmsg : L"");

		MessageBoxW(hwndMain, msg, NULL, MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}




// ---------------------------------------------------------------------------
//	StartAdmin
//	Admin開始
// ---------------------------------------------------------------------------
static bool
StartAdmin(wchar_t *addmsg)
{
	if (!Server_Admin->Start()) {
		wstring reason;
		Logger->GetLogMessage(LOGGER_LOG, L"AdminServer", reason);

		wchar_t msg[256];
		swprintf_s(msg, 256, L"Admin起動失敗:\n%s%s",
			reason.c_str(), addmsg ? addmsg : L"");

		MessageBoxW(hwndMain, msg, NULL, MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}
	
	
	

// ---------------------------------------------------------------------------
//	StartP2P
//	P2P開始
// ---------------------------------------------------------------------------
static bool
StartP2P(bool baloon)
{
	if (!CheckPort()) {
		MessageBoxW(hwndMain, L"ポートが設定されていません",
							L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	//
	// UPnP - AddPortMapping
	//
	if (Profile->UsingUPnP() && !Profile->IsPort0()) {
		string adapter = Profile->GetUPnPAdapterName();
		string location = Profile->GetUPnPLocation();
		string serviceId = Profile->GetUPnPServiceId();
		if (adapter.empty()) {
			MessageBoxW(hwndMain,
				L"UPnPに必要な設定が不足しています(UPnPAdapterName)",
				NULL, MB_OK | MB_ICONERROR);
			return false;
		}
		if (location.empty()) {
			MessageBoxW(hwndMain,
				L"UPnPに必要な設定が不足しています(UPnPLocation)",
				NULL, MB_OK | MB_ICONERROR);
			return false;
		}
		if (serviceId.empty()) {
			MessageBoxW(hwndMain,
				L"UPnPに必要な設定が不足しています(UPnPServiceId)",
				NULL, MB_OK | MB_ICONERROR);
			return false;
		}
		string log;
		if (!UPnP_AddPortMapping(Profile->GetP2PPort(), adapter.c_str(), location.c_str(), serviceId.c_str(), log)) {
			UPnPErrorDlgParam param;
			Logger->GetLogMessage(O2LT_ERROR, L"UPnP", param.caption);
			param.log = &log;
			DialogBoxParam(
				instance,
				MAKEINTRESOURCE(IDD_UPNP_ERROR),
				hwndMain,
				UPnPErrorDlgProc,
				(LPARAM)&param);
			return false;
		}
	}

	DatIO->SetEmergencyHaltCallbackMsg(hwndMain, UM_EMERGENCYHALT);
	//
	//	P2P開始
	//
	Server_P2P->SetPort(Profile->IsPort0() ? 0 : Profile->GetP2PPort());
	Server_P2P->SetSessionLimit(Profile->GetP2PSessionLimit());
	if (!Profile->IsPort0()) {
		if (!Server_P2P->Start()) {
			wstring reason;
			Logger->GetLogMessage(LOGGER_LOG, L"P2PServer", reason);

			wchar_t msg[256];
			swprintf_s(msg, 256,
				L"P2P起動失敗:\n%s",
				reason.c_str());

			MessageBoxW(hwndMain, msg, NULL, MB_OK | MB_ICONERROR);
			return false;
		}
	}

	PerformanceCounter->Start();
	Agent->SchedulerStart();

	ChangeTrayIcon(IDI_A);
	if (baloon && Profile->IsBaloon_P2P())
		ShowTrayBaloon(L"o2on", L"P2Pが起動しました", 3*1000, NIIF_INFO);

	Active = true;

	CLEAR_WORKSET;
	return true;
}




// ---------------------------------------------------------------------------
//	StopP2P
//	P2P停止
// ---------------------------------------------------------------------------
static bool
StopP2P(bool baloon)
{
	uint64 total_send = Server_P2P->GetSendByte()
					  + Agent->GetSendByte();

	uint64 total_recv = Server_P2P->GetRecvByte()
					  + Agent->GetRecvByte();
	
	if (Server_P2P->IsActive())
		Server_P2P->Stop();
	Server_P2P->ResetCounter();

	Agent->SchedulerStop();
	Agent->ResetCounter();
	PerformanceCounter->Stop(total_send, total_recv);
	PerformanceCounter->Save(Profile->GetReportFilePath());

	ChangeTrayIcon(IDI_DISABLE);
	if (baloon && Profile->IsBaloon_P2P())
		ShowTrayBaloon(L"o2on", L"o2onが停止しました", 3*1000, NIIF_INFO);

	DatIO->SetEmergencyHaltCallbackMsg(NULL, NULL);

	Active = false;

	//
	// UPnP - DeletePortMapping
	//
	if (UPnPServiceUsingForPortMapping) {
		string log;
		if (!UPnP_DeletePortMapping(log)) {
			UPnPErrorDlgParam param;
			Logger->GetLogMessage(O2LT_ERROR, L"UPnP", param.caption);
			param.log = &log;
			/*
			DialogBoxParam(
				instance,
				MAKEINTRESOURCE(IDD_UPNP_ERROR),
				hwndMain,
				UPnPErrorDlgProc,
				(LPARAM)&param);
			*/
			return false;
		}
	}

	CLEAR_WORKSET;
	return true;
}




// ---------------------------------------------------------------------------
//	CheckPort
//	ポート設定の妥当性チェック
// ---------------------------------------------------------------------------
static bool
CheckPort(void)
{
	if ((Profile->IsPort0() || Profile->GetP2PPort())
			&& Profile->GetProxyPort()
			&& Profile->GetAdminPort()) {
		return true;
	}
	return false;
}




// ---------------------------------------------------------------------------
//	OpenBrowser
//	ブラウザを開く
// ---------------------------------------------------------------------------
static void
OpenBrowser(const wstring &type, const wstring &path)
{
	TCHAR url[MAX_PATH];
	_stprintf_s(url, MAX_PATH, L"http://127.0.0.1:%d/", Profile->GetAdminPort());

	if (type == L"default") {
		ShellExecuteW(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
	}
	else if (type == L"internal") {
		wstring ipath;
		GetInternalBrowserPath(ipath);
		ShellExecuteW(NULL, NULL, ipath.c_str(), url, NULL, SW_SHOWNORMAL);
	}
	else if (type == L"custom") {
		if (_waccess(path.c_str(), 0) == 0)
			ShellExecuteW(NULL, NULL, path.c_str(), url, NULL, SW_SHOWNORMAL);
		else
			MessageBox(NULL, L"ファイルが見つかりません", NULL, MB_ICONERROR|MB_OK);
	}
}




// ---------------------------------------------------------------------------
//	GetInternalBrowserPath
//	内蔵ブラウザのパスを取得
// ---------------------------------------------------------------------------
#define BROWSER_FILENAME "browser.exe"
static void
GetInternalBrowserPath(tstring &path)
{
	TCHAR module_dir[_MAX_PATH];
	GetModuleDirectory(module_dir);
	path = module_dir;
	path += _T("\\");
	path += _T(BROWSER_FILENAME);
}




// ---------------------------------------------------------------------------
//	SetWindowPosAuto
//	ウィンドウ位置最適化
// ---------------------------------------------------------------------------
static void
SetWindowPosAuto(HWND hwnd)
{
	POINT	p;
	POINT	center;
	POINT	newpos;
	RECT	r;
	RECT	w;

	GetCursorPos(&p);
	GetWindowRect(hwnd, &r);
	SystemParametersInfo(SPI_GETWORKAREA, 0, (void*)&w, 0);

	center.x = (w.right - w.left)/2 + w.left;
	center.y = (w.bottom - w.top)/2 + w.top;

	if (p.x < center.x)
		newpos.x = w.left;
	else
		newpos.x = w.right - (r.right - r.left);

	if (p.y < center.y)
		newpos.y = w.top;
	else
		newpos.y = w.bottom - (r.bottom - r.top);

	SetWindowPos(hwnd, NULL, newpos.x, newpos.y, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}




// ---------------------------------------------------------------------------
//	SetWindowPosToCorner
//	ウィンドウ位置をトレイウィンドウ近くの画面隅に
// ---------------------------------------------------------------------------
static void
SetWindowPosToCorner(HWND hwnd)
{
	POINT	p;
	POINT	center;
	POINT	newpos;
	RECT	r;
	RECT	w;

	p.x = LONG_MAX;
	p.y = LONG_MAX;
	HWND h;
	if ((h = FindWindow(_T("Shell_TrayWnd"), NULL)) != NULL) {
		if ((h = FindWindowEx(h, NULL, _T("TrayNotifyWnd"), NULL)) != NULL) {
			GetWindowRect(h, &r);
			p.x = r.left;
			p.y = r.bottom;
		}
	}

	GetWindowRect(hwnd, &r);
	SystemParametersInfo(SPI_GETWORKAREA, 0, (void*)&w, 0);

	center.x = (w.right - w.left)/2 + w.left;
	center.y = (w.bottom - w.top)/2 + w.top;

	if (p.x < center.x)
		newpos.x = w.left;
	else
		newpos.x = w.right - (r.right - r.left);

	if (p.y < center.y)
		newpos.y = w.top;
	else
		newpos.y = w.bottom - (r.bottom - r.top);

	SetWindowPos(hwnd, NULL, newpos.x, newpos.y, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}




// ---------------------------------------------------------------------------
//	ChangeToModuleDir
//	カレントディレクトリをexeが置かれているディレクトリに変更
// ---------------------------------------------------------------------------
static void
ChangeToModuleDir(void)
{
	TCHAR module_dir[_MAX_PATH];
	GetModuleDirectory(module_dir);
	SetCurrentDirectory(module_dir);
}




// ---------------------------------------------------------------------------
//	GetModuleDirectory
//	exeが置かれているディレクトリを取得
// ---------------------------------------------------------------------------
static void
GetModuleDirectory(TCHAR *module_dir)
{
	TCHAR path[_MAX_PATH];
	TCHAR drive[_MAX_DRIVE];
	TCHAR dir[_MAX_DIR];

	GetModuleFileName(NULL, path, _MAX_PATH);
	_tsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);
	_stprintf_s(module_dir, _MAX_PATH, _T("%s%s"), drive, dir);
}




// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
#if defined(_DEBUG)
void dbdump(O2DatDB *DatDB)
{
	O2DatRecList reclist;
	DatDB->select(reclist, L"2ch.net", L"bbynamazu");

	FILE *fp;
	fopen_s(&fp, "dbdump.txt", "wb");

	wchar_t buff[10000];
	wstring tmpstr;
	for (O2DatRecListIt it = reclist.begin(); it != reclist.end(); it++) {
		it->hash.to_string(tmpstr);
		swprintf_s(buff, 10000, L"%s %s %s %s %I64u %I64u %s %s %I64u %d %d\r\n",
			tmpstr.c_str(),
			it->domain.c_str(),
			it->bbsname.c_str(),
			it->datname.c_str(), 
			it->size, 
			it->disksize, 
			it->url.c_str(), 
			it->title.c_str(), 
			it->res,
			it->lastupdate,
			it->lastpublish);
		fwrite(buff, sizeof(wchar_t), wcslen(buff), fp);
	}
	fclose(fp);
}
#endif
