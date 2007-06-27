/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Profile.cpp
 * description	: 
 *
 */

#include <tchar.h>
#include "O2Profile.h"
#include "O2Version.h"
#include "file.h"
#include "dataconv.h"
#include "../cryptopp/rsa.h"
#include "../cryptopp/randpool.h"

#define MODULE						L"Profile"
#define	DEFAULT_CONF_DIR			L".\\conf"
#define	DEFAULT_DB_DIR				L".\\db"
#define	DEFAULT_CACHE_DIR			L".\\dat"
#define	DEFAULT_ADMIN_DIR			L".\\admin"
#define PROFILE_FILENAME			L"Profile.xml"
#define NODE_FILENAME				L"Node.xml"
#define FRIEND_FILENAME				L"Friend.xml"
#define QUERY_FILENAME				L"Query.xml"
#define SAKU_FILENAME				L"Saku.xml"
#define IM_FILENAME					L"IM.xml"
#define REPORT_FILENAME				L"Report.xml"
#define IPF_P2P_FILENAME			L"IPFilter_P2P.xml"
#define IPF_PROXY_FILENAME			L"IPFilter_Proxy.xml"
#define IPF_ADMIN_FILENAME			L"IPFilter_Admin.xml"

#define DEFAULT_P2P_PORT			0
#define DEFAULT_PROXY_PORT			8000
#define DEFAULT_ADMIN_PORT			9999

#define DEFAULT_P2P_SESSION_LIMIT	15

//#define DEFAULT_NODE_LIMIT			300
#define DEFAULT_KEY_LIMIT			3000
#define DEFAULT_QUERY_LIMIT			1000

#define DEFAULT_LOG_LIMIT			1000
#define DEFAULT_NET_LOG_LIMIT		1000
#define DEFAULT_HOKAN_LOG_LIMIT		1000
#define DEFAULT_IPF_LOG_LIMIT		1000




O2Profile::
O2Profile(O2Logger *lgr, bool load)
{
	Logger = lgr;

	SetID(NULL, 0);
	SetRSAKey(NULL, 0, NULL, 0);

	IP					= 0;
	P2PPort				= DEFAULT_P2P_PORT;
	ProxyPort			= DEFAULT_PROXY_PORT;
	AdminPort			= DEFAULT_ADMIN_PORT;
	NodeNameA			= "";
	NodeNameW			= L"";
	Comment				= L"";

	SetConfDir(DEFAULT_CONF_DIR);
	SetDBDir(DEFAULT_DB_DIR);
	SetCacheRoot(DEFAULT_CACHE_DIR);
	SetAdminRoot(DEFAULT_ADMIN_DIR);

	ProfileFilePath		= ConfDirW + L"\\" + PROFILE_FILENAME;
	NodeFilePath		= ConfDirW + L"\\" + NODE_FILENAME;
	FriendFilePath		= ConfDirW + L"\\" + FRIEND_FILENAME;
	QueryFilePath		= ConfDirW + L"\\" + QUERY_FILENAME;
	SakuFilePath		= ConfDirW + L"\\" + SAKU_FILENAME;
	IMFilePath			= ConfDirW + L"\\" + IM_FILENAME;
	ReportFilePath		= ConfDirW + L"\\" + REPORT_FILENAME;
	IPF_P2PFilePath		= ConfDirW + L"\\" + IPF_P2P_FILENAME;
	IPF_ProxyFilePath	= ConfDirW + L"\\" + IPF_PROXY_FILENAME;
	IPF_AdminFilePath	= ConfDirW + L"\\" + IPF_ADMIN_FILENAME;

	P2PSessionLimit		= DEFAULT_P2P_SESSION_LIMIT;

	Port0				= false;
	P2PAutoStart		= true;
	LoadSubdirIndex		= false;
	MaruUser			= false;
	PublicReport		= false;
	PublicRecentDat		= false;

//	NodeLimit			= DEFAULT_NODE_LIMIT;
	KeyLimit			= DEFAULT_KEY_LIMIT;
	QueryLimit			= DEFAULT_QUERY_LIMIT;

	LogLimit			= DEFAULT_LOG_LIMIT;
	NetLogLimit			= DEFAULT_NET_LOG_LIMIT;
	HokanLogLimit		= DEFAULT_HOKAN_LOG_LIMIT;
	IPFLogLimit			= DEFAULT_IPF_LOG_LIMIT;

	Baloon_P2P			= true;
	Baloon_Query		= true;
	Baloon_Hokan		= true;
	Baloon_IM			= true;

	AutoResume			= true;
	ResumeDelayMs		= 30000;

	QuarterSize 		= 0;
	QuarterFullOperation= 0;

	AdminBrowserType	= L"default";
	AdminBrowserPath	= L"";

	UseUPnP				= false;
	UPnPAdapterName		= "";
	UPnPLocation		= "";
	UPnPServiceId		= "";

	DatStorageFlag		= false;

	char tmpA[64];
	sprintf_s(tmpA, 64, USERAGENT_FORMAT, 
		PROTOCOL_NAME, PROTOCOL_VER,
		APP_NAME, APP_VER_MAJOR, APP_VER_MINOR, APP_BUILDNO,
		O2_PLATFORM);
	UserAgentA = tmpA;

	wchar_t tmpW[64];
	swprintf_s(tmpW, 64, _T(USERAGENT_FORMAT),
		_T(PROTOCOL_NAME), PROTOCOL_VER,
		_T(APP_NAME), APP_VER_MAJOR, APP_VER_MINOR, APP_BUILDNO,
		_T(O2_PLATFORM));
	UserAgentW = tmpW;

	if (load)
		Load();
}




O2Profile::
~O2Profile()
{
}




