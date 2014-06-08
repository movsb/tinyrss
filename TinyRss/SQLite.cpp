#include "stdpch.h"

#include "sqlite3/sqlite3.h"
#pragma comment(lib,"sqlite3/sqlite3.lib")

#include "SQLite.h"
#include "RssManager.h"


bool CSQLite::Open(const char* db)
{
	int rv;
	rv = sqlite3_open(db, &m_db);
	if(rv != SQLITE_OK){
		sqlite3_close(m_db);
		m_db = nullptr;
		throw "未能成功打开数据库!";
	}

	CreateTableSources();
	CreateTableCaches();

	return true;
}

bool CSQLite::Close()
{
	if(sqlite3_close(m_db) == SQLITE_OK){
		m_db = nullptr;
		return true;
	}
	else{
		throw "未能成功关闭数据库!";
	}
	return false;
}

bool CSQLite::AddSource( const char* tt, const char* link, const char* category )
{
	std::stringstream ss;
	std::string sql;

	bool bExists;
	if(QuerySourceExistence(link, &bExists)
		&& bExists == true)
		return true;

	std::string ttt,tlink,tcate;
	if(WrapApostrophe(tt, &ttt))
		tt = ttt.c_str();
	if(WrapApostrophe(link, &tlink))
		link = tlink.c_str();
	if(WrapApostrophe(category, &tcate))
		category = tcate.c_str();

	ss << "insert into " << kSourcesTableName << " (title,source,category) values ('"
		<< tt << "','" 
		<< link << "','"
		<< category << "');";

	sql = ss.str();
	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) == SQLITE_OK){
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "未能添加到缓存!";
	}
	return false;
}


bool CSQLite::IsSourceExists( const char* link )
{
	bool b;
	if(QuerySourceExistence(link, &b)
		&& b == true)
		return true;
	return false;
}

bool CSQLite::DeleteSource( int id, const char* source)
{
	std::stringstream ss;
	std::string sql;
	ss << "delete from " << kSourcesTableName << " where id=" << id << ";";
	sql = ss.str();

	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK){
		std::string e(err);
		sqlite3_free(err);
		throw "删除RSS源时遇到错误!";
	}

	if(source && strlen(source)){
		ss.clear();
		ss.str(std::string(""));
		std::string ts;
		if(WrapApostrophe(source, &ts))
			source = ts.c_str();
		ss << "delete from " << kCachesTableName << " where source='" << source << "';";
		sql = ss.str();

		if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK){
			std::string e(err);
			sqlite3_free(err);
			throw "删除RSS源缓存时遇到错误!";
		}
	}
	return true;
}


bool CSQLite::UpdateSource( int id, CSource* pSource )
{
	std::stringstream ss;
	std::string sql;

	const char* t = pSource->title.c_str();
	const char* s = pSource->source.c_str();
	const char* c = pSource->category.c_str();

	std::string tt, ts, tc;
	if(WrapApostrophe(t, &tt))
		t = tt.c_str();
	if(WrapApostrophe(s, &ts))
		s = ts.c_str();
	if(WrapApostrophe(c, &tc))
		c = tc.c_str();

	ss << "update " << kSourcesTableName << " set title='" << t
		<< "',source='" << s
		<< "',category='" << c
		<< "' where id=" << id << ";";
	sql = ss.str();

	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) == SQLITE_OK){
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "更新RSS源时出错!";
	}
	return true;
}

bool CSQLite::GetSources(std::vector<CSource*>* sources)
{
	std::stringstream ss;
	std::string sql;
	ss << "select * from " << kSourcesTableName << ";";
	sql = ss.str();

	sqlite3_stmt* stmt;
	if(sqlite3_prepare(m_db, sql.c_str(),-1,&stmt,nullptr) == SQLITE_OK){
		int rv;
		while((rv = sqlite3_step(stmt)) == SQLITE_ROW){
			auto pSource = new CSource;
			pSource->id = sqlite3_column_int(stmt, 0);
			pSource->title = (const char*)sqlite3_column_text(stmt, 1);
			pSource->source = (const char*)sqlite3_column_text(stmt, 2);
			pSource->category = (const char*)sqlite3_column_text(stmt ,3);
			sources->push_back(pSource);
		}
		if(rv != SQLITE_DONE){
			throw "未能取得缓存数据!";
		}
		sqlite3_finalize(stmt);
		return true;
	}
	else{
		throw "sqlite3错误!";
	}
	return false;
}

