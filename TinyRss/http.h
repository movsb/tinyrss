#pragma once

class CMyByteArray
{
public:
	CMyByteArray();
	~CMyByteArray();
	void* GetData();
	size_t GetSize();

	size_t Write(const void* data,size_t sz);

	//最后增加4字节的0, 但是不增加大小, 如果需要以0结尾的内存 ...
	bool MakeNull();
	bool Duplicate(void** ppData, bool bMakeNull = false);

private:
	void*	m_pData;
	size_t	m_iDataSize;
	size_t	m_iDataPos;

private:
	size_t GetGranularity();
	size_t GetSpaceLeft();
	size_t& GetSpaceUsed();

	void ReallocateDataSpace(size_t szAddition);
};

struct CMyHttpResonse
{
	std::string version;
	std::string state;
	std::string comment;

	std::map<std::string,std::string> heads;

	void* data;
	size_t size;

	CMyHttpResonse():data(0){}
	~CMyHttpResonse(){if(data) delete[] data;}
};

class CMyHttp
{
public:
	CMyHttp(const char* host,const char* port="80");

public:
	bool Connect();
	bool Disconnect();
	bool PostData(const char* data,size_t sz, int /*timeouts*/,CMyHttpResonse** ppr);

private:
	bool CMyHttp::parseResponse(CMyHttpResonse* R);
	bool send(const void* data,size_t sz);
	int recv(char* buf,size_t sz);
	bool ungzip(const void* data, size_t sz, CMyByteArray* bytes);

private:
	std::string m_ip;
	std::string m_port;
	SOCKET m_sock;
};