void
O2Profile::
assign(O2Profile &src)
{
	Lock();
	src.Lock();

	ID					= src.ID;
	PrivKey				= src.PrivKey;
	PubKey				= src.PubKey;

	IP					= src.IP;
	P2PPort				= src.P2PPort;		
	ProxyPort			= src.ProxyPort;
	AdminPort			= src.AdminPort;
	NodeNameA			= src.NodeNameA;
	NodeNameW			= src.NodeNameW;
	Comment				= src.Comment;

	UserAgentA			= src.UserAgentA;
	UserAgentW			= src.UserAgentW;
	ConfDirA			= src.ConfDirA;
	ConfDirW			= src.ConfDirW;
	DBDirA				= src.DBDirA;
	DBDirW				= src.DBDirW;
	CacheRootA			= src.CacheRootA;
	CacheRootW			= src.CacheRootW;
	AdminRootA			= src.AdminRootA;
	AdminRootW			= src.AdminRootW;

	ProfileFilePath		= src.ProfileFilePath;
	NodeFilePath		= src.NodeFilePath;
	FriendFilePath		= src.FriendFilePath;
	QueryFilePath		= src.QueryFilePath;
	SakuFilePath		= src.SakuFilePath;
	IMFilePath			= src.IMFilePath;
	ReportFilePath		= src.ReportFilePath;
	IPF_P2PFilePath		= src.IPF_P2PFilePath;
	IPF_ProxyFilePath	= src.IPF_ProxyFilePath;
	IPF_AdminFilePath	= src.IPF_AdminFilePath;

	P2PSessionLimit		= src.P2PSessionLimit;

	Port0				= src.Port0;
	P2PAutoStart		= src.P2PAutoStart;
	LoadSubdirIndex		= src.LoadSubdirIndex;
	MaruUser			= src.MaruUser;
	PublicReport		= src.PublicReport;
	PublicRecentDat		= src.PublicRecentDat;
						  
//	NodeLimit			= src.NodeLimit;
	KeyLimit			= src.KeyLimit;
	QueryLimit			= src.QueryLimit;
						  
	LogLimit			= src.LogLimit;
	NetLogLimit			= src.NetLogLimit;
	HokanLogLimit		= src.HokanLogLimit;
	IPFLogLimit			= src.IPFLogLimit;

	Baloon_P2P			= src.Baloon_P2P;
	Baloon_Query		= src.Baloon_Query;
	Baloon_Hokan		= src.Baloon_Hokan;
	Baloon_IM			= src.Baloon_IM;

	AutoResume			= src.AutoResume;
	ResumeDelayMs		= src.ResumeDelayMs;

	QuarterSize 		= src.QuarterSize;
	QuarterFullOperation= src.QuarterFullOperation;

	AdminBrowserType	= src.AdminBrowserType;
	AdminBrowserPath	= src.AdminBrowserPath;

	UseUPnP				= src.UseUPnP;
	UPnPAdapterName		= src.UPnPAdapterName;
	UPnPLocation		= src.UPnPLocation;
	UPnPServiceId		= src.UPnPServiceId;

	Unlock();
	src.Unlock();
}




bool
O2Profile::
SetIP(ulong globalIP)
{
	IP = globalIP; //big endian
	return true;
}
ulong
O2Profile::
GetIP(void) const
{
	return (IP); //big endian
}
bool
O2Profile::
SetP2PPort(ushort pn)
{
	if (pn == ProxyPort)
		return false;
	if (pn == AdminPort)
		return false;

	P2PPort = pn;
	return true;
}
ushort
O2Profile::
GetP2PPort(void) const
{
	return (P2PPort);
}

bool
O2Profile::
SetProxyPort(ushort pn)
{
	if (pn == P2PPort)
		return false;
	if (pn == AdminPort)
		return false;

	ProxyPort = pn;
	return true;
}
bool
O2Profile::
SetAdminPort(ushort pn)
{
	if (pn == P2PPort)
		return false;
	if (pn == ProxyPort)
		return false;

	AdminPort = pn;
	return true;
}

ushort
O2Profile::
GetProxyPort(void) const
{
	return (ProxyPort);
}
ushort
O2Profile::
GetAdminPort(void) const
{
	return (AdminPort);
}

const char *
O2Profile::
GetNodeNameA(void) const
{
	return (&NodeNameA[0]);
}
const wchar_t *
O2Profile::
GetNodeNameW(void) const
{
	return (&NodeNameW[0]);
}
bool
O2Profile::
SetNodeName(const wchar_t *name)
{
	if (wcslen(name) > O2_MAX_NAME_LEN)
		return false;
	if (wcsstr(name, L"]]>"))
		return false;
	Lock();
	NodeNameW = name;
	FromUnicode(_T(DEFAULT_XML_CHARSET), NodeNameW, NodeNameA);
	Unlock();
	return true;
}


const wchar_t *
O2Profile::
GetComment(void) const
{
	return (&Comment[0]);
}
bool
O2Profile::
SetComment(const wchar_t *comment)
{
	if (wcslen(comment) > O2_MAX_COMMENT_LEN)
		return false;
	if (wcsstr(comment, L"]]>"))
		return false;
	Lock();
	Comment = comment;
	Unlock();
	return true;
}




void
O2Profile::
SetID(const byte *in, size_t len)
{
	if (in && len == HASHSIZE) {
		ID.assign(in, len);
	}
	else {
		ID.random();
	}
}
void
O2Profile::
GetID(hashT &out) const
{
	out = ID;
}