// id integer primary key,source text,title text,link text,description blob,read integer
bool CSQLite::GetCaches(std::vector<CCache*>* caches, const char* source, int read)
{
	std::stringstream ss;
	std::string sql;
	std::string tsource;
	if(WrapApostrophe(source, &tsource))
		source = tsource.c_str();

	ss << "select * from " << kCachesTableName << " where source='" << source << "'";
	if(read == 1) ss << " and read=1;";
	else if(read == 0) ss << " and read=0;";
	else ss << ";";

	sql = ss.str();

	sqlite3_stmt* stmt;
	if(sqlite3_prepare(m_db, sql.c_str(),-1,&stmt,nullptr) == SQLITE_OK){
		int rv;
		while((rv = sqlite3_step(stmt)) == SQLITE_ROW){
			auto pCache = new CCache;
			pCache->id = sqlite3_column_int(stmt, 0);
			pCache->source = (const char*)sqlite3_column_text(stmt, 1);
			pCache->title = (const char*)sqlite3_column_text(stmt, 2);
			pCache->link = (const char*)sqlite3_column_text(stmt, 3);
			pCache->description = (const char*)sqlite3_column_blob(stmt, 4);
			pCache->read = !!sqlite3_column_int(stmt, 5);
			caches->push_back(pCache);
		}
		if(rv != SQLITE_DONE){
			throw "未能取得缓存数据!";
		}
		sqlite3_finalize(stmt);
		return true;
	}
	else{
		throw "sqlite3错误!";
	}
	return false;
}

bool CSQLite::AddCache( const char* source, const CRssItem* pItem)
{
	std::stringstream ss;
	std::string sql;

	bool bExists;
	if(QueryCacheExistence(source, pItem->strLink.c_str(), &bExists)
		&& bExists == true)
		return true;

	const char *s,*t,*l;
	std::string tsource,ttitle,tlink;
	s = WrapApostrophe(source,&tsource)?tsource.c_str() : source;
	t = WrapApostrophe(pItem->strTitle.c_str(),&ttitle) ? ttitle.c_str() : pItem->strTitle.c_str();
	l = WrapApostrophe(pItem->strLink.c_str(), &tlink) ? tlink.c_str() : pItem->strLink.c_str();
	
	ss << "insert into " << kCachesTableName << " (source,title,link,description,read) values ('"
		<< s << "','" 
		<< t << "','"
		<< l << "',"
		<< "?,"
		<< /*(pItem->bRead==1?1:0)*/ 0 << ");";
	sql = ss.str();

	sqlite3_stmt* stmt;
	bool bSucceed = false;
	if(sqlite3_prepare(m_db, sql.c_str(),-1,&stmt, nullptr) == SQLITE_OK){
		if(sqlite3_bind_blob(stmt, 1, pItem->strDescription.c_str(), pItem->strDescription.size()+1,nullptr) == SQLITE_OK){
			if(sqlite3_step(stmt) == SQLITE_DONE){
				bSucceed = true;
			}
		}
		sqlite3_finalize(stmt);
	}

	return bSucceed;
}

bool CSQLite::RemoveOutdatedCaches( const CRssSource& rss )
{
	std::vector<CCache*> caches;
	GetCaches(&caches, reinterpret_cast<CSource*>(rss.pSource)->source.c_str(), 1);
	
	for(auto cache : caches){
		bool bDelete = true;
		for(auto item : rss.RssItems){
			if(cache->link == item->strLink){
				bDelete = false;
				break;
			}
		}
		if(bDelete){
			SMART_ENSURE(RemoveCache(cache->id),==true)(cache->id)(cache->link).Warning();
		}
	}

	for(auto cache : caches)
		delete cache;

	return true;
}

bool CSQLite::MarkReadFromCache( CRssSource* rss )
{
	std::vector<CCache*> caches;
	GetCaches(&caches, reinterpret_cast<CSource*>(rss->pSource)->source.c_str(), 1);

	for(auto cache : caches){
		for(auto item : rss->RssItems){
			if(cache->link == item->strLink){
				item->bRead = true;
				break;
			}
		}
	}

	for(auto cache : caches)
		delete cache;

	return true;
}

bool CSQLite::MarkCacheAsRead(const char* rss, const char* link)
{
	std::stringstream ss;
	std::string sql;

	std::string trss,tlink;
	if(WrapApostrophe(rss,&trss))
		rss = trss.c_str();
	if(WrapApostrophe(link,&tlink))
		link = tlink.c_str();

	ss << "update " << kCachesTableName << " set read=1 where source='" << rss << "' and link='"
		<< link << "';";
	sql = ss.str();

	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) == SQLITE_OK){
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "未能标记为已读!";
	}
	return false;
}

