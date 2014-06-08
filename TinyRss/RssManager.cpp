#include "stdpch.h"
#define TINYXML2_IMPORT
#include "tinyxml2.h"
#pragma comment(lib,"tinyxml2/tinyxml2.lib")

typedef tinyxml2::XMLDocument	TiXmlDocument;
typedef tinyxml2::XMLNode		TiXmlNode;
typedef tinyxml2::XMLElement	TiXmlElement;
typedef tinyxml2::XMLAttribute	TiXmlAttribute;
typedef tinyxml2::XMLComment	TiXmlComment;
typedef tinyxml2::XMLText		TiXmlText;
typedef tinyxml2::XMLDeclaration	TiXmlDeclaration;
typedef tinyxml2::XMLUnknown	TiXmlUnknown;
typedef tinyxml2::XMLPrinter	TiXmlPrinter;
typedef tinyxml2::XMLHandle		TiXmlHandle;

#include "Charset.h"

#define  RssManager_Cpp
#include "RssManager.h"
#include "http.h"
#include "SQLite.h"

using namespace std;
using namespace tinyxml2;


bool CRssManager::OpenDB( const char* file )
{
	return m_RssDB->Open(file);
}

bool CRssManager::CloseDB()
{
	return m_RssDB->Close();
}


bool CRssManager::GetSources( std::vector<CSource*>* sources )
{
	return m_RssDB->GetSources(sources);
}

bool CRssManager::GetRssSource(CSource* source, CRssSource* rss)
{
	auto*& p = rss;

	for(auto& item : p->RssItems){
		delete item;
	}

	p->RssItems.clear();
	try{
		GetRssContent(*source, p);
	}
	catch(...){
		throw;
	}

	for(auto item : p->RssItems){
		m_RssDB.AddCache(source->source.c_str(), item);
	}

	m_RssDB->MarkReadFromCache(p);
	m_RssDB->AppendUnreadCache(p);
	m_RssDB->RemoveOutdatedCaches(*p);

	return true;
}

bool CRssManager::GetRssContent(CSource& src, CRssSource* rss)
{
	string host,file;
	SplitSourceLink(src.source.c_str(), &host, &file);

	CMyByteArray bytes;
	CMyHttpResonse* R = nullptr;

	try{
		CMyHttp http(host.c_str());
		http.Connect();

		stringstream get;
		get << "GET " << file << " HTTP/1.1\r\n"
			"Host: " << host << "\r\n"
			"Accept: */*\r\n"
			"Accept-Encoding: gzip, deflate\r\n"
			"Connection: keep-alive\r\n"
			"Accept-Language: en-US,en;q=0.8\r\n"
			"User-Agent: Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/35.0.1916.114 Safari/537.36\r\n"
			"\r\n";

		string get_str(get.str());

		try{
			http.PostData(get_str.c_str(),get_str.size(), 0, &R);
		}
		catch(const char*){
			//cout << "[Error] " << str << endl;
			throw;
		}
		http.Disconnect();
	}
	catch(...){
		if(R)
			delete R;
		throw;
	}
	
	if(R->size){
		CRssParser parser;
		parser.Parse((char*)R->data, rss);
	}

	delete R;
	return true;
}

bool CRssManager::SplitSourceLink( const char* link, std::string* host, std::string* file )
{
	const char* phttp = strstr(link, "://");
	const char* host_start = phttp + 3;
	const char* host_end = strstr(host_start,"/"); // -1
	if(host_end){
		string h(host_start, string::size_type(host_end-host_start));
		*host = h;
		*file = host_end;
	}
	else{
		*host = host_start;
		*file = '/';
	}
	return true;
}

CSQLite& CRssManager::GetDB()
{
	return m_RssDB;
}

CRssManager::CRssManager()
{

}

//////////////////////////////////////////////////////////////////////////