void
O2Profile::
SetRSAKey(const byte *priv, size_t privlen, const byte *pub, size_t publen)
{
	bool valid = false;

	if (priv && privlen && pub && publen) {
		if (privlen == RSA_PRIVKEY_SIZE && publen == RSA_PUBKEY_SIZE) {
			PrivKey.assign(priv, privlen);
			PubKey.assign(pub, publen);
			valid = true;
		}
	}

	if (!valid) {
		GUID guid;
		CoCreateGuid(&guid);

		CryptoPP::RandomPool randpool;
		randpool.Put((byte*)&guid, sizeof(GUID));

		byte tmpPriv[RSA_PRIVKEY_SIZE];
		CryptoPP::RSAES_OAEP_SHA_Decryptor priv(randpool, RSA_KEYLENGTH);
		CryptoPP::ArraySink privArray(tmpPriv, RSA_PRIVKEY_SIZE);
		priv.DEREncode(privArray);
		privArray.MessageEnd();
		PrivKey.assign(tmpPriv, RSA_PRIVKEY_SIZE);

		byte tmpPub[RSA_PUBKEY_SIZE];
		CryptoPP::RSAES_OAEP_SHA_Encryptor pub(priv);
		CryptoPP::ArraySink pubArray(tmpPub, RSA_PUBKEY_SIZE);
		pub.DEREncode(pubArray);
		pubArray.MessageEnd();
		PubKey.assign(tmpPub, RSA_PUBKEY_SIZE);
	}
}
void
O2Profile::
GetPubkeyA(string &out) const
{
	out.erase();
	PubKey.to_string(out);
}
void
O2Profile::
GetPubkeyW(wstring &out) const
{
	out.erase();
	PubKey.to_string(out);
}




/*
 *	GetUserAgentA()
 *	GetUserAgentW()
 *
 */
const char *
O2Profile::
GetUserAgentA(void) const
{
	return (&UserAgentA[0]);
}
const wchar_t *
O2Profile::
GetUserAgentW(void) const
{
	return (&UserAgentW[0]);
}




/*
 *	SetConfDir()
 *	GetConfDirA()
 *	GetConfDirW()
 *	MakeConfDir()
 *
 */
void
O2Profile::
SetConfDir(const wchar_t *path)
{
	ConfDirW = path;
	if (ConfDirW[ConfDirW.size()-1] == L'\\')
		ConfDirW.erase(ConfDirW.size()-1);
	FromUnicode(L"shift_jis", ConfDirW, ConfDirA);
}
const char *
O2Profile::
GetConfDirA(void) const
{
	return (&ConfDirA[0]);
}
const wchar_t *
O2Profile::
GetConfDirW(void) const
{
	return (&ConfDirW[0]);
}
bool
O2Profile::
MakeConfDir(void)
{
	if (_waccess(ConfDirW.c_str(), 0) == -1) {
		if (_wmkdir(ConfDirW.c_str()) == -1)
			return false;
	}
	return true;
}




/*
 *	SetDBDir()
 *	GetDBDirA()
 *	GetDBDirW()
 *	MakeDBDir()
 *
 */
void
O2Profile::
SetDBDir(const wchar_t *path)
{
	DBDirW = path;
	if (DBDirW[DBDirW.size()-1] == L'\\')
		DBDirW.erase(DBDirW.size()-1);
	FromUnicode(L"shift_jis", DBDirW, DBDirA);
}
const char *
O2Profile::
GetDBDirA(void) const
{
	return (&DBDirA[0]);
}
const wchar_t *
O2Profile::
GetDBDirW(void) const
{
	return (&DBDirW[0]);
}
bool
O2Profile::
MakeDBDir(void)
{
	if (_waccess(DBDirW.c_str(), 0) == -1) {
		if (_wmkdir(DBDirW.c_str()) == -1)
			return false;
	}
	return true;
}




/*
 *	SetCacheRoot()
 *	GetCacheRootA()
 *	GetCacheRootW()
 *	MakeCacheRoot()
 *
 */
void
O2Profile::
SetCacheRoot(const wchar_t *path)
{
	CacheRootW = path;
	if (CacheRootW[CacheRootW.size()-1] == L'\\')
		CacheRootW.erase(CacheRootW.size()-1);
	FromUnicode(L"shift_jis", CacheRootW, CacheRootA);

	char abspath[_MAX_PATH];
	_fullpath(abspath, GetCacheRootA(), _MAX_PATH);
	CacheRootFullPathA = "\\\\?\\";
	CacheRootFullPathA += abspath;
}
const char *
O2Profile::
GetCacheRootA(void) const
{
	return (&CacheRootA[0]);
}
const wchar_t *
O2Profile::
GetCacheRootW(void) const
{
	return (&CacheRootW[0]);
}
const char *
O2Profile::
GetCacheRootFullPathA(void) const
{
	return (&CacheRootFullPathA[0]);
}
bool
O2Profile::
MakeCacheRoot(void)
{
	if (_waccess(CacheRootW.c_str(), 0) == -1) {
		if (_wmkdir(CacheRootW.c_str()) == -1)
			return false;
	}
	return true;
}




/*
 *	SetAdminRoot()
 *	GetAdminRootA()
 *	GetAdminRootW()
 *	CheckAdminRoot()
 *
 */
void
O2Profile::
SetAdminRoot(const wchar_t *path)
{
	AdminRootW = path;
	if (AdminRootW[AdminRootW.size()-1] == L'\\')
		AdminRootW.erase(AdminRootW.size()-1);
	FromUnicode(L"shift_jis", AdminRootW, AdminRootA);
}
const char *
O2Profile::
GetAdminRootA(void) const
{
	return (&AdminRootA[0]);
}
const wchar_t *
O2Profile::
GetAdminRootW(void) const
{
	return (&AdminRootW[0]);
}
bool
O2Profile::
CheckAdminRoot(void)
{
	if (_waccess(AdminRootW.c_str(), 0) == -1)
		return false;
	return true;
}




