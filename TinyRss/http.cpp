#include "stdpch.h"

#include "zlib.h"
#pragma comment(lib,"zdll.lib")

#include "http.h"

CMyHttp::CMyHttp(const char* host,const char* port)
{
	m_port = port;
	if(*host>='0' && *host<='9'){
		m_ip = host;
	}else{
		PHOSTENT pHost = nullptr;
		pHost = ::gethostbyname(host);
		if(pHost){
			char str[16];
			unsigned char* pip = (unsigned char*)*pHost->h_addr_list;
			sprintf(str,"%d.%d.%d.%d",pip[0],pip[1],pip[2],pip[3]);
			m_ip = str;
		}
		else{
			throw "无法解析到服务器IP地址!";
		}
	}
}

bool CMyHttp::Connect()
{
	int rv;
	SOCKADDR_IN addr;

	SOCKET s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(s==INVALID_SOCKET)
		throw "建立套接字失败!";

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(m_port.c_str()));
	addr.sin_addr.s_addr = inet_addr(m_ip.c_str());
	rv = connect(s,(sockaddr*)&addr,sizeof(addr));
	if(rv == SOCKET_ERROR)
		throw "无法连接到服务器!";
	m_sock = s;

	return true;
}

bool CMyHttp::Disconnect()
{
	closesocket(m_sock);
	return true;
}

bool CMyHttp::PostData(const char* data,size_t sz, int /*timeouts*/,CMyHttpResonse** ppr)
{
	CMyHttpResonse* R = new CMyHttpResonse;
	if(!send(data,sz)){
		throw "未能成功向服务器发送数据!";
	}
	parseResponse(R);
	*ppr = R;
	return true;
}

bool CMyHttp::parseResponse(CMyHttpResonse* R)
{
	const char* kHttpError = "HTTP报文解析遇到错误, 请向作者反馈信息!";
	char c;
	//读HTTP版本
	while(recv(&c,1) && c!=' '){
		R->version += c;
	}
	//读状态号
	while(recv(&c,1) && c!= ' '){
		R->state += c;
	}
	//读状态说明
	while(recv(&c,1) && c!='\r'){
		R->comment += c;
	}
	// \r已读,还有一个\n
	recv(&c,1);
	if(c!='\n')
		throw kHttpError;
	//读键值对
	for(;;){
		std::string k,v;
		//读键
		recv(&c,1);
		if(c=='\r'){
			recv(&c,1);
			assert(c=='\n');
			if(c!='\n')
				throw kHttpError;
			break;
		}
		else
			k += c;
		while(recv(&c,1) && c!=':'){
			k += c;
		}
		//读值
		recv(&c,1);
		assert(c==' ');
		if(c!=' ')
			throw kHttpError;
		while(recv(&c,1) && c!='\r'){
			v += c;
		}
		// 跳过 \n
		recv(&c,1);
		assert(c=='\n');
		if(c!='\n')
			throw kHttpError;
		//结果
		R->heads[k] = v;
	}
	//如果有数据返回的话, 数据从这里开始

	if(R->state != "200"){
		static std::string t; //Oops!!!
		t = R->version;
		t += " ";
		t += R->state;
		t += " ";
		t += R->comment;
		throw t.c_str();
	}
	
	bool bContentLength,bTransferEncoding;
	bContentLength = R->heads.count("Content-Length")!=0;
	bTransferEncoding = R->heads.count("Transfer-Encoding")!=0;

	if(bContentLength){
		int len = atoi(R->heads["Content-Length"].c_str());
		char* data = new char[len+1]; //多余的1字节,使结果为'\0',但返回实际长度
		memset(data,0,len+1);		//这句可以不要
		recv(data,len);

		bool bGzipped = R->heads["Content-Encoding"] == "gzip";
		if(bGzipped){
			CMyByteArray gzip;
			ungzip(data, len, &gzip);
			gzip.Duplicate(&R->data, true);
			R->size = gzip.GetSize();
			return true;
		}

		R->data = data;
		R->size = len;	//没有多余的1字节

		return true;
	}
	if(bTransferEncoding){
		//参考:http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
		if(R->heads["Transfer-Encoding"] != "chunked")
			throw "未处理的传送编码类型!";

		auto char_val = [](char ch)
		{
			int val = 0;
			if(ch>='0' && ch<='9') val = ch-'0'+ 0;
			if(ch>='a' && ch<='f') val = ch-'a'+10;
			if(ch>='A' && ch<='F') val = ch-'A'+10;
			return val;
		};

		int len;
		CMyByteArray bytes;

		for(;;){
			len = 0;
			//读取chunked数据长度
			while(recv(&c,1) && c!=' ' && c!='\r'){
				len = len*16 + char_val(c);
			}

			//结束当前chunked块长度部分
			if(c!='\r'){
				while(recv(&c,1) && c!='\r')
					;
			}
			recv(&c,1);
			assert(c=='\n');
			if(c!='\n')
				throw kHttpError;
			//块长度为零,chunked数据结束
			if(len == 0){
				break;
			}

			//以下开始读取chunked数据块数据部分 + \r\n
			char* p = new char[len+2];
			recv(p,len+2);

			assert(p[len+0] == '\r');
			assert(p[len+1] == '\n');
			if(p[len+0]!='\r')
				throw kHttpError;
			if(p[len+1]!='\n')
				throw kHttpError;

			bytes.Write(p,len);
			delete[] p;
		}
		//chunked 数据读取结束
		recv(&c,1);
		assert(c == '\r');
		if(c!='\r')
			throw kHttpError;
		recv(&c,1);
		assert(c == '\n');
		if(c!='\n')
			throw kHttpError;

		bool bGzipped = R->heads["Content-Encoding"] == "gzip";
		if(bGzipped){
			CMyByteArray gzip;
			if(!ungzip(bytes.GetData(), bytes.GetSize(), &gzip))
				throw "HTTP报文Gzip解压时遇到错误! 请反馈!";
			gzip.Duplicate(&R->data, true);
			R->size = gzip.GetSize();
			return true;
		}

		bytes.Duplicate(&R->data, true);
		R->size = bytes.GetSize();
		return true;
	}
	//如果到了这里, 说明没有消息数据
	R->data = 0;
	R->size = 0;
	return true;
}

