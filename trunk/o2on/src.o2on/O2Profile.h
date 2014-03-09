/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Profile.h
 * description	: o2on node
 *
 */

#pragma once
#include "O2Define.h"
#include "O2Logger.h"
#include "mutex.h"
#include "sha.h"
#include "rsa.h"
#include "O2SAX2Parser.h"




#define PROF_XMLELM_NONE				0x00000000
#define PROF_XMLELM_ID					0x00000001
#define PROF_XMLELM_IP					0x00000002
#define PROF_XMLELM_P2PPORT				0x00000004
#define PROF_XMLELM_NAME				0x00000008
#define PROF_XMLELM_COMMENT				0x00000010
#define PROF_XMLELM_PUBKEY				0x00000020
#define PROF_XMLELM_PROXYPORT			0x00000100
#define PROF_XMLELM_ADMINPORT			0x00000200
#define PROF_XMLELM_PRIVKEY				0x00000400
#define PROF_XMLELM_DBDIR				0x00000800
#define PROF_XMLELM_CACHEROOT			0x00001000
#define PROF_XMLELM_ADMINROOT			0x00002000
#define PROF_XMLELM_ADMIN_BROWSER_TYPE	0x00004000
#define PROF_XMLELM_ADMIN_BROWSER_PATH	0x00008000
#define PROF_XMLELM_UPNP_ADAPTERNAME	0x00010000
#define PROF_XMLELM_UPNP_LOCATION		0x00020000
#define PROF_XMLELM_UPNP_SERVICEID		0x00040000
#define PROF_XMLELM_LIMIT				0x00080000
#define PROF_XMLELM_BOOL				0x00100000
#define PROF_XMLELM_SIZE_T				0x00200000

#define PROF_XMLELM_COMMON				0x000000ff
#define PROF_XMLELM_ALL					0x0fffffff




/*
 *	O2ProfileSelectCondition
 *
 */
struct O2ProfileSelectCondition
{
	wstring		charset;
	uint		mask;
	wstring		xsl;
	wstring		timeformat;
	wstring		rootelement;

	O2ProfileSelectCondition(uint m = PROF_XMLELM_COMMON)
		: charset(_T(DEFAULT_XML_CHARSET))
		, mask(m)
		, rootelement(L"profile")
	{
	}
};




/*
 *	O2Profile
 *
 */
class O2Profile_SAX2Handler;