const wchar_t *
O2Profile::
GetProfileFilePath(void) const
{
	return (&ProfileFilePath[0]);
}
const wchar_t *
O2Profile::
GetNodeFilePath(void) const
{
	return (&NodeFilePath[0]);
}
const wchar_t *
O2Profile::
GetFriendFilePath(void) const
{
	return (&FriendFilePath[0]);
}
const wchar_t *
O2Profile::
GetQueryFilePath(void) const
{
	return (&QueryFilePath[0]);
}
const wchar_t *
O2Profile::
GetSakuFilePath(void) const
{
	return (&SakuFilePath[0]);
}
const wchar_t *
O2Profile::
GetIMFilePath(void) const
{
	return (&IMFilePath[0]);
}
const wchar_t *
O2Profile::
GetReportFilePath(void) const
{
	return (&ReportFilePath[0]);
}
const wchar_t *
O2Profile::
GetIPF_P2PFilePath(void) const
{
	return (&IPF_P2PFilePath[0]);
}
const wchar_t *
O2Profile::
GetIPF_ProxyFilePath(void) const
{
	return (&IPF_ProxyFilePath[0]);
}
const wchar_t *
O2Profile::
GetIPF_AdminFilePath(void) const
{
	return (&IPF_AdminFilePath[0]);
}




void
O2Profile::
SetP2PSessionLimit(uint n)
{
	P2PSessionLimit = n;
}
uint
O2Profile::
GetP2PSessionLimit(void) const
{
	return (P2PSessionLimit);
}




bool
O2Profile::
IsPort0(void) const
{
	return (Port0);
}
void
O2Profile::
SetPort0(bool flag)
{
	Port0 = flag;
}
bool
O2Profile::
IsP2PAutoStart(void) const
{
	return (P2PAutoStart);
}
void
O2Profile::
SetP2PAutoStart(bool flag)
{
	P2PAutoStart = flag;
}
bool
O2Profile::
IsLoadSubdirIndex(void) const
{
	return (LoadSubdirIndex);
}
void
O2Profile::
SetLoadSubdirIndex(bool flag)
{
	LoadSubdirIndex = flag;
}
bool
O2Profile::
IsMaruUser(void) const
{
	return (MaruUser);
}
void
O2Profile::
SetMaruUser(bool flag)
{
	MaruUser = flag;
}
bool
O2Profile::
IsPublicReport(void) const
{
	return (PublicReport);
}
void
O2Profile::
SetPublicReport(bool flag)
{
	PublicReport = flag;
}
bool
O2Profile::
IsPublicRecentDat(void) const
{
	return (PublicRecentDat);
}
void
O2Profile::
SetPublicRecentDat(bool flag)
{
	PublicRecentDat = flag;
}
/*
uint
O2Profile::
GetNodeLimit(void) const
{
	return (NodeLimit);
}
void
O2Profile::
SetNodeLimit(uint n)
{
	NodeLimit = n;
}
*/
uint
O2Profile::
GetKeyLimit(void) const
{
	return (KeyLimit);
}
void
O2Profile::
SetKeyLimit(uint n)
{
	KeyLimit = n;
}

uint
O2Profile::
GetQueryLimit(void) const
{
	return (QueryLimit);
}
void
O2Profile::
SetQueryLimit(uint n)
{
	QueryLimit = n;
}

uint
O2Profile::
GetLogLimit(void) const
{
	return (LogLimit);
}
void
O2Profile::
SetLogLimit(uint n)
{
	LogLimit = n;
}
uint
O2Profile::
GetNetLogLimit(void) const
{
	return (NetLogLimit);
}
void
O2Profile::
SetNetLogLimit(uint n)
{
	NetLogLimit = n;
}
uint
O2Profile::
GetHokanLogLimit(void) const
{
	return (HokanLogLimit);
}
void
O2Profile::
SetHokanLogLimit(uint n)
{
	HokanLogLimit = n;
}
uint
O2Profile::
GetIPFLogLimit(void) const
{
	return (IPFLogLimit);
}
void
O2Profile::
SetIPFLogLimit(uint n)
{
	IPFLogLimit = n;
}




bool
O2Profile::
IsBaloon_P2P(void) const
{
	return (Baloon_P2P);
}
bool
O2Profile::
IsBaloon_Query(void) const
{
	return (Baloon_Query);
}
bool
O2Profile::
IsBaloon_Hokan(void) const
{
	return (Baloon_Hokan);
}
bool
O2Profile::
IsBaloon_IM(void) const
{
	return (Baloon_IM);
}
void
O2Profile::
SetBaloon_P2P(bool flag)
{
	Baloon_P2P = flag;
}
void
O2Profile::
SetBaloon_Query(bool flag)
{
	Baloon_Query = flag;
}
void
O2Profile::
SetBaloon_Hokan(bool flag)
{
	Baloon_Hokan = flag;
}
void
O2Profile::
SetBaloon_IM(bool flag)
{
	Baloon_IM = flag;
}




bool
O2Profile::
IsAutoResume(void)
{
	return (AutoResume);
}
void
O2Profile::
SetAutoResume(bool flag)
{
	AutoResume = flag;
}

uint
O2Profile::
GetResumeDelayMs(void)
{
	return (ResumeDelayMs);
}
void
O2Profile::
SetResumeDelayMs(uint ms)
{
	ResumeDelayMs = ms;
}




uint64
O2Profile::
GetQuarterSize(void) const
{
	return (QuarterSize);
}
void
O2Profile::
SetQuarterSize(uint64 size)
{
	QuarterSize = size;
}
uint
O2Profile::
GetQuarterFullOperation(void) const
{
	return (QuarterFullOperation);
}
void
O2Profile::
SetQuarterFullOperation(uint id)
{
	QuarterFullOperation = id;
}




