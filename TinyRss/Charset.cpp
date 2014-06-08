#include <windows.h>
#include <string>

#include "Charset.h"

std::string Ansi2Utf8(const char* a)
{
	int rv1,rv2;
	wchar_t* pw;
	char* pc;

	rv1 = MultiByteToWideChar(CP_ACP,0,a,-1,NULL,0);
	if(rv1 <= 0) return "";

	pw = new wchar_t[rv1*2];
	MultiByteToWideChar(CP_ACP,0,a,-1,pw,rv1);

	rv2 = WideCharToMultiByte(CP_UTF8,0,pw,-1,NULL,0,NULL,NULL);
	pc = new char[rv2];
	WideCharToMultiByte(CP_UTF8,0,pw,rv1,pc,rv2,NULL,NULL);
	
	std::string s(pc);
	delete[] pw;
	delete[] pc;
	return s;
}

std::string Utf82Ansi(const char* u)
{
	int rv1,rv2;
	char* pc;
	wchar_t* pw;

	rv1 = MultiByteToWideChar(CP_UTF8,0,u,-1,NULL,0);
	pw = new wchar_t[rv1*2];
	MultiByteToWideChar(CP_UTF8,0,u,-1,pw,rv1);

	rv2 = WideCharToMultiByte(CP_ACP,0,pw,-1,NULL,0,NULL,NULL);
	pc = new char[rv2];
	WideCharToMultiByte(CP_ACP,0,pw,-1,pc,rv2,NULL,NULL);

	std::string s(pc);
	delete[] pc;
	delete[] pw;
	return s;
}
