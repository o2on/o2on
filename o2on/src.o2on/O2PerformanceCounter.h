/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2PerformanceCounter.h
 * description	: 
 *
 */

#pragma once
#include "O2Logger.h"
#include "O2Job.h"
#include "mutex.h"
#include "O2SAX2Parser.h"
#include "file.h"
#include "dataconv.h"
#include <tchar.h>
#include <time.h>
#include <pdh.h>




class O2PerformanceCounter
	: public O2Job
	, public O2SAX2Parser
	, public SAX2Handler
	, public Mutex
{
protected:
	HQUERY		hQuery;
	HCOUNTER	hCounter_ProcessorTime;
	HCOUNTER	hCounter_HandleCount;
	HCOUNTER	hCounter_ThreadCount;
	uint64		SampleNum;
	uint		CPUNum;
	double		ProcTime;
	double		ProcTimeAvg;
	int 		HandleCount;
	int 		ThreadCount;
	time_t		StartTime;
	uint64		Total_Uptime;
	uint64		Total_Send;
	uint64		Total_Recv;
	uint64		*pval;

public:
	O2PerformanceCounter(const wchar_t	*name
					   , time_t			interval
					   , bool			startup
					   , O2Logger		*lgr)
		: O2Job(name, interval, startup)
		, SAX2Handler(L"PerformanceCounter", lgr)
		, hQuery(NULL)
		, hCounter_ProcessorTime(NULL)
		, hCounter_HandleCount(NULL)
		, hCounter_ThreadCount(NULL)
		, SampleNum(0)
		, CPUNum(0)
		, ProcTime(0.0)
		, ProcTimeAvg(0.0)
		, HandleCount(0)
		, ThreadCount(0)
		, StartTime(0)
		, Total_Uptime(0)
		, Total_Send(0)
		, Total_Recv(0)
		, pval(NULL)
	{
		SYSTEM_INFO sinf;
		GetSystemInfo(&sinf);
		CPUNum = sinf.dwNumberOfProcessors;

		PdhOpenQuery(NULL, 0, &hQuery);
		PdhAddCounter(hQuery, _T("\\Process(o2on)\\% Processor Time"), 0, &hCounter_ProcessorTime);
		PdhAddCounter(hQuery, _T("\\Process(o2on)\\Handle Count"), 0, &hCounter_HandleCount);
		PdhAddCounter(hQuery, _T("\\Process(o2on)\\Thread Count"), 0, &hCounter_ThreadCount);
		PdhCollectQueryData(hQuery);
	}

	~O2PerformanceCounter()
	{
		if (hQuery)
			PdhCloseQuery(hQuery);
	}

	void JobThreadFunc(void)
	{
		Lock();
		{
			PdhCollectQueryData(hQuery);

			PDH_FMT_COUNTERVALUE fcval;
			PdhGetFormattedCounterValue(
				hCounter_ProcessorTime, PDH_FMT_DOUBLE, NULL, &fcval);
			ProcTime = fcval.doubleValue / CPUNum;
			ProcTimeAvg = (ProcTimeAvg * SampleNum + ProcTime) / (SampleNum + 1);

			PdhGetFormattedCounterValue(
				hCounter_HandleCount, PDH_FMT_LONG, NULL, &fcval);
			HandleCount = fcval.longValue;

			PdhGetFormattedCounterValue(
				hCounter_ThreadCount, PDH_FMT_LONG, NULL, &fcval);
			ThreadCount = fcval.longValue;

			SampleNum++;
		}
		Unlock();
	}

	void GetValue(double &ptime, double &ptimeavg, int &handle_c, int &thread_c)
	{
		JobThreadFunc();
		ptime = ProcTime;
		ptimeavg = ProcTimeAvg;
		handle_c = HandleCount;
		thread_c = ThreadCount;
	}

	void Start(void)
	{
		StartTime = time(NULL);
	}

	void Stop(uint64 total_send, uint64 total_recv)
	{
		if (StartTime != 0)
			Total_Uptime += (time(NULL) - StartTime);
		StartTime = 0;
		Total_Send += total_send;
		Total_Recv += total_recv;
	}

	time_t GetStartTime(void)
	{
		return (StartTime);
	}

	time_t GetUptime(void)
	{
		if (StartTime == 0)
			return (0);
		return (time(NULL) - StartTime);
	}

	time_t GetTotalUptime(void)
	{
		return (Total_Uptime);
	}
	time_t GetTotalSend(void)
	{
		return (Total_Send);
	}
	time_t GetTotalRecv(void)
	{
		return (Total_Recv);
	}

public:
	bool Save(const wchar_t *filename)
	{
		wstring xml;
		xml += L"<?xml version=\"1.0\" encoding=\"";
		xml += _T(DEFAULT_XML_CHARSET);
		xml += L"\"?>"EOL;
		xml += L"<report>"EOL;
		xml_AddElement(xml, L"uptime", NULL, Total_Uptime);
		xml_AddElement(xml, L"send",   NULL, Total_Send);
		xml_AddElement(xml, L"recv",   NULL, Total_Recv);
		xml += L"</report>"EOL;
		string out;
		FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
/*
		FILE *fp;
		if (_wfopen_s(&fp, filename, L"wb") != 0)
			return false;
		fwrite(&out[0], 1, out.size(), fp);
		fclose(fp);
*/
		File f;
		if (!f.open(filename, MODE_W)) {
			if (Logger)
				Logger->AddLog(O2LT_ERROR, L"PerformanceCounter", 0, 0,
				L"ファイルを開けません(%s)", filename);
			return false;
		}
		f.write((void*)&out[0], out.size());
		f.close();

		return true;
	}
	bool Load(const wchar_t *filename)
	{
		bool ret = false;

		struct _stat st;
		if (_wstat(filename, &st) == -1)
			return false;
		if (st.st_size == 0)
			return false;

		SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
		parser->setContentHandler(this);
		parser->setErrorHandler(this);

		Lock();
		{
			try {
				LocalFileInputSource source(filename);
				parser->parse(source);
				ret = true;
			}
			catch (const OutOfMemoryException &e) {
				if (Logger) {
					Logger->AddLog(O2LT_ERROR, L"PerformanceCounter", 0, 0,
						L"SAX2: Out of Memory: %s", e.getMessage());
				}
			}
			catch (const XMLException &e) {
				if (Logger) {
					Logger->AddLog(O2LT_ERROR, L"PerformanceCounter", 0, 0,
						L"SAX2: Exception: %s", e.getMessage());
				}
			}
			catch (...) {
				if (Logger) {
					Logger->AddLog(O2LT_ERROR, L"PerformanceCounter", 0, 0,
						L"SAX2: Unexpected exception during parsing.");
				}
			}
		}
		Unlock();

		delete parser;
		return (ret);
	}

public:
	void endDocument(void)
	{
	}
	void startElement(const XMLCh* const uri
					, const XMLCh* const localname
					, const XMLCh* const qname
					, const Attributes& attrs)
	{
		pval = NULL;
		if (MATCHLNAME(L"uptime")) {
			pval = &Total_Uptime;
		}
		else if (MATCHLNAME(L"send")) {
			pval = &Total_Send;
		}
		else if (MATCHLNAME(L"recv")) {
			pval = &Total_Recv;
		}
	}
	void endElement(const XMLCh* const uri
				  , const XMLCh* const localname
				  , const XMLCh* const qname)
	{
		pval = NULL;
	}
	void characters(const XMLCh* const chars
				  , const unsigned int length)
	{
		if (!pval)
			return;
		*pval = _wcstoui64(chars, NULL, 10);
	}
};