const wchar_t *
O2Profile::
GetAdminBrowserType(void) const
{
	return (AdminBrowserType.c_str());
}
void
O2Profile::
SetAdminBrowserType(const wchar_t *type)
{
	AdminBrowserType = type;
}
const wchar_t *
O2Profile::
GetAdminBrowserPath(void) const
{
	return (AdminBrowserPath.c_str());
}
void
O2Profile::
SetAdminBrowserPath(const wchar_t *path)
{
	AdminBrowserPath = path;
}




bool
O2Profile::
UsingUPnP(void) const
{
	return (UseUPnP);
}
void
O2Profile::
SetUseUPnP(bool flag)
{
	UseUPnP = flag;
}
const char *
O2Profile::
GetUPnPAdapterName(void) const
{
	return (&UPnPAdapterName[0]);
}
void
O2Profile::
SetUPnPAdapterName(const char *adapterName)
{
	UPnPAdapterName = adapterName;
}
const char *
O2Profile::
GetUPnPLocation(void) const
{
	return (&UPnPLocation[0]);
}
void
O2Profile::
SetUPnPLocation(const char *location)
{
	UPnPLocation = location;
}
const char *
O2Profile::
GetUPnPServiceId(void) const
{
	return (&UPnPServiceId[0]);
}
void
O2Profile::
SetUPnPServiceId(const char *serviceId)
{
	UPnPServiceId = serviceId;
}

	
	
	
void
O2Profile::
SetDatStorageFlag(bool flag)
{
	DatStorageFlag = flag;
}
void
O2Profile::
GetFlags(string &out) const
{
	out += PublicReport		? 'r' : '-';
	out += PublicRecentDat	? 't' : '-';
	out += DatStorageFlag	? 'D' : '-';
}


	
	
bool
O2Profile::
Save(void)
{
wchar_t aaa[256];GetCurrentDirectoryW(256,aaa);

	O2ProfileSelectCondition cond;
	cond.mask = PROF_XMLELM_ALL ^ PROF_XMLELM_IP;
	cond.rootelement = L"profile";

	Lock();
	string out;
	ExportToXML(cond, out);
	Unlock();
/*	
	FILE *fp;
	if (_wfopen_s(&fp, ProfileFilePath.c_str(), L"wb") != 0)
		return false;
	fwrite(&out[0], 1, out.size(), fp);
	fclose(fp);
*/
	File f;
	if (!f.open(ProfileFilePath.c_str(), MODE_W)) {
		if (Logger)
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0, L"ファイルを開けません(%s)", ProfileFilePath.c_str());
		return false;
	}
	f.write((void*)&out[0], out.size());
	f.close();

	return true;
}




bool
O2Profile::
Load(void)
{
	struct _stat st;
	if (_wstat(ProfileFilePath.c_str(), &st) == -1)
		return false;
	if (st.st_size == 0)
		return false;
	return (ImportFromXML(ProfileFilePath.c_str(), NULL, 0));
}




bool
O2Profile::
GetEncryptedProfile(string &out) const
{
	if (!IP || !P2PPort)
		return false;

	string idstr;
	ID.to_string(idstr);

	string encipstr;
	ip2e(IP, encipstr);

	string encportstr;
	port2e(P2PPort, encportstr);
		
	out = idstr + encipstr + encportstr;
	return true;
}