class O2Profile
	: public O2SAX2Parser
	, private Mutex
{
private:
	hashT		ID;
	privT		PrivKey;
	pubT		PubKey;

	ulong		IP;
	ushort		P2PPort;
	ushort		ProxyPort;
	ushort		AdminPort;
	string		NodeNameA;
	wstring		NodeNameW;
	wstring		Comment;

	string		UserAgentA;
	wstring		UserAgentW;
	string		ConfDirA;
	wstring		ConfDirW;
	string		DBDirA;
	wstring		DBDirW;
	string		CacheRootA;
	wstring		CacheRootW;
	string		AdminRootA;
	wstring		AdminRootW;

	wstring		ProfileFilePath;
	wstring		NodeFilePath;
	wstring		FriendFilePath;
	wstring		QueryFilePath;
	wstring		SakuFilePath;
	wstring 	IMFilePath;
	wstring 	ReportFilePath;
	wstring		IPF_P2PFilePath;
	wstring		IPF_ProxyFilePath;
	wstring		IPF_AdminFilePath;

	uint		P2PSessionLimit;
	uint		AgentInterval;
	uint		NodeCollectorLimit;
	uint		KeyCollectorLimit;
	uint		QueryCollectorLimit;
	uint		DatCollectorLimit;

	bool		Port0;
	bool		P2PAutoStart;
	bool		LoadSubdirIndex;
	bool		MaruUser;
	bool		PublicReport;
	bool		PublicRecentDat;

	//uint		NodeLimit;
	uint		KeyLimit;
	uint		QueryLimit;

	uint		LogLimit;
	uint		NetLogLimit;
	uint		HokanLogLimit;
	uint		IPFLogLimit;

	bool		Baloon_P2P;
	bool		Baloon_Query;
	bool		Baloon_Hokan;
	bool		Baloon_IM;

	bool		AutoResume;
	uint		ResumeDelayMs;

	uint64		QuarterSize;
	uint		QuarterFullOperation;

	wstring		AdminBrowserType;
	wstring		AdminBrowserPath;

	bool		UseUPnP;
	string		UPnPAdapterName;
	string		UPnPLocation;
	string		UPnPServiceId;

	bool		DatStorageFlag;

	//Logger
	O2Logger	*Logger;

public:
	O2Profile(O2Logger *lgr, bool load);
	~O2Profile();

	void	assign(O2Profile &src);

	bool	SetIP(ulong globalIP);
	ulong	GetIP(void) const;
	bool	SetP2PPort(ushort pn);
	bool	SetProxyPort(ushort pn);
	bool	SetAdminPort(ushort pn);
	ushort	GetP2PPort(void) const;
	ushort	GetProxyPort(void) const;
	ushort	GetAdminPort(void) const;

	bool SetNodeName(const wchar_t *name);
	const char *GetNodeNameA(void) const;
	const wchar_t *GetNodeNameW(void) const;
	bool SetComment(const wchar_t *comment);
	const wchar_t *GetComment(void) const;

	void SetID(const byte *in, size_t len);
	void GetID(hashT &out) const;

	void SetRSAKey(const byte *priv, size_t privlen, const byte *pub, size_t publen);
	void GetPubkeyA(string &out) const;
	void GetPubkeyW(wstring &out) const;

	const char *GetUserAgentA(void) const;
	const wchar_t *GetUserAgentW(void) const;

	void SetConfDir(const wchar_t *path);
	const char *GetConfDirA(void) const;
	const wchar_t *GetConfDirW(void) const;
	bool MakeConfDir(void);

	void SetDBDir(const wchar_t *path);
	const char *GetDBDirA(void) const;
	const wchar_t *GetDBDirW(void) const;
	bool MakeDBDir(void);

	void SetCacheRoot(const wchar_t *path);
	const char *GetCacheRootA(void) const;
	const wchar_t *GetCacheRootW(void) const;
	bool MakeCacheRoot(void);

	void SetAdminRoot(const wchar_t *path);
	const char *GetAdminRootA(void) const;
	const wchar_t *GetAdminRootW(void) const;
	bool CheckAdminRoot(void);

	const wchar_t *GetProfileFilePath(void) const;
	const wchar_t *GetNodeFilePath(void) const;
	const wchar_t *GetFriendFilePath(void) const;
	const wchar_t *GetQueryFilePath(void) const;
	const wchar_t *GetSakuFilePath(void) const;
	const wchar_t *GetIMFilePath(void) const;
	const wchar_t *GetReportFilePath(void) const;
	const wchar_t *GetIPF_P2PFilePath(void) const;
	const wchar_t *GetIPF_ProxyFilePath(void) const;
	const wchar_t *GetIPF_AdminFilePath(void) const;

	uint GetP2PSessionLimit(void) const;
	void SetP2PSessionLimit(uint n);

	bool IsPort0(void) const;
	void SetPort0(bool flag);
	bool IsP2PAutoStart(void) const;
	void SetP2PAutoStart(bool flag);
	bool IsLoadSubdirIndex(void) const;
	void SetLoadSubdirIndex(bool flag);
	bool IsMaruUser(void) const;
	void SetMaruUser(bool flag);
	bool IsPublicReport(void) const;
	void SetPublicReport(bool flag);
	bool IsPublicRecentDat(void) const;
	void SetPublicRecentDat(bool flag);

	//uint GetNodeLimit(void) const;
	//void SetNodeLimit(uint n);
	uint GetKeyLimit(void) const;
	void SetKeyLimit(uint n);
	uint GetQueryLimit(void) const;
	void SetQueryLimit(uint n);

	uint GetLogLimit(void) const;
	void SetLogLimit(uint n);
	uint GetNetLogLimit(void) const;
	void SetNetLogLimit(uint n);
	uint GetHokanLogLimit(void) const;
	void SetHokanLogLimit(uint n);
	uint GetIPFLogLimit(void) const;
	void SetIPFLogLimit(uint n);

	bool IsBaloon_P2P(void) const;
	bool IsBaloon_Query(void) const;
	bool IsBaloon_Hokan(void) const;
	bool IsBaloon_IM(void) const;
	void SetBaloon_P2P(bool flag);
	void SetBaloon_Query(bool flag);
	void SetBaloon_Hokan(bool flag);
	void SetBaloon_IM(bool flag);

	bool IsAutoResume(void);
	void SetAutoResume(bool flag);
	uint GetResumeDelayMs(void);
	void SetResumeDelayMs(uint ms);

	uint64 GetQuarterSize(void) const;
	void SetQuarterSize(uint64 size);
	uint GetQuarterFullOperation(void) const;
	void SetQuarterFullOperation(uint id);

	const wchar_t *GetAdminBrowserType(void) const;
	void SetAdminBrowserType(const wchar_t *type);
	const wchar_t *GetAdminBrowserPath(void) const;
	void SetAdminBrowserPath(const wchar_t *path);

	bool UsingUPnP(void) const;
	void SetUseUPnP(bool flag);
	const char *GetUPnPAdapterName(void) const;
	void SetUPnPAdapterName(const char *adapterName);
	const char *GetUPnPLocation(void) const;
	void SetUPnPLocation(const char *location);
	const char *GetUPnPServiceId(void) const;
	void SetUPnPServiceId(const char *serviceId);

	void SetDatStorageFlag(bool flag);
	void GetFlags(string &out) const;

	bool Save(void);
	bool Load(void);

	bool GetEncryptedProfile(string &out) const;
	bool ExportToXML(const O2ProfileSelectCondition cond, string &out);
	bool ImportFromXML(const wchar_t *filename, const char *in, uint len);

	// ÉÅÉìÉoÇ™ëΩÇ∑Ç¨ÇÈÇÃÇ≈friendÇ…ÇµÇƒÇµÇ‹Ç®Ç§
	friend O2Profile_SAX2Handler;
};




//
//	O2Profile_SAX2Handler
//
class O2Profile_SAX2Handler
	: public SAX2Handler
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	uint		CurElm;
	uint		*plimit;
	uint64		*puint64;
	bool		*pbool;
	wstring		buf;

public:
	O2Profile_SAX2Handler(O2Logger *lgr, O2Profile *prof);
	~O2Profile_SAX2Handler(void);

	void endDocument(void);
	void startElement(const XMLCh* const uri
					, const XMLCh* const localname
					, const XMLCh* const qname
					, const Attributes& attrs);
	void endElement(const XMLCh* const uri
				  , const XMLCh* const localname
				  , const XMLCh* const qname);
	void characters(const XMLCh* const chars
				  , const unsigned int length);
};