bool CRssParser::Parse(const char* xml, CRssSource* R)
{
	string ansi;
	auto bom = reinterpret_cast<const unsigned char*>(xml);
	if(bom[0]==0xEF 
		&& bom[1]==0xBB 
		&& bom[2]==0xBF)
		xml += 3;
	transEncoding(xml, &ansi);
	TiXmlDocument doc;
	if(doc.Parse(ansi.c_str())!=0)
		return false;

	auto pRss = doc.FirstChildElement("rss");
	if(pRss) return parseRss(&doc, R);

	auto pFeed = doc.FirstChildElement("feed");
	if(pFeed) return parseAtom(&doc, R);

	throw "未知的XML文件内容!";

	return false;
}

bool CRssParser::transEncoding(const char* xml, string* str)
{
	*str = Utf82Ansi(xml);
	return true;
}

bool CRssParser::parseAtom(TiXmlDocument* doc, CRssSource* R)
{
	auto nodeFeed = doc->FirstChildElement("feed");
	if(nodeFeed == nullptr){
		throw "未找到feed, 不是有效的Atom源!";
	}

	auto GetText = [](TiXmlHandle& handle)->TiXmlText*
	{
		return handle.FirstChild().ToText();
	};

	for(auto node = nodeFeed->FirstChildElement();
		node != nullptr;
		node = node->NextSiblingElement())
	{
		auto name = node->Name();
		TiXmlHandle hnode(node);
		if(strcmp(name, "entry") == 0){
			auto pItem = new CRssItem;
			R->RssItems.push_back(pItem);
			for(auto entry = node->FirstChildElement();
				entry != nullptr;
				entry = entry->NextSiblingElement())
			{
				auto ename = entry->Name();
				TiXmlHandle hentry(entry);
				if(strcmp(ename, "id")==0){
					if(auto id = GetText(hentry)) 
						pItem->strLink = id->Value();
				}
				else if(strcmp(ename, "title")==0){
					if(auto title = GetText(hentry))
						pItem->strTitle = title->Value();
				}
				else if(strcmp(ename, "summary")==0){
					if(auto summary = GetText(hentry))
						pItem->strDescription = summary->Value();
				}
				else if(strcmp(ename, "content")==0){

				}
				else{

				}
			}
		}
		else if(strcmp(name, "title")==0){
			if(auto title = GetText(hnode))
				R->strTitle = title->Value();
		}
		else if(strcmp(name, "subtitle")==0){
			if(auto subtitle = GetText(hnode))
				R->strDescription = subtitle->Value();
		}
		else if(strcmp(name, "author")==0){
			if(auto pUri = hnode.FirstChildElement("uri").FirstChild().ToText())
				R->strLink =pUri->Value();
		}
		else{

		}
	}
	return true;
}

bool CRssParser::parseRss(TiXmlDocument* doc, CRssSource* R)
{
	TiXmlElement* nodeRss = doc->FirstChildElement("rss");
	if(nodeRss == nullptr){
		throw "未找到rss节点, 不是有效的Rss源!";
	}

	TiXmlElement* nodeChannel = nodeRss->FirstChildElement("channel");
	if(nodeChannel == nullptr){
		throw "未找到channel节点, 空Rss源!";
	}

	auto GetText = [](TiXmlHandle& handle, const char* name)->TiXmlText*
	{
		return handle.FirstChildElement(name).FirstChild().ToText();
	};

	TiXmlHandle handle(nodeChannel);
	if(auto title = GetText(handle, "title"))
		R->strTitle = title->Value();
	if(auto link = GetText(handle, "link"))
		R->strLink = link->Value();
	if(auto desc = GetText(handle, "description"))
		R->strDescription = desc->Value();

	for(auto nodeItem=nodeChannel->FirstChildElement("item");
		nodeItem != nullptr;
		nodeItem = nodeItem->NextSiblingElement("item"))
	{
		auto pRssItem = new CRssItem;
		TiXmlHandle hitem(nodeItem);
		if(auto title = GetText(hitem, "title"))
			pRssItem->strTitle = title->Value();
		if(auto link = GetText(hitem, "link"))
			pRssItem->strLink = link->Value();
		if(auto desc = GetText(hitem, "description"))
			pRssItem->strDescription = desc->Value();

		R->RssItems.push_back(pRssItem);
	}
	return true;
}