bool
O2Profile::
ExportToXML(const O2ProfileSelectCondition cond, string &out)
{
	wstring tmpstr;
	wchar_t tmp[16];

	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	if (!cond.xsl.empty()) {
		xml += L"<?xml-stylesheet type=\"text/xsl\" href=\"";
		xml += cond.xsl;
		xml += L"\"?>"EOL;
	}

	xml += L"<";
	xml += cond.rootelement;
	xml += L">"EOL;

	Lock();

	//ID
	if (cond.mask & PROF_XMLELM_ID) {
		ID.to_string(tmpstr);
		xml += L" <ID>";
		xml += tmpstr;
		xml += L"</ID>"EOL;
	}

	//IP
	if (cond.mask & PROF_XMLELM_IP) {
		ip2e(IP, tmpstr);
		xml += L" <IP>";
		xml += tmpstr;
		xml += L"</IP>"EOL;
	}

	//NodeName
	if (cond.mask & PROF_XMLELM_NAME) {
		//convertGTLT(NodeName, tmpstr);
		xml += L" <nodename><![CDATA[";
		xml += NodeNameW;
		xml += L"]]></nodename>"EOL;
	}

	//Comment
	if (cond.mask & PROF_XMLELM_COMMENT) {
		//convertGTLT(Comment, tmpstr);
		xml += L" <comment><![CDATA[";
		xml += Comment;
		xml += L"]]></comment>"EOL;
	}

	//P2PPort
	if (cond.mask & PROF_XMLELM_P2PPORT) {
		xml += L" <port>";
		swprintf_s(tmp, 16, L"%d", P2PPort);
		xml += tmp;
		xml += L"</port>"EOL;
	}

	//ProxyPort
	if (cond.mask & PROF_XMLELM_PROXYPORT) {
		xml += L" <ProxyPort>";
		swprintf_s(tmp, 16, L"%d", ProxyPort);
		xml += tmp;
		xml += L"</ProxyPort>"EOL;
	}

	//AdminPort
	if (cond.mask & PROF_XMLELM_ADMINPORT) {
		xml += L" <AdminPort>";
		swprintf_s(tmp, 16, L"%d", AdminPort);
		xml += tmp;
		xml += L"</AdminPort>"EOL;
	}
	
	//PrivKey
	if (cond.mask & PROF_XMLELM_PRIVKEY) {
		PrivKey.to_string(tmpstr);
		xml += L" <PrivKey>";
		xml += tmpstr;
		xml += L"</PrivKey>"EOL;
	}

	//PubKey
	if (cond.mask & PROF_XMLELM_PUBKEY) {
		PubKey.to_string(tmpstr);
		xml += L" <PubKey>";
		xml += tmpstr;
		xml += L"</PubKey>"EOL;
	}

	//DBDir
	if (cond.mask & PROF_XMLELM_DBDIR) {
		xml += L" <DBDir><![CDATA[";
		xml += DBDirW;
		xml += L"]]></DBDir>"EOL;
	}

	//CacheRoot
	if (cond.mask & PROF_XMLELM_CACHEROOT) {
		xml += L" <CacheRoot><![CDATA[";
		xml += CacheRootW;
		xml += L"]]></CacheRoot>"EOL;
	}

	//AdminRoot
	if (cond.mask & PROF_XMLELM_CACHEROOT) {
		xml += L" <AdminRoot><![CDATA[";
		xml += AdminRootW;
		xml += L"]]></AdminRoot>"EOL;
	}

	//AdminBrowserType
	if (cond.mask & PROF_XMLELM_ADMIN_BROWSER_TYPE) {
		xml += L" <AdminBrowserType>";
		xml += AdminBrowserType;
		xml += L"</AdminBrowserType>"EOL;
	}

	//AdminBrowserPath
	if (cond.mask & PROF_XMLELM_ADMIN_BROWSER_PATH) {
		xml += L" <AdminBrowserPath><![CDATA[";
		xml += AdminBrowserPath;
		xml += L"]]></AdminBrowserPath>"EOL;
	}

	//UPnPAdapterName
	if (cond.mask & PROF_XMLELM_UPNP_ADAPTERNAME) {
		ascii2unicode(UPnPAdapterName, tmpstr);
		xml += L" <UPnPAdapterName><![CDATA[";
		xml += tmpstr;
		xml += L"]]></UPnPAdapterName>"EOL;
	}

	//UPnPLocation
	if (cond.mask & PROF_XMLELM_UPNP_LOCATION) {
		ascii2unicode(UPnPLocation, tmpstr);
		xml += L" <UPnPLocation><![CDATA[";
		xml += tmpstr;
		xml += L"]]></UPnPLocation>"EOL;
	}

	//UPnPServiceId
	if (cond.mask & PROF_XMLELM_UPNP_SERVICEID) {
		ascii2unicode(UPnPServiceId, tmpstr);
		xml += L" <UPnPServiceId><![CDATA[";
		xml += tmpstr;
		xml += L"]]></UPnPServiceId>"EOL;
	}

	//limits
	if (cond.mask & PROF_XMLELM_LIMIT) {
		swprintf_s(tmp, 16, L"%u", P2PSessionLimit);
		xml += L" <p2psessionlimit>";
		xml += tmp;
		xml += L"</p2psessionlimit>"EOL;

/*
		swprintf_s(tmp, 16, L"%u", NodeLimit);
		xml += L" <nodelimit>";
		xml += tmp;
		xml += L"</nodelimit>"EOL;
*/
		swprintf_s(tmp, 16, L"%u", KeyLimit);
		xml += L" <keylimit>";
		xml += tmp;
		xml += L"</keylimit>"EOL;

		swprintf_s(tmp, 16, L"%u", QueryLimit);
		xml += L" <querylimit>";
		xml += tmp;
		xml += L"</querylimit>"EOL;

		swprintf_s(tmp, 16, L"%u", LogLimit);
		xml += L" <loglimit>";
		xml += tmp;
		xml += L"</loglimit>"EOL;

		swprintf_s(tmp, 16, L"%u", NetLogLimit);
		xml += L" <netloglimit>";
		xml += tmp;
		xml += L"</netloglimit>"EOL;

		swprintf_s(tmp, 16, L"%u", HokanLogLimit);
		xml += L" <hokanloglimit>";
		xml += tmp;
		xml += L"</hokanloglimit>"EOL;

		swprintf_s(tmp, 16, L"%u", IPFLogLimit);
		xml += L" <ipfloglimit>";
		xml += tmp;
		xml += L"</ipfloglimit>"EOL;

		swprintf_s(tmp, 16, L"%I64u", QuarterSize);
		xml += L" <quartersize>";
		xml += tmp;
		xml += L"</quartersize>"EOL;

		swprintf_s(tmp, 16, L"%u", QuarterFullOperation);
		xml += L" <quarterfulloperation>";
		xml += tmp;
		xml += L"</quarterfulloperation>"EOL;

		swprintf_s(tmp, 16, L"%u", ResumeDelayMs);
		xml += L" <ResumeDelayMs>";
		xml += tmp;
		xml += L"</ResumeDelayMs>"EOL;
	}

	if (cond.mask & PROF_XMLELM_BOOL) {
		xml += L" <port0>";
		xml += (Port0 ? L"1" : L"0");
		xml += L"</port0>"EOL;

		xml += L" <p2pautostart>";
		xml += (P2PAutoStart ? L"1" : L"0");
		xml += L"</p2pautostart>"EOL;

		xml += L" <loadsubdirindex>";
		xml += (LoadSubdirIndex ? L"1" : L"0");
		xml += L"</loadsubdirindex>"EOL;

		xml += L" <maruuser>";
		xml += (MaruUser ? L"1" : L"0");
		xml += L"</maruuser>"EOL;

		xml += L" <publicprofile>";
		xml += (PublicReport ? L"1" : L"0");
		xml += L"</publicprofile>"EOL;

		xml += L" <publicrecentdat>";
		xml += (PublicRecentDat ? L"1" : L"0");
		xml += L"</publicrecentdat>"EOL;

		xml += L" <baloon_p2p>";
		xml += (Baloon_P2P ? L"1" : L"0");
		xml += L"</baloon_p2p>"EOL;

		xml += L" <baloon_query>";
		xml += (Baloon_Query ? L"1" : L"0");
		xml += L"</baloon_query>"EOL;

		xml += L" <baloon_hokan>";
		xml += (Baloon_Hokan ? L"1" : L"0");
		xml += L"</baloon_hokan>"EOL;

		xml += L" <baloon_im>";
		xml += (Baloon_IM ? L"1" : L"0");
		xml += L"</baloon_im>"EOL;

		xml += L" <AutoResume>";
		xml += (AutoResume ? L"1" : L"0");
		xml += L"</AutoResume>"EOL;

		xml += L" <UseUPnP>";
		xml += (UseUPnP ? L"1" : L"0");
		xml += L"</UseUPnP>"EOL;
	}

	Unlock();

	xml += L"</";
	xml += cond.rootelement;
	xml += L">"EOL;

	return (FromUnicode(cond.charset.c_str(), xml, out));
}