bool CMyHttp::send(const void* data,size_t sz)
{
	const char* p = (const char*)data;
	int sent = 0;
	while(sent < (int)sz){
		int cb = ::send(m_sock,p+sent,sz-sent,0);
		if(cb==0) return false;
		sent += cb;
	}
	return true;
}

int CMyHttp::recv(char* buf,size_t sz)
{
	int rv;
	size_t read=0;
	while(read != sz){
		rv = ::recv(m_sock,buf+read,sz-read,0);
		if(!rv || rv == -1){
			throw "recv() 网络错误!";
		}
		read += rv;
	}
	return read; // equals to sz
}

bool CMyHttp::ungzip(const void* data, size_t sz, CMyByteArray* bytes)
{
	int ret;
	z_stream strm;
	const int buf_size = 16384; //16KB
	unsigned char out[buf_size];

	strm.zalloc   = Z_NULL;
	strm.zfree    = Z_NULL;
	strm.opaque   = Z_NULL;
	strm.avail_in = 0;
	strm.next_in  = Z_NULL;

	ret = inflateInit2(&strm, MAX_WBITS+16);
	if(ret != Z_OK)
		return false;

	strm.avail_in = sz;
	strm.next_in = (Bytef*)data;
	do{
		unsigned int have;
		strm.avail_out = buf_size;
		strm.next_out = out;
		ret = inflate(&strm, Z_NO_FLUSH);
		if(ret!=Z_OK && ret!=Z_STREAM_END)
			break;
		
		have = buf_size-strm.avail_out;
		if(have){
			bytes->Write(out, have);
		}
		if(ret == Z_STREAM_END)
			break;
	}while(strm.avail_out == 0);
	inflateEnd(&strm);
	return ret==Z_STREAM_END;
}

//////////////////////////////////////////////////////////////////////////

CMyByteArray::CMyByteArray()
	: m_pData(0)
	, m_iDataSize(0)
	, m_iDataPos(0)
{

}

CMyByteArray::~CMyByteArray()
{
	if(m_pData){
		delete[] m_pData;
		m_pData = 0;
	}
}

void* CMyByteArray::GetData()
{
	return m_pData;
}

size_t CMyByteArray::GetSize()
{
	return GetSpaceUsed();
}

size_t CMyByteArray::Write( const void* data,size_t sz )
{
	if(sz > GetSpaceLeft()) ReallocateDataSpace(sz);
	memcpy((char*)m_pData+GetSpaceUsed(),data,sz);
	GetSpaceUsed() += sz;
	return sz;
}

bool CMyByteArray::MakeNull()
{
	int x = 0;
	Write(&x, sizeof(x));
	GetSpaceUsed() -= sizeof(x);
	return true;
}

bool CMyByteArray::Duplicate( void** ppData, bool bMakeNull /*= false*/ )
{
	int addition = bMakeNull ? sizeof(int) : 0;
	if(bMakeNull) MakeNull();
	char* p = new char[GetSpaceUsed()+addition];
	memcpy(p,GetData(),GetSize()+addition);
	*ppData = p;
	return true;
}

size_t CMyByteArray::GetGranularity()
{
	return (size_t)1<<20;
}

size_t CMyByteArray::GetSpaceLeft()
{
	//如果m_iDataPos == m_iDataSize
	//结果正确, 但貌似表达式错误?
	// -1 + 1 = 0, size_t 得不到-1, 0x~FF + 1,也得0
	return m_iDataSize-1 - m_iDataPos + 1;
}

size_t& CMyByteArray::GetSpaceUsed()
{
	//结束-开始+1
	//return m_iDataPos-1 -0 + 1;
	return m_iDataPos;
}

void CMyByteArray::ReallocateDataSpace( size_t szAddition )
{
	//只有此条件才会分配空间,也即调用此函数前必须检测空间剩余
	//assert(GetSpaceLeft()<szAddition);

	//计算除去剩余空间后还应增加的空间大小
	size_t left = szAddition - GetSpaceLeft();

	//按分配粒度计算块数及剩余
	size_t nBlocks = left / GetGranularity();
	size_t cbRemain = left - nBlocks*GetGranularity();
	if(cbRemain) ++nBlocks;

	//计算新空间并重新分配
	size_t newDataSize = m_iDataSize + nBlocks*GetGranularity();
	void*  pData = (void*)new char[newDataSize];

	//复制原来的数据到新空间
	//memcpy支持第3个参数传0吗?
	if(GetSpaceUsed()) memcpy(pData,m_pData,GetSpaceUsed());

	if(m_pData) delete[] m_pData;
	m_pData = pData;
	m_iDataSize = newDataSize;
}