bool CSQLite::AppendUnreadCache(CRssSource* rss)
{
	std::vector<CCache*> caches;
	GetCaches(&caches, reinterpret_cast<CSource*>(rss->pSource)->source.c_str(), 0);

	for(auto cache : caches){
		bool bNeeded = true;
		for(auto item : rss->RssItems){
			if(item->strLink == cache->link){
				bNeeded = false;
				break;
			}
		}
		if(bNeeded == false)
			continue;

		auto pItem = new CRssItem;
		pItem->bRead = false;
		pItem->strTitle = cache->title;
		pItem->strLink = cache->link;
		pItem->strDescription = cache->description;
		rss->RssItems.push_back(pItem);
	}

	for(auto cache : caches)
		delete cache;

	return true;
}

bool CSQLite::QueryCacheExistence( const char* source, const char* link, bool* bExists )
{
	std::stringstream ss;
	std::string sql;

	std::string tsource,tlink;
	if(WrapApostrophe(source,&tsource)) 
		source = tsource.c_str();
	if(WrapApostrophe(link,&tlink)) 
		link = tlink.c_str();

	ss << "select id from " << kCachesTableName << " where source='" << source
		<< "' and link='" << link << "';";
	sql = ss.str();

	char* err = nullptr;
	CQueryCacheExistence cb;
	int rv = sqlite3_exec(m_db, sql.c_str(), cbQeuryCallback, &cb, &err);
	if(rv == SQLITE_OK || rv == SQLITE_ABORT){
		*bExists = cb.bExists;
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "失败!";
	}
	return false;
}

bool CSQLite::CreateTableSources()
{
	std::stringstream ss;
	std::string sql;

	ss << "create table if not exists " << kSourcesTableName 
		<< " (id integer primary key,title text,source text,category text);";
	sql = ss.str();

	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) == SQLITE_OK){
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "未能创建源表!";
	}
	return false;
}

bool CSQLite::CreateTableCaches()
{
	std::stringstream ss;
	std::string sql;

	ss << "create table if not exists " << kCachesTableName 
		<< " (id integer primary key,source text,title text,link text,description blob,read integer);";
	sql = ss.str();

	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) == SQLITE_OK){
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "未能创建源表!";
	}
	return false;
}

int __cdecl CSQLite::cbQeuryCallback( void* ud,int argc,char** argv,char** col )
{
	auto bcb = reinterpret_cast<CSQLiteCallback*>(ud);
	if(0){}
	else if(bcb->type == CSQLiteCallback::TYPE::kQueryCacheExistence){
		auto pQuery = reinterpret_cast<CQueryCacheExistence*>(bcb);
		pQuery->bExists = true;
		return 1;
	}
	else if(bcb->type == CSQLiteCallback::TYPE::kQuerySourceExistence){
		auto pQuery = reinterpret_cast<CQuerySourceExistence*>(bcb);
		pQuery->bExists = true;
		return 1;
	}
	return 0;
}

bool CSQLite::RemoveCache( int id )
{
	std::stringstream ss;
	std::string sql;
	ss << "delete from " << kCachesTableName << " where id=" << id << ";";
	sql = ss.str();
	char* err = nullptr;
	if(sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) == SQLITE_OK){
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "未能删除id!";
	}
	return false;
}

CSQLite::CSQLite()
	: kCachesTableName("caches")
	, kSourcesTableName("sources")
{

}

CSQLite::~CSQLite()
{

}

bool CSQLite::QuerySourceExistence(const char* source, bool* bExists)
{
	std::stringstream ss;
	std::string sql;

	std::string apos;
	if(WrapApostrophe(source, &apos))
		source = apos.c_str();

	ss << "select id from " << kSourcesTableName << " where source='" << source << "';";
	sql = ss.str();

	char* err = nullptr;
	CQuerySourceExistence cb;
	int rv = sqlite3_exec(m_db, sql.c_str(), cbQeuryCallback, &cb, &err);
	if(rv == SQLITE_OK || rv == SQLITE_ABORT){
		*bExists = cb.bExists;
		return true;
	}
	else{
		std::string e(err);
		sqlite3_free(err);
		throw "失败!";
	}
	return false;
}

bool CSQLite::WrapApostrophe(const char* str, std::string* out)
{
	if(strchr(str,'\'')==nullptr)
		return false;

	std::string& t = *out;
	while(*str){
		if(*str != '\''){
			t += *str;
		}
		else{
			t += "''";
		}
		++str;
	}
	return true;
}