bool
O2Profile::
ImportFromXML(const wchar_t *filename, const char *in, uint len)
{
	bool ret = false;

	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	O2Profile_SAX2Handler handler(Logger, this);
	parser->setContentHandler(&handler);
	parser->setErrorHandler(&handler);

	Lock();
	{
		try {
			if (filename) {
				LocalFileInputSource source(filename);
				parser->parse(source);
			}
			else {
				MemBufInputSource source((const XMLByte*)in, len, "");
				parser->parse(source);
			}
			ret = true;
		}
		catch (const OutOfMemoryException &e) {
			if (Logger) {
				Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
					L"SAX2: Out of Memory: %s", e.getMessage());
			}
		}
		catch (const XMLException &e) {
			if (Logger) {
				Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
					L"SAX2: Exception: %s", e.getMessage());
			}
		}
		catch (...) {
			if (Logger) {
				Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
					L"SAX2: Unexpected exception during parsing.");
			}
		}
	}
	Unlock();

	delete parser;
	return (ret);
}




// ---------------------------------------------------------------------------
//
//	O2Profile_SAX2Handler::
//	Implementation of the SAX2 Handler interface
//
// ---------------------------------------------------------------------------

O2Profile_SAX2Handler::
O2Profile_SAX2Handler(O2Logger *lgr, O2Profile *prof)
	: SAX2Handler(MODULE, lgr)
	, Profile(prof)
{
}

O2Profile_SAX2Handler::
~O2Profile_SAX2Handler(void)
{
}

void
O2Profile_SAX2Handler::
endDocument(void)
{
}

