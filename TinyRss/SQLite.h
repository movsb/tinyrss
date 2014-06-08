#pragma once

class CRssSource;

class CSource
{
public:
	int id;
	std::string title;
	std::string source;
	std::string category;
	CRssSource* pRss;

	CSource()
		: id(0)
	{}
	CSource(const char* t, const char* s, const char* c, CRssSource* r = nullptr)
	{
		title = t;
		source = s;
		category = c;
		pRss = r;
	}
};

class CCache
{
public:
	int id;
	std::string source;
	std::string title;
	std::string link;
	std::string description;
	bool read;
};

class CRssSource;
class CRssItem;
struct sqlite3;

class CSQLite
{
private:
	class CSQLiteCallback
	{
	public:
		enum class TYPE{
			kGetSources,
			kGetCaches,
			kQueryCacheExistence,
			kQuerySourceExistence,
		};
		TYPE type;

	public:
		CSQLiteCallback(TYPE _type)
			: type(_type)
		{}
	};

	class CQueryCacheExistence : public CSQLiteCallback
	{
	public:
		bool bExists;
	public:
		CQueryCacheExistence()
			: CSQLiteCallback(CSQLiteCallback::TYPE::kQueryCacheExistence)
			, bExists(false)
		{}
	};

	class CQuerySourceExistence : public CSQLiteCallback
	{
	public:
		bool bExists;
	public:
		CQuerySourceExistence()
			: CSQLiteCallback(CSQLiteCallback::TYPE::kQuerySourceExistence)
			, bExists(false)
		{}
	};

public:
	CSQLite();
	~CSQLite();
	CSQLite* operator->(){return this;}
	bool Open(const char* db);
	bool Close();

	bool AddSource(const char* tt, const char* link, const char* category);
	bool IsSourceExists(const char* link);

	// 删除指定id的RS源, 并指定是否需要删除源对应的缓存
	// 如果不需要删除缓存, 请指定source=nullptr 或 source=""
	bool DeleteSource(int id, const char* source = "");

	bool UpdateSource(int id, CSource* pSource);

	// 取出所有的Rss源
	bool GetSources(std::vector<CSource*>* sources);

	// 取出所有的缓存项,
	//	read: 0 - 未读
	//		1 - 已读
	//		-1 - 未读+已读
	bool GetCaches(std::vector<CCache*>* caches, const char* source, int read);

	// 添加到缓存
	bool AddCache(const char* source, const CRssItem* pItem);

	// 移除过时的缓存, 过时的缓存是指: Rss内容中没有, 并且被标记为 read
	bool RemoveOutdatedCaches(const CRssSource& rss);

	// 从缓存中取出标记为read的内容, 把他们从Rss内容中标记出来
	bool MarkReadFromCache(CRssSource* rss);

	// 把Cache中某项标记为 未读 ( 该项已经被通过AddCache 以 未读 添加进来)
	bool MarkCacheAsRead(const char* rss, const char* link);

	// 把Cache中未读的项添加到rss内容中
	bool AppendUnreadCache(CRssSource* rss);


private:
	static int __cdecl cbQeuryCallback(void* ud,int argc,char** argv,char** col);
	bool CreateTableSources();
	bool CreateTableCaches();
	bool RemoveCache(int id);
	bool QueryCacheExistence(const char* source, const char* link, bool* bExists);
	bool QuerySourceExistence(const char* source, bool* bExists);
	bool WrapApostrophe(const char* str, std::string* out);

private:
	sqlite3* m_db;
	const char* const kSourcesTableName;
	const char* const kCachesTableName;
};
