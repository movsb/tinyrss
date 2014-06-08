#pragma once

#include "SQLite.h"

class CSQLite;
class CSource;

#ifndef RssManager_Cpp
typedef struct TiXmlDocument TiXmlDocument;
#else 
#undef RssManager_Cpp
#endif

class CRssItem
{
public:
	std::string strTitle;
	std::string strLink;
	std::string strDescription;
	bool bRead;

	CRssItem()
		: bRead(false)
	{}
};


class CRssSource
{
public:
	std::string strTitle;
	std::string strLink;
	std::string strDescription;
	std::vector<CRssItem*> RssItems;
	CSource* pSource;
};

class CRssParser
{
public:
	CRssParser(){}
	bool Parse(const char* xml, CRssSource* R);

private:
	bool transEncoding(const char* xml, std::string* str);
	bool parseRss(TiXmlDocument* doc, CRssSource* R);
	bool parseAtom(TiXmlDocument* doc, CRssSource* R);
};


class CRssManager
{
public:
	CRssManager();
	bool OpenDB(const char* file);
	bool CloseDB();
	bool GetSources(std::vector<CSource*>* sources);
	bool GetRssSource(CSource* source, CRssSource* rss);
	CSQLite& GetDB();

private:
	bool GetRssContent(CSource& src, CRssSource* rss);
	bool SplitSourceLink(const char* link, std::string* host, std::string* file);
private:
	CSQLite m_RssDB;
};