void
O2Profile_SAX2Handler::
startElement(const XMLCh* const uri
		   , const XMLCh* const localname
		   , const XMLCh* const qname
		   , const Attributes& attrs)
{
	CurElm = PROF_XMLELM_NONE;
	plimit = NULL;
	pbool = NULL;

	if (MATCHLNAME(L"profile")) {
		CurElm = PROF_XMLELM_NONE;
	}
	else if (MATCHLNAME(L"ID")) {
		CurElm = PROF_XMLELM_ID;
	}
	else if (MATCHLNAME(L"PubKey")) {
		CurElm = PROF_XMLELM_PUBKEY;
	}
	else if (MATCHLNAME(L"IP")) {
		CurElm = PROF_XMLELM_IP;
	}
	else if (MATCHLNAME(L"nodename")) {
		CurElm = PROF_XMLELM_NAME;
	}
	else if (MATCHLNAME(L"comment")) {
		CurElm = PROF_XMLELM_COMMENT;
	}
	else if (MATCHLNAME(L"port")) {
		CurElm = PROF_XMLELM_P2PPORT;
	}
	// local
	else if (MATCHLNAME(L"ProxyPort")) {
		CurElm = PROF_XMLELM_PROXYPORT;
	}
	else if (MATCHLNAME(L"AdminPort")) {
		CurElm = PROF_XMLELM_ADMINPORT;
	}
	else if (MATCHLNAME(L"PrivKey")) {
		CurElm = PROF_XMLELM_PRIVKEY;
	}
	else if (MATCHLNAME(L"DBDir")) {
		CurElm = PROF_XMLELM_DBDIR;
	}
	else if (MATCHLNAME(L"CacheRoot")) {
		CurElm = PROF_XMLELM_CACHEROOT;
	}
	else if (MATCHLNAME(L"AdminRoot")) {
		CurElm = PROF_XMLELM_ADMINROOT;
	}
	else if (MATCHLNAME(L"AdminBrowserType")) {
		CurElm = PROF_XMLELM_ADMIN_BROWSER_TYPE;
	}
	else if (MATCHLNAME(L"AdminBrowserPath")) {
		CurElm = PROF_XMLELM_ADMIN_BROWSER_PATH;
	}
	else if (MATCHLNAME(L"UPnPAdapterName")) {
		CurElm = PROF_XMLELM_UPNP_ADAPTERNAME;
	}
	else if (MATCHLNAME(L"UPnPLocation")) {
		CurElm = PROF_XMLELM_UPNP_LOCATION;
	}
	else if (MATCHLNAME(L"UPnPServiceId")) {
		CurElm = PROF_XMLELM_UPNP_SERVICEID;
	}

	else if (MATCHLNAME(L"p2psessionlimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->P2PSessionLimit;
	}
/*
	else if (MATCHLNAME(L"nodelimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->NodeLimit;
	}
*/	else if (MATCHLNAME(L"keylimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->KeyLimit;
	}
	else if (MATCHLNAME(L"querylimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->QueryLimit;
	}

	else if (MATCHLNAME(L"loglimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->LogLimit;
	}
	else if (MATCHLNAME(L"netloglimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->NetLogLimit;
	}
	else if (MATCHLNAME(L"hokanloglimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->HokanLogLimit;
	}
	else if (MATCHLNAME(L"ipfloglimit")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->IPFLogLimit;
	}
	else if (MATCHLNAME(L"quartersize")) {
		CurElm = PROF_XMLELM_SIZE_T;
		puint64 = &Profile->QuarterSize;
	}
	else if (MATCHLNAME(L"quarterfulloperation")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->QuarterFullOperation;
	}
	else if (MATCHLNAME(L"ResumeDelayMs")) {
		CurElm = PROF_XMLELM_LIMIT;
		plimit = &Profile->ResumeDelayMs;
	}

	else if (MATCHLNAME(L"port0")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->Port0;
	}
	else if (MATCHLNAME(L"p2pautostart")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->P2PAutoStart;
	}
	else if (MATCHLNAME(L"loadsubdirindex")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->LoadSubdirIndex;
	}
	else if (MATCHLNAME(L"maruuser")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->MaruUser;
	}
	else if (MATCHLNAME(L"publicprofile")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->PublicReport;
	}
	else if (MATCHLNAME(L"publicrecentdat")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->PublicRecentDat;
	}
	else if (MATCHLNAME(L"baloon_p2p")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->Baloon_P2P;
	}
	else if (MATCHLNAME(L"baloon_query")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->Baloon_Query;
	}
	else if (MATCHLNAME(L"baloon_hokan")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->Baloon_Hokan;
	}
	else if (MATCHLNAME(L"baloon_im")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->Baloon_IM;
	}
	else if (MATCHLNAME(L"AutoResume")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->AutoResume;
	}
	else if (MATCHLNAME(L"UseUPnP")) {
		CurElm = PROF_XMLELM_BOOL;
		pbool = &Profile->UseUPnP;
	}
}




void
O2Profile_SAX2Handler::
endElement(const XMLCh* const uri
		 , const XMLCh* const localname
		 , const XMLCh* const qname)
{
	CurElm = PROF_XMLELM_NONE;
	if (!MATCHLNAME(L"profile"))
		return;

	if (Profile->PrivKey.size() != RSA_PRIVKEY_SIZE
			|| Profile->PubKey.size() != RSA_PUBKEY_SIZE) {
		Profile->SetRSAKey(NULL, 0, NULL, 0);
	}
}




void
O2Profile_SAX2Handler::
characters(const XMLCh* const chars, const unsigned int length)
{
	wstring str;

	switch (CurElm) {
		case PROF_XMLELM_ID:
			Profile->ID.assign(chars, length);
			break;
		case PROF_XMLELM_PRIVKEY:
			Profile->PrivKey.assign(chars, length);
			break;
		case PROF_XMLELM_PUBKEY:
			Profile->PubKey.assign(chars, length);
			break;
		case PROF_XMLELM_IP:
			Profile->IP = e2ip(chars, length);
			break;
		case PROF_XMLELM_P2PPORT:
			Profile->P2PPort = (ushort)wcstoul(chars, NULL, 10);
			break;
		case PROF_XMLELM_PROXYPORT:
			Profile->ProxyPort = (ushort)wcstoul(chars, NULL, 10);
			break;
		case PROF_XMLELM_ADMINPORT:
			Profile->AdminPort = (ushort)wcstoul(chars, NULL, 10);
			break;
		case PROF_XMLELM_NAME:
			if (length <= O2_MAX_NAME_LEN) {
				str.assign(chars, length);
				Profile->SetNodeName(str.c_str());
			}
			break;
		case PROF_XMLELM_COMMENT:
			if (length <= O2_MAX_COMMENT_LEN) {
				for (size_t i = 0; i < length; i++) {
					if (chars[i] == L'\n')
						Profile->Comment += L"\r\n";
					else
						Profile->Comment += chars[i];
				}
			}
			break;
		case PROF_XMLELM_DBDIR:
			str.assign(chars, length);
			Profile->SetDBDir(str.c_str());
			break;
		case PROF_XMLELM_CACHEROOT:
			str.assign(chars, length);
			Profile->SetCacheRoot(str.c_str());
			break;
		case PROF_XMLELM_ADMINROOT:
			str.assign(chars, length);
			Profile->SetAdminRoot(str.c_str());
			break;
		case PROF_XMLELM_ADMIN_BROWSER_TYPE:
			Profile->AdminBrowserType.assign(chars, length);
			break;
		case PROF_XMLELM_ADMIN_BROWSER_PATH:
			Profile->AdminBrowserPath.assign(chars, length);
			break;
		case PROF_XMLELM_UPNP_ADAPTERNAME:
			str.assign(chars, length);
			unicode2ascii(str, Profile->UPnPAdapterName);
			break;
		case PROF_XMLELM_UPNP_LOCATION:
			str.assign(chars, length);
			unicode2ascii(str, Profile->UPnPLocation);
			break;
		case PROF_XMLELM_UPNP_SERVICEID:
			str.assign(chars, length);
			unicode2ascii(str, Profile->UPnPServiceId);
			break;

		case PROF_XMLELM_LIMIT:
			if (plimit)
				*plimit = wcstoul(chars, NULL, 10);
			break;
		case PROF_XMLELM_SIZE_T:
			if (puint64)
				*puint64 = _wcstoui64(chars, NULL, 10);
			break;
		case PROF_XMLELM_BOOL:
			*pbool = chars[0] == '1' ? true : false;
			break;
	}
}
